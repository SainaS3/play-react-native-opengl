#include "../shared/renderer/ViewerRenderer.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <GLES2/gl2.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

static void Require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
static std::string Read(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    Require(bool(input), "Cannot read test resource");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
static size_t ReadMeshPixels() {
    std::vector<unsigned char> pixels(128 * 128 * 4);
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    Require(glGetError() == GL_NO_ERROR, "GLES draw/readback failed");
    size_t changed = 0;
    for (size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] > 25 || pixels[i + 1] > 35 || pixels[i + 2] > 50)
            ++changed;
    return changed;
}
int main(int argc, char **argv) {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    viewer::Renderer renderer;
    int result = 1;
    try {
        Require(argc == 2, "Pass repository root");
        std::filesystem::path resources = std::filesystem::path(argv[1]) / "shared/resources";
        auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
            eglGetProcAddress("eglGetPlatformDisplayEXT"));
        Require(getDisplay != nullptr, "ANGLE display extension missing");
#if defined(__APPLE__)
        constexpr EGLint backend = EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE;
#else
        constexpr EGLint backend = EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE;
#endif
        EGLint attributes[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, backend, EGL_NONE};
        // EXT takes void*, while Darwin's EGLNativeDisplayType is an integer.
        display = getDisplay(EGL_PLATFORM_ANGLE_ANGLE, nullptr, attributes);
        Require(display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr),
                "Cannot initialize ANGLE backend");
        EGLint configAttrs[] = {EGL_SURFACE_TYPE,
                                EGL_PBUFFER_BIT,
                                EGL_RENDERABLE_TYPE,
                                EGL_OPENGL_ES2_BIT,
                                EGL_RED_SIZE,
                                8,
                                EGL_GREEN_SIZE,
                                8,
                                EGL_BLUE_SIZE,
                                8,
                                EGL_ALPHA_SIZE,
                                8,
                                EGL_DEPTH_SIZE,
                                24,
                                EGL_NONE};
        EGLConfig config{};
        EGLint count = 0;
        Require(eglChooseConfig(display, configAttrs, &config, 1, &count) && count,
                "No pbuffer config");
        EGLint contextAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttrs);
        EGLint surfaceAttrs[] = {EGL_WIDTH, 128, EGL_HEIGHT, 128, EGL_NONE};
        surface = eglCreatePbufferSurface(display, config, surfaceAttrs);
        Require(context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE &&
                    eglMakeCurrent(display, surface, surface, context),
                "Cannot bind test context");
        std::cout << "Backend: " << glGetString(GL_RENDERER) << '\n';
        const auto version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        Require(version && std::string(version).find("OpenGL ES 2.") == 0,
                "Expected an OpenGL ES 2 context");
        std::cout << "Version: " << version << '\n';
        auto create = [&] {
            renderer.createResources(Read(resources / "shaders/sVertexLighting.vsh"),
                                     Read(resources / "shaders/sVertexLighting.fsh"));
        };
        create();
        viewer::Settings settings;
        settings.spinning = false;
        size_t models = 0;
        for (auto const &entry : std::filesystem::directory_iterator(resources / "models")) {
            if (entry.path().extension() != ".obj")
                continue;
            renderer.loadModel(entry.path().filename().string(), Read(entry.path()));
            renderer.draw(128, 128, 0, settings);
            auto pixels = ReadMeshPixels();
            Require(pixels > 0, "Model did not produce pixels");
            std::cout << entry.path().filename().string() << ": " << renderer.triangleCount()
                      << " vertices, " << pixels << " mesh pixels\n";
            ++models;
        }
        Require(models == 10, "Expected all ten bundled models");
        renderer.loadModel("cone.obj", Read(resources / "models/cone.obj"));
        renderer.draw(128, 128, 0, settings);
        auto solid = ReadMeshPixels();
        std::vector<unsigned char> initialFrame(128 * 128 * 4), rotatedFrame(initialFrame.size());
        glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, initialFrame.data());
        settings.rotationX = .7f;
        settings.rotationY = .4f;
        renderer.draw(128, 128, 0, settings);
        Require(ReadMeshPixels() > 0, "Manual rotation did not render");
        glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, rotatedFrame.data());
        Require(initialFrame != rotatedFrame, "Manual rotation did not change the frame");
        settings.rotationX = settings.rotationY = 0;
        renderer.draw(128, 128, 0, settings);
        glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, rotatedFrame.data());
        Require(initialFrame == rotatedFrame, "Clearing manual rotation did not restore the frame");
        settings.zoom = 1.25f;
        renderer.draw(128, 128, 0, settings);
        Require(ReadMeshPixels() > solid, "Zoom in did not increase mesh coverage");
        settings.zoom = .5f;
        renderer.draw(128, 128, 0, settings);
        auto zoomedOut = ReadMeshPixels();
        Require(zoomedOut > 0 && zoomedOut < solid, "Zoom out coverage invalid");
        settings.zoom = 0;
        renderer.draw(128, 128, 0, settings);
        Require(ReadMeshPixels() == zoomedOut, "Zoom lower limit was not enforced");
        settings.zoom = std::numeric_limits<float>::quiet_NaN();
        renderer.draw(128, 128, 0, settings);
        Require(ReadMeshPixels() == solid, "Invalid zoom did not fall back to normal");
        settings.zoom = 1;
        renderer.draw(128, 128, 0, settings);
        glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, rotatedFrame.data());
        Require(initialFrame == rotatedFrame, "Restoring zoom did not restore the frame");
        settings.wireframe = true;
        renderer.draw(128, 128, 0, settings);
        auto wire = ReadMeshPixels();
        Require(wire > 0 && wire < solid, "Wireframe coverage invalid");
        settings.wireframe = false;
        settings.spinning = true;
        settings.flying = true;
        for (int i = 0; i < 20; ++i)
            renderer.draw(128, 128, .05f, settings);
        Require(ReadMeshPixels() > 0, "Animation did not render");
        renderer.resetAnimation();
        renderer.draw(128, 128, 0, settings);
        Require(ReadMeshPixels() == solid, "Reset did not restore initial frame coverage");
        for (auto const &source :
             {"v 0 0 0\nf 0 1 1", "v 0 0 0\nf 1 2 3", "v 0 0 0\nf 1 1", "v nan 0 0\nf 1 1 1"}) {
            bool rejected = false;
            try {
                renderer.loadModel("invalid.obj", source);
            } catch (const std::exception &) {
                rejected = true;
            }
            Require(rejected, "Malformed OBJ accepted");
        }
        renderer.loadModel("relative.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3 -2 -1");
        Require(renderer.triangleCount() == 3, "Relative indices failed");
        Require(viewer::Renderer::parseColor("#12aBef") == 0x12abef, "Color parsing failed");
        bool rejected = false;
        try {
            viewer::Renderer::parseColor("#123xxz");
        } catch (const std::exception &) {
            rejected = true;
        }
        Require(rejected, "Invalid color accepted");
        renderer.releaseResources();
        Require(renderer.triangleCount() == 0, "Released handles retained");
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, context);
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttrs);
        Require(context != EGL_NO_CONTEXT && eglMakeCurrent(display, surface, surface, context),
                "Context recreation failed");
        create();
        renderer.loadModel("cone.obj", Read(resources / "models/cone.obj"));
        renderer.draw(128, 128, 0, settings);
        Require(ReadMeshPixels() == solid, "Resources did not survive recreation");
        // Simulate losing the context without a chance to delete its GL objects.
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
        renderer.abandonResources();
        Require(renderer.triangleCount() == 0, "Lost-context handles retained");
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttrs);
        Require(context != EGL_NO_CONTEXT && eglMakeCurrent(display, surface, surface, context),
                "Lost-context recreation failed");
        create();
        renderer.loadModel("cone.obj", Read(resources / "models/cone.obj"));
        renderer.draw(128, 128, 0, settings);
        Require(ReadMeshPixels() == solid, "Abandoned resources did not recreate");
        std::cout << "PASS: models, rotation, zoom, wireframe, animation/reset, validation, "
                     "release/abandon/context recreation\n";
        result = 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }
    if (context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE &&
        eglMakeCurrent(display, surface, surface, context))
        renderer.releaseResources();
    else
        renderer.abandonResources();
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface != EGL_NO_SURFACE)
            eglDestroySurface(display, surface);
        if (context != EGL_NO_CONTEXT)
            eglDestroyContext(display, context);
        eglTerminate(display);
    }
    return result;
}
