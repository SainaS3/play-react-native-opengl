#include "ViewerRenderer.hpp"
#include "ViewerMath.hpp"
#include <GLES2/gl2.h>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace viewer {
namespace {
struct Vec {
    float x, y, z;
};
struct Vertex {
    Vec position, normal;
};
Vec Sub(Vec a, Vec b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

// CPU mesh preparation stays separate from context-bound GLES uploads.
std::vector<Vertex> ParseObj(const std::string &source) {
    std::istringstream input(source);
    std::string line;
    std::vector<Vec> positions;
    std::vector<Vertex> vertices;
    while (std::getline(input, line)) {
        std::istringstream row(line.substr(0, line.find('#')));
        std::string type;
        row >> type;
        if (type == "v") {
            Vec p{};
            if (!(row >> p.x >> p.y >> p.z) || !std::isfinite(p.x) || !std::isfinite(p.y) ||
                !std::isfinite(p.z))
                throw std::runtime_error("Invalid OBJ position");
            positions.push_back(p);
        } else if (type == "f") {
            std::vector<size_t> face;
            std::string token;
            while (row >> token) {
                auto field = token.substr(0, token.find('/'));
                size_t parsed = 0;
                auto index = std::stoll(field, &parsed);
                if (parsed != field.size() || index == 0)
                    throw std::runtime_error("Invalid OBJ index");
                index = index > 0 ? index - 1 : static_cast<int64_t>(positions.size()) + index;
                if (index < 0 || static_cast<size_t>(index) >= positions.size())
                    throw std::runtime_error("OBJ index out of bounds");
                face.push_back(static_cast<size_t>(index));
            }
            if (face.size() < 3)
                throw std::runtime_error("OBJ face has fewer than three vertices");
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                auto a = positions[face[0]], b = positions[face[i]], c = positions[face[i + 1]];
                auto u = Sub(b, a), v = Sub(c, a);
                Vec n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
                float length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
                n = length > 1e-12f ? Vec{n.x / length, n.y / length, n.z / length} : Vec{0, 1, 0};
                vertices.insert(vertices.end(), {{a, n}, {b, n}, {c, n}});
            }
        }
    }
    if (vertices.empty() ||
        vertices.size() > static_cast<size_t>(std::numeric_limits<GLsizei>::max() / 2))
        throw std::runtime_error("Invalid mesh size");
    return vertices;
}

void NormalizeMesh(std::vector<Vertex> &vertices) {
    Vec low = vertices.front().position, high = low;
    for (auto const &v : vertices) {
        low = {std::min(low.x, v.position.x), std::min(low.y, v.position.y),
               std::min(low.z, v.position.z)};
        high = {std::max(high.x, v.position.x), std::max(high.y, v.position.y),
                std::max(high.z, v.position.z)};
    }
    Vec center{(low.x + high.x) * .5f, (low.y + high.y) * .5f, (low.z + high.z) * .5f};
    float scale = 2.4f / std::max({high.x - low.x, high.y - low.y, high.z - low.z, 1e-6f});
    for (auto &v : vertices) {
        auto p = Sub(v.position, center);
        v.position = {p.x * scale, p.y * scale, p.z * scale};
    }
}

std::vector<Vertex> BuildEdges(const std::vector<Vertex> &vertices) {
    std::vector<Vertex> edges;
    edges.reserve(vertices.size() * 2);
    for (size_t i = 0; i < vertices.size(); i += 3)
        edges.insert(edges.end(), {vertices[i], vertices[i + 1], vertices[i + 1], vertices[i + 2],
                                   vertices[i + 2], vertices[i]});
    return edges;
}
} // namespace

struct Renderer::Impl {
    GLuint program = 0, triangles = 0, lines = 0;
    GLsizei triangleCount = 0, lineCount = 0;
    float angle = 0, height = 0, velocity = 0;
    void Reset() {
        angle = height = velocity = 0;
    }
    GLuint Shader(GLenum type, const std::string &source) {
        auto text = source.c_str();
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[4096]{};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            glDeleteShader(shader);
            throw std::runtime_error(log);
        }
        return shader;
    }
    void Create(const std::string &vertexSource, const std::string &fragmentSource) {
        GLuint vert = Shader(GL_VERTEX_SHADER, vertexSource), frag = 0;
        try {
            frag = Shader(GL_FRAGMENT_SHADER, fragmentSource);
        } catch (...) {
            glDeleteShader(vert);
            throw;
        }
        program = glCreateProgram();
        glAttachShader(program, vert);
        glAttachShader(program, frag);
        glBindAttribLocation(program, 0, "a_position");
        glBindAttribLocation(program, 1, "a_normal");
        glLinkProgram(program);
        glDeleteShader(vert);
        glDeleteShader(frag);
        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[4096]{};
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            throw std::runtime_error(log);
        }
        glGenBuffers(1, &triangles);
        glGenBuffers(1, &lines);
    }
    void Load(const std::string &source) {
        auto vertices = ParseObj(source);
        NormalizeMesh(vertices);
        auto edges = BuildEdges(vertices);
        glBindBuffer(GL_ARRAY_BUFFER, triangles);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(),
                     GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, lines);
        glBufferData(GL_ARRAY_BUFFER, edges.size() * sizeof(Vertex), edges.data(), GL_STATIC_DRAW);
        triangleCount = static_cast<GLsizei>(vertices.size());
        lineCount = static_cast<GLsizei>(edges.size());
        Reset();
    }
    void Uniform(char const *name, float x, float y, float z, float w) {
        glUniform4f(glGetUniformLocation(program, name), x, y, z, w);
    }
    void Draw(int width, int viewHeight, float dt, const Settings &settings) {
        if (width <= 0 || viewHeight <= 0 || !program || !triangleCount)
            return;
        dt = std::isfinite(dt) ? std::max(0.f, std::min(.05f, dt)) : 0.f;
        if (settings.spinning)
            angle += dt * .6f;
        float acceleration = settings.flying ? .5f : 0;
        height += velocity * dt + .5f * acceleration * dt * dt;
        velocity += acceleration * dt;
        glViewport(0, 0, width, viewHeight);
        glClearColor(.067f, .106f, .161f, 1);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(program);
        float aspect = static_cast<float>(width) / viewHeight;
        float rotationX = std::isfinite(settings.rotationX) ? settings.rotationX : 0;
        float rotationY = std::isfinite(settings.rotationY) ? settings.rotationY : 0;
        auto rotation = ViewerMatrix4Multiply(ViewerMatrix4MakeXRotation(rotationX),
                                             ViewerMatrix4MakeYRotation(angle + rotationY));
        auto mv = ViewerMatrix4Multiply(ViewerMatrix4MakeTranslation(0, height, 0), rotation);
        auto projection =
            ViewerMatrix4MakeOrtho(-1.8f * aspect, 1.8f * aspect, -1.8f, 1.8f, -10, 10);
        auto mvp = ViewerMatrix4Multiply(projection, mv);
        glUniformMatrix4fv(glGetUniformLocation(program, "u_mvMatrix"), 1, GL_FALSE, mv.m);
        glUniformMatrix4fv(glGetUniformLocation(program, "u_mvpMatrix"), 1, GL_FALSE, mvp.m);
        float length = std::sqrt(.4f * .4f + .7f * .7f + 1);
        glUniform3f(glGetUniformLocation(program, "u_directionalLight.direction"), .4f / length,
                    .7f / length, 1 / length);
        glUniform3f(glGetUniformLocation(program, "u_directionalLight.halfplane"), 0, 0, 1);
        Uniform("u_directionalLight.ambientColor", .4f, .4f, .4f, 1);
        Uniform("u_directionalLight.diffuseColor", 1, 1, 1, 1);
        Uniform("u_directionalLight.specularColor", .3f, .3f, .3f, 1);
        Uniform("u_material.ambientFactor", 1, 1, 1, 1);
        Uniform("u_material.diffuseFactor", ((settings.color >> 16) & 255) / 255.f,
                ((settings.color >> 8) & 255) / 255.f, (settings.color & 255) / 255.f, 1);
        Uniform("u_material.specularFactor", .5f, .6f, .5f, 1);
        glUniform1f(glGetUniformLocation(program, "u_material.shininess"), 24);
        glBindBuffer(GL_ARRAY_BUFFER, settings.wireframe ? lines : triangles);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void *>(offsetof(Vertex, position)));
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void *>(offsetof(Vertex, normal)));
        glDrawArrays(settings.wireframe ? GL_LINES : GL_TRIANGLES, 0,
                     settings.wireframe ? lineCount : triangleCount);
    }
};
Renderer::Renderer() : impl(new Impl) {}
Renderer::~Renderer() = default;
void Renderer::createResources(const std::string &vertexSource, const std::string &fragmentSource) {
    impl->Create(vertexSource, fragmentSource);
}
void Renderer::loadModel(const std::string &name, const std::string &source) {
    if (name.size() < 5 || name.substr(name.size() - 4) != ".obj" ||
        name.find_first_of("/\\:") != std::string::npos)
        throw std::runtime_error("Invalid OBJ filename");
    impl->Load(source);
}
void Renderer::resetAnimation() {
    impl->Reset();
}
void Renderer::draw(int width, int height, float elapsedSeconds, const Settings &settings) {
    impl->Draw(width, height, elapsedSeconds, settings);
}
void Renderer::releaseResources() {
    if (impl->program)
        glDeleteProgram(impl->program);
    glDeleteBuffers(1, &impl->triangles);
    glDeleteBuffers(1, &impl->lines);
    abandonResources();
}
void Renderer::abandonResources() noexcept {
    impl->program = impl->triangles = impl->lines = 0;
    impl->triangleCount = impl->lineCount = 0;
}
int Renderer::triangleCount() const noexcept {
    return impl->triangleCount;
}
unsigned Renderer::parseColor(const std::string &color) {
    if (color.size() != 7 || color[0] != '#' ||
        color.find_first_not_of("0123456789abcdefABCDEF", 1) != std::string::npos)
        throw std::runtime_error("Invalid mesh color");
    return static_cast<unsigned>(std::stoul(color.substr(1), nullptr, 16));
}
} // namespace viewer
