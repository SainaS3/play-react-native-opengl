#pragma once
#include <memory>
#include <string>
#include <cstdint>

namespace viewer {
struct Settings {
    float rotationX = 0, rotationY = 0; // Manual rotation in radians from React gestures.
    unsigned color = 0xe8b56b;
    bool spinning = true, flying = false, wireframe = false;
};
// Platform adapters bind their drawable before calling this shared GLES renderer.
// They own scheduling, asset access, context lifetime and presentation.
class Renderer {
  public:
    Renderer();
    ~Renderer(); // CPU ownership only; host must explicitly release GL resources.
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    void createResources(const std::string &vertexSource, const std::string &fragmentSource);
    void loadModel(const std::string &name, const std::string &source);
    void resetAnimation();
    void draw(int width, int height, float elapsedSeconds, const Settings &settings);
    void releaseResources();          // Requires the owning context to be current.
    void abandonResources() noexcept; // Context lost: no GL calls.
    int triangleCount() const noexcept;
    static unsigned parseColor(const std::string &color);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace viewer
