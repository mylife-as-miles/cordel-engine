// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/scene.hpp"
#include <SDL3/SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL3/SDL_opengl.h>
#include <memory>

namespace cordel {
// A small loader for exactly the core GL calls used by this spike. SDL supplies
// declarations; no GL development package or linked gl* symbol is required.
#define CORDEL_GL_FUNCTIONS(X) \
    X(ClearColor) X(Clear) X(Enable) X(Disable) X(DepthFunc) X(Viewport) X(Finish) \
    X(GetString) X(GetIntegerv) X(GetFramebufferAttachmentParameteriv) X(GetError) X(ReadPixels) X(PixelStorei) X(DrawElements) \
    X(GenVertexArrays) X(BindVertexArray) X(DeleteVertexArrays) \
    X(GenBuffers) X(BindBuffer) X(BufferData) X(DeleteBuffers) \
    X(VertexAttribPointer) X(EnableVertexAttribArray) \
    X(CreateShader) X(ShaderSource) X(CompileShader) X(GetShaderiv) X(GetShaderInfoLog) X(DeleteShader) \
    X(CreateProgram) X(AttachShader) X(DetachShader) X(LinkProgram) X(GetProgramiv) X(GetProgramInfoLog) X(DeleteProgram) \
    X(UseProgram) X(GetUniformLocation) X(UniformMatrix4fv) X(Uniform4fv)
struct GL {
#define CORDEL_GL_MEMBER(name) decltype(&::gl##name) name{};
    CORDEL_GL_FUNCTIONS(CORDEL_GL_MEMBER)
#undef CORDEL_GL_MEMBER
    GL();
    void check(const char* operation) const;
};
class GlObject {
    GL* gl_{};
    ResourceKind kind_{};
    GLuint id_{};
    Ticket ticket_;
    void reset() noexcept;
public:
    GlObject()=default;
    GlObject(GL& gl,Counters& counters,ResourceKind kind,GLenum shader_type=0);
    ~GlObject();
    GlObject(const GlObject&)=delete;
    GlObject& operator=(const GlObject&)=delete;
    GlObject(GlObject&& other) noexcept;
    GlObject& operator=(GlObject&& other) noexcept;
    GLuint id() const { return id_; }
};
struct GpuMesh {
    GlObject vao,vbo,ebo;
    Mat4 model;
    std::array<float,4> color;
    GLsizei indices{};
    GpuMesh(GL&,Counters&,const Mesh&);
};
struct GpuScene {
    Ticket ticket;
    GlObject program;
    std::vector<GpuMesh> meshes;
    SceneData source;
    GLint mvp_uniform{},color_uniform{};
    GpuScene(GL&,Counters&,SceneData);
};
struct RenderTimes { double prep_ms{},submit_ms{}; };
class Renderer {
    GL gl_;
    Counters& counters_;
    std::unique_ptr<GpuScene> scene_;
public:
    explicit Renderer(Counters& counters);
    void load(const std::filesystem::path& path);
    void unload();
    const SceneData& scene() const;
    RenderTimes render(const Camera&,int width,int height,bool depth=true,bool reverse=false);
    std::vector<unsigned char> read_pixels(int width,int height);
    void save_ppm(const std::filesystem::path&,int width,int height);
    std::string version() const;
    std::string gpu() const;
    int profile() const;
    int depth_bits() const;
    void check() const { gl_.check("renderer operation"); }
    // Synchronous diagnostic backpressure: offscreen EGL swap is not a queue
    // drain. Bounds submissions/memory; wall wait duration is NOT GPU timing.
    void complete_frame() const { gl_.Finish();gl_.check("frame completion"); }
};
double monotonic_seconds();
}
