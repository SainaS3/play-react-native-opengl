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
#include "../../native/shared/ViewerRenderer.h"
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
    viewer::Renderer renderer;
    viewer::Settings settings;
    std::string model="cone.obj", loadedModel;
    bool active=false, failed=false, suspended=false;
    int64_t resetToken=0;
    bool checkedFrame=false;
    event_token rendering{}, suspending{}, resuming{}, visibility{};
    std::chrono::steady_clock::time_point last{};
    ~Scene() { Stop(); }
    void Reset() { renderer.resetAnimation(); last={}; }
    void Error(std::string const& message) {
        failed=true;
        Log(message);
        if (reportError) reportError(message);
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
        auto backend=reinterpret_cast<char const*>(glGetString(GL_RENDERER));
        Log(std::string("Renderer: ")+(backend?backend:"unknown"));
        Log(std::string("GLES: ")+reinterpret_cast<char const*>(glGetString(GL_VERSION)));
        renderer.createResources(ReadResource(L"shaders\\sVertexLighting.vsh"),ReadResource(L"shaders\\sVertexLighting.fsh"));
        LoadModel(); last={}; checkedFrame=false;
    }
    void Cleanup() noexcept {
        if (display!=EGL_NO_DISPLAY) {
            if (context!=EGL_NO_CONTEXT && surface!=EGL_NO_SURFACE && eglMakeCurrent(display,surface,surface,context)) {
                renderer.releaseResources();
            }
            eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
            if (surface!=EGL_NO_SURFACE) eglDestroySurface(display,surface);
            if (context!=EGL_NO_CONTEXT) eglDestroyContext(display,context);
            eglTerminate(display);
        }
        display=EGL_NO_DISPLAY; context=EGL_NO_CONTEXT; surface=EGL_NO_SURFACE;
        renderer.abandonResources(); loadedModel.clear();
    }
    void LoadModel() {
        if (model==loadedModel) return;
        // Validate before accessing a packaged path.
        if (model.size()<5 || model.substr(model.size()-4)!=".obj" || model.find_first_of("/\\:")!=std::string::npos)
            throw std::runtime_error("Invalid OBJ filename");
        renderer.loadModel(model,ReadResource(std::wstring(L"models\\")+std::wstring(to_hstring(model))));
        loadedModel=model; last={};
        Log("Loaded "+model+": "+std::to_string(renderer.triangleCount())+" vertices");
    }
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
            renderer.draw(width,viewHeight,dt,settings);
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
                    scene->settings.color=viewer::Renderer::parseColor(color);
                } else if (name=="spinning") scene->settings.spinning=value.IsNull()?true:value.AsBoolean();
                else if (name=="flying") scene->settings.flying=value.AsBoolean();
                else if (name=="wireframe") scene->settings.wireframe=value.AsBoolean();
                else if (name=="resetToken" && scene->resetToken!=value.AsInt64()) { scene->resetToken=value.AsInt64(); scene->Reset(); }
            }
            Log("React settings: model="+scene->model+", spinning="+std::to_string(scene->settings.spinning)+
                ", flying="+std::to_string(scene->settings.flying)+", wireframe="+std::to_string(scene->settings.wireframe)+
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


