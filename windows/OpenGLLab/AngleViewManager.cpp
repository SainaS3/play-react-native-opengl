#include "pch.h"
#include "AngleViewManager.h"
#include <windows.h>
#include <unknwn.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <GLES2/gl2.h>
#include <angle_windowsstore.h>
#include <DirectXMath.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <memory>
#include <limits>
#include <type_traits>
#include <functional>
#include <JSValue.h>
#include <ReactContext.h>
#include <cwctype>

using namespace winrt;
using namespace winrt::Microsoft::ReactNative;

using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::Foundation;
static_assert(std::is_same<EGLNativeWindowType, ::IInspectable*>::value, "ANGLE must use UWP native windows");

namespace {
struct Vec { float x, y, z; };
struct Vertex { Vec position, normal; };
Vec Sub(Vec a, Vec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
std::string ReadResource(std::wstring const& relative) {
    auto root = Windows::ApplicationModel::Package::Current().InstalledLocation().Path();
    std::ifstream stream(std::wstring(root)+L"\\resources\\"+relative, std::ios::binary);
    if (!stream) throw std::runtime_error("Missing packaged resource: "+to_string(relative));
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
void EglCheck(EGLBoolean ok, char const* operation) {
    if (!ok) { std::ostringstream error; error << operation << " failed, EGL error 0x" << std::hex << eglGetError(); throw std::runtime_error(error.str()); }
}
void Log(std::string const& text) noexcept {
    OutputDebugStringA(("ANGLE UWP: "+text+"\n").c_str());
    try {
        auto path=std::wstring(Windows::Storage::ApplicationData::Current().LocalFolder().Path())+L"\\renderer.log";
        std::ofstream file(path,std::ios::app); file<<text<<"\n";
    } catch (...) {}
}

// All EGL/GL work runs on the XAML UI thread. Loaded/Unloaded delimit ownership;
// CompositionTarget drives frames without JavaScript animation callbacks.
struct Scene : std::enable_shared_from_this<Scene> {
    weak_ref<SwapChainPanel> panel;
    std::function<void(std::string const&)> reportError;
    EGLDisplay display=EGL_NO_DISPLAY;
    EGLContext context=EGL_NO_CONTEXT;
    EGLSurface surface=EGL_NO_SURFACE;
    GLuint program=0, triangles=0, lines=0;
    GLsizei triangleCount=0, lineCount=0;
    std::string model="cone.obj", loadedModel;
    unsigned color=0xe8b56b;
    bool spinning=true, flying=false, wireframe=false, active=false, failed=false, suspended=false;
    float angle=0, height=0, velocity=0;
    int64_t resetToken=0;
    bool checkedFrame=false;
    event_token rendering{}, suspending{}, resuming{}, visibility{};
    std::chrono::steady_clock::time_point last{};
    ~Scene() { Stop(); }
    void Reset() { angle=height=velocity=0; last={}; }
    void Error(std::string const& message) {
        failed=true;
        Log(message);
        if (reportError) reportError(message);
    }
    GLuint Shader(GLenum type, std::wstring const& file) {
        auto source=ReadResource(L"shaders\\"+file); auto text=source.c_str();
        GLuint shader=glCreateShader(type); glShaderSource(shader,1,&text,nullptr); glCompileShader(shader);
        GLint ok=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
        if (!ok) { char log[4096]{}; glGetShaderInfoLog(shader,sizeof(log),nullptr,log); glDeleteShader(shader); throw std::runtime_error(log); }
        return shader;
    }
    void Init() {
        auto nativePanel=panel.get();
        if (!nativePanel) throw std::runtime_error("ANGLE view is detached");
        auto getDisplay=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (!getDisplay) throw std::runtime_error("eglGetPlatformDisplayEXT unavailable");
        EGLint attributes[]={EGL_PLATFORM_ANGLE_TYPE_ANGLE,EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,EGL_TRUE,EGL_NONE};
        display=getDisplay(EGL_PLATFORM_ANGLE_ANGLE,EGL_DEFAULT_DISPLAY,attributes);
        if (display==EGL_NO_DISPLAY) EglCheck(EGL_FALSE,"get display");
        EglCheck(eglInitialize(display,nullptr,nullptr),"eglInitialize D3D11");
        EglCheck(eglBindAPI(EGL_OPENGL_ES_API),"eglBindAPI");
        EGLint configAttributes[]={EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
        EGLConfig config{}; EGLint count=0;
        EglCheck(eglChooseConfig(display,configAttributes,&config,1,&count),"eglChooseConfig");
        if (!count) throw std::runtime_error("No GLES2 window configuration");
        EGLint contextAttributes[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
        context=eglCreateContext(display,config,EGL_NO_CONTEXT,contextAttributes);
        if (context==EGL_NO_CONTEXT) EglCheck(EGL_FALSE,"eglCreateContext");
        surface=eglCreateWindowSurface(display,config,reinterpret_cast<::IInspectable*>(get_abi(nativePanel)),nullptr);
        if (surface==EGL_NO_SURFACE) EglCheck(EGL_FALSE,"eglCreateWindowSurface SwapChainPanel");
        EglCheck(eglMakeCurrent(display,surface,surface,context),"eglMakeCurrent");
        EglCheck(eglSwapInterval(display,1),"eglSwapInterval");
        auto renderer=reinterpret_cast<char const*>(glGetString(GL_RENDERER));
        Log(std::string("Renderer: ")+(renderer?renderer:"unknown"));
        Log(std::string("GLES: ")+reinterpret_cast<char const*>(glGetString(GL_VERSION)));
        GLuint vert=Shader(GL_VERTEX_SHADER,L"sVertexLighting.vsh"), frag=0;
        try { frag=Shader(GL_FRAGMENT_SHADER,L"sVertexLighting.fsh"); } catch (...) { glDeleteShader(vert); throw; }
        program=glCreateProgram(); glAttachShader(program,vert); glAttachShader(program,frag);
        glBindAttribLocation(program,0,"a_position"); glBindAttribLocation(program,1,"a_normal");
        glLinkProgram(program); glDeleteShader(vert); glDeleteShader(frag);
        GLint linked=0; glGetProgramiv(program,GL_LINK_STATUS,&linked);
        if (!linked) { char log[4096]{}; glGetProgramInfoLog(program,sizeof(log),nullptr,log); throw std::runtime_error(log); }
        glGenBuffers(1,&triangles); glGenBuffers(1,&lines); LoadModel(); last={}; checkedFrame=false;
    }
    void Cleanup() noexcept {
        if (display!=EGL_NO_DISPLAY) {
            if (context!=EGL_NO_CONTEXT && surface!=EGL_NO_SURFACE && eglMakeCurrent(display,surface,surface,context)) {
                if (program) glDeleteProgram(program); glDeleteBuffers(1,&triangles); glDeleteBuffers(1,&lines);
            }
            eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
            if (surface!=EGL_NO_SURFACE) eglDestroySurface(display,surface);
            if (context!=EGL_NO_CONTEXT) eglDestroyContext(display,context);
            eglTerminate(display);
        }
        display=EGL_NO_DISPLAY; context=EGL_NO_CONTEXT; surface=EGL_NO_SURFACE;
        program=triangles=lines=0; triangleCount=lineCount=0; loadedModel.clear();
    }
    void LoadModel() {
        if (model==loadedModel) return;
        if (model.size()<5 || model.substr(model.size()-4)!=".obj" || model.find_first_of("/\\:")!=std::string::npos)
            throw std::runtime_error("Invalid OBJ filename");
        std::istringstream input(ReadResource(std::wstring(L"models\\")+std::wstring(to_hstring(model)))); std::string line;
        std::vector<Vec> positions; std::vector<Vertex> vertices;
        while (std::getline(input,line)) {
            std::istringstream row(line.substr(0,line.find('#'))); std::string type; row>>type;
            if (type=="v") {
                Vec p{}; if (!(row>>p.x>>p.y>>p.z) || !std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) throw std::runtime_error("Invalid OBJ position");
                positions.push_back(p);
            } else if (type=="f") {
                std::vector<size_t> face; std::string token;
                while (row>>token) {
                    auto field=token.substr(0,token.find('/')); size_t parsed=0; auto index=std::stoll(field,&parsed);
                    if (parsed!=field.size() || index==0) throw std::runtime_error("Invalid OBJ index");
                    index=index>0?index-1:static_cast<int64_t>(positions.size())+index;
                    if (index<0 || static_cast<size_t>(index)>=positions.size()) throw std::runtime_error("OBJ index out of bounds");
                    face.push_back(static_cast<size_t>(index));
                }
                if (face.size()<3) throw std::runtime_error("OBJ face has fewer than three vertices");
                for (size_t i=1;i+1<face.size();++i) {
                    auto a=positions[face[0]],b=positions[face[i]],c=positions[face[i+1]];
                    auto u=Sub(b,a),v=Sub(c,a); Vec n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
                    float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
                    n=length>1e-12f?Vec{n.x/length,n.y/length,n.z/length}:Vec{0,1,0};
                    vertices.insert(vertices.end(),{{a,n},{b,n},{c,n}});
                }
            }
        }
        if (vertices.empty() || vertices.size()>static_cast<size_t>(std::numeric_limits<GLsizei>::max()/2)) throw std::runtime_error("Invalid mesh size");
        Vec low=vertices.front().position, high=low;
        for (auto const& v:vertices) { low={std::min(low.x,v.position.x),std::min(low.y,v.position.y),std::min(low.z,v.position.z)}; high={std::max(high.x,v.position.x),std::max(high.y,v.position.y),std::max(high.z,v.position.z)}; }
        Vec center{(low.x+high.x)*.5f,(low.y+high.y)*.5f,(low.z+high.z)*.5f};
        float scale=2.4f/std::max({high.x-low.x,high.y-low.y,high.z-low.z,1e-6f});
        for (auto& v:vertices) { auto p=Sub(v.position,center); v.position={p.x*scale,p.y*scale,p.z*scale}; }
        std::vector<Vertex> edges; edges.reserve(vertices.size()*2);
        for (size_t i=0;i<vertices.size();i+=3) edges.insert(edges.end(),{vertices[i],vertices[i+1],vertices[i+1],vertices[i+2],vertices[i+2],vertices[i]});
        glBindBuffer(GL_ARRAY_BUFFER,triangles); glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER,lines); glBufferData(GL_ARRAY_BUFFER,edges.size()*sizeof(Vertex),edges.data(),GL_STATIC_DRAW);
        triangleCount=static_cast<GLsizei>(vertices.size()); lineCount=static_cast<GLsizei>(edges.size()); loadedModel=model; Reset();
        Log("Loaded "+model+": "+std::to_string(triangleCount)+" vertices");
    }
    void Uniform(char const* name,float x,float y,float z,float w) { glUniform4f(glGetUniformLocation(program,name),x,y,z,w); }
    void Frame() {
        auto nativePanel=panel.get();
        if (!nativePanel || failed || suspended || nativePanel.ActualWidth()<=0 || nativePanel.ActualHeight()<=0) { last={}; return; }
        try {
            if (display==EGL_NO_DISPLAY) Init();
            EglCheck(eglMakeCurrent(display,surface,surface,context),"bind frame"); LoadModel();
            EGLint width=0, viewHeight=0;
            EglCheck(eglQuerySurface(display,surface,EGL_WIDTH,&width),"surface width");
            EglCheck(eglQuerySurface(display,surface,EGL_HEIGHT,&viewHeight),"surface height");
            if (width<=0||viewHeight<=0) return;
            auto now=std::chrono::steady_clock::now(); float dt=last.time_since_epoch().count()?std::min(.05f,std::chrono::duration<float>(now-last).count()):0; last=now;
            if (spinning) angle+=dt*.6f;
            float acceleration=flying?.5f:0; height+=velocity*dt+.5f*acceleration*dt*dt; velocity+=acceleration*dt;
            glViewport(0,0,width,viewHeight); glClearColor(.067f,.106f,.161f,1); glEnable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); glUseProgram(program);
            using namespace DirectX;
            // DirectX stores row-vector matrices; the same memory is their column-vector transpose in GLES.
            auto mv=XMMatrixRotationY(angle)*XMMatrixTranslation(0,height,0);
            float aspect=static_cast<float>(width)/viewHeight;
            auto projection=XMMatrixOrthographicOffCenterRH(-1.8f*aspect,1.8f*aspect,-1.8f,1.8f,-10,10);
            XMFLOAT4X4 matrix; XMStoreFloat4x4(&matrix,mv); glUniformMatrix4fv(glGetUniformLocation(program,"u_mvMatrix"),1,GL_FALSE,&matrix._11);
            XMStoreFloat4x4(&matrix,mv*projection); glUniformMatrix4fv(glGetUniformLocation(program,"u_mvpMatrix"),1,GL_FALSE,&matrix._11);
            float length=std::sqrt(.4f*.4f+.7f*.7f+1);
            glUniform3f(glGetUniformLocation(program,"u_directionalLight.direction"),.4f/length,.7f/length,1/length);
            glUniform3f(glGetUniformLocation(program,"u_directionalLight.halfplane"),0,0,1);
            Uniform("u_directionalLight.ambientColor",.4f,.4f,.4f,1); Uniform("u_directionalLight.diffuseColor",1,1,1,1); Uniform("u_directionalLight.specularColor",.3f,.3f,.3f,1);
            Uniform("u_material.ambientFactor",1,1,1,1); Uniform("u_material.diffuseFactor",((color>>16)&255)/255.f,((color>>8)&255)/255.f,(color&255)/255.f,1);
            Uniform("u_material.specularFactor",.5f,.6f,.5f,1); glUniform1f(glGetUniformLocation(program,"u_material.shininess"),24);
            glBindBuffer(GL_ARRAY_BUFFER,wireframe?lines:triangles); glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,position)));
            glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,normal)));
            glDrawArrays(wireframe?GL_LINES:GL_TRIANGLES,0,wireframe?lineCount:triangleCount);
            if (!checkedFrame) {
                // Read a central tile once to establish actual drawing, beyond context/shader creation.
                auto tileWidth=std::min(width,64),tileHeight=std::min(viewHeight,64);
                std::vector<uint8_t> pixels(static_cast<size_t>(tileWidth)*tileHeight*4);
                glReadPixels((width-tileWidth)/2,(viewHeight-tileHeight)/2,tileWidth,tileHeight,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                size_t changed=0;
                for (size_t i=0;i<pixels.size();i+=4) if (pixels[i]>25||pixels[i+1]>35||pixels[i+2]>50) ++changed;
                Log("Readback: "+std::to_string(changed)+" pixels above background in central tile");
            }
            auto error=glGetError(); if (error!=GL_NO_ERROR) throw std::runtime_error("GLES draw error "+std::to_string(error));
            if (!eglSwapBuffers(display,surface)) {
                auto swapError=eglGetError(); Cleanup();
                if (swapError!=EGL_CONTEXT_LOST && swapError!=EGL_BAD_SURFACE && swapError!=EGL_BAD_ALLOC) throw std::runtime_error("eglSwapBuffers error "+std::to_string(swapError));
                Init(); // Recreate buffers and shaders after device loss.
            } else if (!checkedFrame) {
                checkedFrame=true; Log("First frame presented: "+std::to_string(width)+"x"+std::to_string(viewHeight)+", GL_NO_ERROR");
            }
        } catch (std::exception const& e) { Cleanup(); Error(e.what()); }
          catch (hresult_error const& e) { Cleanup(); Error(to_string(e.message())); }
    }
    void Start() {
        if (active) return; active=true; failed=false; auto weak=weak_from_this();
        rendering=Media::CompositionTarget::Rendering([weak](auto const&,auto const&) { if (auto self=weak.lock()) self->Frame(); });
        suspending=Windows::ApplicationModel::Core::CoreApplication::Suspending([weak](auto const&,auto const&) { if (auto self=weak.lock()) { self->suspended=true; self->Cleanup(); } });
        resuming=Windows::ApplicationModel::Core::CoreApplication::Resuming([weak](auto const&,auto const&) { if (auto self=weak.lock()) { self->suspended=false; self->last={}; } });
        visibility=Window::Current().VisibilityChanged([weak](auto const&,Windows::UI::Core::VisibilityChangedEventArgs const& e) { if (auto self=weak.lock()) { self->suspended=!e.Visible(); self->last={}; } });
    }
    void Stop() noexcept {
        if (active) {
            Media::CompositionTarget::Rendering(rendering);
            Windows::ApplicationModel::Core::CoreApplication::Suspending(suspending);
            Windows::ApplicationModel::Core::CoreApplication::Resuming(resuming);
            if (Window::Current()) Window::Current().VisibilityChanged(visibility);
            active=false;
        }
        Cleanup();
    }
};

struct SceneHolder : implements<SceneHolder,winrt::Windows::Foundation::IInspectable> {
    std::shared_ptr<Scene> scene;
};
DependencyProperty SceneProperty() {
    // React Native owns FrameworkElement.Tag. Store renderer state in a separate slot.
    static auto property=DependencyProperty::RegisterAttached(L"AngleScene",
        xaml_typename<winrt::Windows::Foundation::IInspectable>(), xaml_typename<SwapChainPanel>(),
        PropertyMetadata(winrt::Windows::Foundation::IInspectable{nullptr}));
    return property;
}
struct AngleViewManager : implements<AngleViewManager,IViewManager,
    IViewManagerWithNativeProperties,IViewManagerWithReactContext,
    IViewManagerWithExportedEventTypeConstants,IViewManagerWithDropViewInstance> {
    IReactContext reactContext{nullptr};
    hstring Name() const noexcept { return L"LegacyOpenGLView"; }
    IReactContext ReactContext() const noexcept { return reactContext; }
    void ReactContext(IReactContext const& value) noexcept { reactContext=value; }
    FrameworkElement CreateView() noexcept {
        SwapChainPanel panel;
        auto scene=std::make_shared<Scene>();
        scene->panel=make_weak(panel);
        auto weakScene=std::weak_ptr<Scene>(scene);
        auto weakPanel=make_weak(panel);
        winrt::Microsoft::ReactNative::ReactContext context(reactContext);
        scene->reportError=[weakPanel,context](std::string const& error) {
            if (auto view=weakPanel.get()) context.DispatchEvent(view,L"topError",JSValueObject{{"message",error}});
        };
        auto holder=make_self<SceneHolder>(); holder->scene=scene;
        panel.SetValue(SceneProperty(),holder.as<winrt::Windows::Foundation::IInspectable>());
        panel.Loaded([weakScene](auto const&,auto const&) { if (auto s=weakScene.lock()) s->Start(); });
        panel.Unloaded([weakScene](auto const&,auto const&) { if (auto s=weakScene.lock()) s->Stop(); });
        Log("React Native created LegacyOpenGLView");
        return panel;
    }
    Windows::Foundation::Collections::IMapView<hstring,ViewManagerPropertyType> NativeProps() const noexcept {
        return single_threaded_map<hstring,ViewManagerPropertyType>(std::map<hstring,ViewManagerPropertyType>{
            {L"model",ViewManagerPropertyType::String},{L"meshColor",ViewManagerPropertyType::String},
            {L"spinning",ViewManagerPropertyType::Boolean},{L"flying",ViewManagerPropertyType::Boolean},
            {L"wireframe",ViewManagerPropertyType::Boolean},{L"resetToken",ViewManagerPropertyType::Number}}).GetView();
    }
    void UpdateProperties(FrameworkElement const& view,IJSValueReader const& reader) noexcept {
        auto tag=view.GetValue(SceneProperty()); if (!tag) return;
        auto scene=get_self<SceneHolder>(tag)->scene;
        try {
            for (auto const& property:JSValue::ReadObjectFrom(reader)) {
                auto const& name=property.first; auto const& value=property.second;
                if (name=="model") { scene->model=value.IsNull()?"cone.obj":value.AsString(); scene->failed=false; }
                else if (name=="meshColor") {
                    auto color=value.IsNull()?std::string("#e8b56b"):value.AsString();
                    if (color.size()!=7 || color[0]!='#' || color.find_first_not_of("0123456789abcdefABCDEF",1)!=std::string::npos) throw std::runtime_error("Invalid mesh color");
                    scene->color=std::stoul(color.substr(1),nullptr,16);
                } else if (name=="spinning") scene->spinning=value.IsNull()?true:value.AsBoolean();
                else if (name=="flying") scene->flying=value.AsBoolean();
                else if (name=="wireframe") scene->wireframe=value.AsBoolean();
                else if (name=="resetToken" && scene->resetToken!=value.AsInt64()) { scene->resetToken=value.AsInt64(); scene->Reset(); }
            }
            Log("React settings: model="+scene->model+", spinning="+std::to_string(scene->spinning)+
                ", flying="+std::to_string(scene->flying)+", wireframe="+std::to_string(scene->wireframe)+
                ", reset="+std::to_string(scene->resetToken));
        } catch (std::exception const& e) { scene->Error(e.what()); }
          catch (hresult_error const& e) { scene->Error(to_string(e.message())); }
    }
    void OnDropViewInstance(FrameworkElement const& view) noexcept {
        if (auto tag=view.GetValue(SceneProperty())) { get_self<SceneHolder>(tag)->scene->Stop(); view.ClearValue(SceneProperty()); }
    }
    ConstantProviderDelegate ExportedCustomBubblingEventTypeConstants() const noexcept { return nullptr; }
    ConstantProviderDelegate ExportedCustomDirectEventTypeConstants() const noexcept {
        return [](IJSValueWriter const& writer) { WriteProperty(writer,L"topError",JSValueObject{{"registrationName","onError"}}); };
    }
};
}
IViewManager MakeAngleViewManager() { return make<AngleViewManager>(); }
void LogAngleHost(std::string const& message) noexcept { Log(message); }


