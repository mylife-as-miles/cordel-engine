// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/renderer.hpp"
#include <chrono>
#include <fstream>
#include <sstream>

namespace cordel {
double monotonic_seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
GL::GL() {
#define CORDEL_LOAD(name) \
    name=reinterpret_cast<decltype(name)>(SDL_GL_GetProcAddress("gl" #name)); \
    if(!name) throw std::runtime_error("Missing gl" #name);
    CORDEL_GL_FUNCTIONS(CORDEL_LOAD)
#undef CORDEL_LOAD
}
void GL::check(const char* operation) const {
    GLenum error=GetError();
    if(error!=GL_NO_ERROR) throw std::runtime_error(std::string(operation)+": GL error "+std::to_string(error));
}
GlObject::GlObject(GL& gl,Counters& c,ResourceKind kind,GLenum shader_type):gl_(&gl),kind_(kind) {
    switch(kind) {
        case ResourceKind::Program:id_=gl.CreateProgram();break;
        case ResourceKind::Shader:id_=gl.CreateShader(shader_type);break;
        case ResourceKind::Vao:gl.GenVertexArrays(1,&id_);break;
        case ResourceKind::Vbo:case ResourceKind::Ebo:gl.GenBuffers(1,&id_);break;
        default:throw std::invalid_argument("Invalid GL object kind");
    }
    if(!id_) throw std::runtime_error("GL object creation failed");
    ticket_=Ticket(c,kind);
}
void GlObject::reset() noexcept {
    if(!id_) return;
    switch(kind_) {
        case ResourceKind::Program:gl_->DeleteProgram(id_);break;
        case ResourceKind::Shader:gl_->DeleteShader(id_);break;
        case ResourceKind::Vao:gl_->DeleteVertexArrays(1,&id_);break;
        case ResourceKind::Vbo:case ResourceKind::Ebo:gl_->DeleteBuffers(1,&id_);break;
        default:break;
    }
    id_=0;ticket_.reset();
}
GlObject::~GlObject() { reset(); }
GlObject::GlObject(GlObject&& b) noexcept:
    gl_(b.gl_),kind_(b.kind_),id_(std::exchange(b.id_,0)),ticket_(std::move(b.ticket_)) {}
GlObject& GlObject::operator=(GlObject&& b) noexcept {
    if(this!=&b) { reset();gl_=b.gl_;kind_=b.kind_;id_=std::exchange(b.id_,0);ticket_=std::move(b.ticket_); }
    return *this;
}
static GlObject compile(GL& gl,Counters& c,GLenum type,const char* source) {
    GlObject shader(gl,c,ResourceKind::Shader,type);
    gl.ShaderSource(shader.id(),1,&source,nullptr);gl.CompileShader(shader.id());
    GLint ok=0;gl.GetShaderiv(shader.id(),GL_COMPILE_STATUS,&ok);
    if(!ok) {
        GLint size=0;gl.GetShaderiv(shader.id(),GL_INFO_LOG_LENGTH,&size);
        std::string log(static_cast<std::size_t>(std::max(size,1)),0);
        gl.GetShaderInfoLog(shader.id(),size,nullptr,log.data());
        throw std::runtime_error("Shader compile: "+log);
    }
    return shader;
}
static GlObject program(GL& gl,Counters& c) {
    // Same normal/color/light equation as Phase 1.1. Translation-only fixture
    // means normals already have world orientation. No import axis reflection.
    auto vertex=compile(gl,c,GL_VERTEX_SHADER,R"GLSL(#version 330 core
layout(location=0) in vec3 a_position;
layout(location=1) in vec3 a_normal;
uniform mat4 u_mvp;
out float light;
void main() {
    gl_Position=u_mvp*vec4(a_position,1.0);
    light=0.35+0.65*max(dot(normalize(a_normal),normalize(vec3(-0.4,0.8,0.5))),0.0);
}
)GLSL");
    auto fragment=compile(gl,c,GL_FRAGMENT_SHADER,R"GLSL(#version 330 core
in float light;
uniform vec4 u_color;
out vec4 color;
void main() { color=vec4(u_color.rgb*light,1.0); }
)GLSL");
    GlObject result(gl,c,ResourceKind::Program);
    gl.AttachShader(result.id(),vertex.id());gl.AttachShader(result.id(),fragment.id());
    gl.LinkProgram(result.id());
    GLint ok=0;gl.GetProgramiv(result.id(),GL_LINK_STATUS,&ok);
    if(!ok) {
        GLint size=0;gl.GetProgramiv(result.id(),GL_INFO_LOG_LENGTH,&size);
        std::string log(static_cast<std::size_t>(std::max(size,1)),0);
        gl.GetProgramInfoLog(result.id(),size,nullptr,log.data());
        throw std::runtime_error("Shader link: "+log);
    }
    gl.DetachShader(result.id(),vertex.id());gl.DetachShader(result.id(),fragment.id());
    gl.check("program creation");return result;
}
GpuMesh::GpuMesh(GL& gl,Counters& c,const Mesh& mesh):
    vao(gl,c,ResourceKind::Vao),vbo(gl,c,ResourceKind::Vbo),ebo(gl,c,ResourceKind::Ebo),
    model(mesh.model),color(mesh.color) {
    if(mesh.indices.size()>static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()))
        throw std::runtime_error("Too many indices");
    indices=static_cast<GLsizei>(mesh.indices.size());
    gl.BindVertexArray(vao.id());
    gl.BindBuffer(GL_ARRAY_BUFFER,vbo.id());
    gl.BufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(mesh.vertices.size()*sizeof(Vertex)),mesh.vertices.data(),GL_STATIC_DRAW);
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo.id());
    gl.BufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<GLsizeiptr>(mesh.indices.size()*sizeof(unsigned)),mesh.indices.data(),GL_STATIC_DRAW);
    gl.VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,normal)));
    gl.EnableVertexAttribArray(1);gl.BindVertexArray(0);
    gl.check("mesh upload");
}
GpuScene::GpuScene(GL& gl,Counters& c,SceneData data):
    ticket(c,ResourceKind::Scene),program(cordel::program(gl,c)),source(std::move(data)) {
    mvp_uniform=gl.GetUniformLocation(program.id(),"u_mvp");
    color_uniform=gl.GetUniformLocation(program.id(),"u_color");
    if(mvp_uniform<0||color_uniform<0) throw std::runtime_error("Missing shader uniform");
    for(const auto& mesh:source.meshes) meshes.emplace_back(gl,c,mesh);
}
Renderer::Renderer(Counters& c):counters_(c) {
    if(!(profile()&GL_CONTEXT_CORE_PROFILE_BIT)) throw std::runtime_error("Context is not core profile");
    if(depth_bits()<16) throw std::runtime_error("Context lacks depth buffer");
}
void Renderer::load(const std::filesystem::path& path) {
    if(scene_) throw std::logic_error("Scene already loaded");
    scene_=std::make_unique<GpuScene>(gl_,counters_,load_scene(path));
}
void Renderer::unload() {
    gl_.BindVertexArray(0);gl_.UseProgram(0);gl_.BindBuffer(GL_ARRAY_BUFFER,0);
    scene_.reset();gl_.check("scene unload");
}
const SceneData& Renderer::scene() const {
    if(!scene_) throw std::logic_error("No loaded scene");
    return scene_->source;
}
RenderTimes Renderer::render(const Camera& camera,int width,int height,bool depth,bool reverse) {
    if(!scene_) throw std::logic_error("No loaded scene");
    RenderTimes timing;
    double start=monotonic_seconds();
    Mat4 pv=multiply(camera.projection(width,height),camera.view());
    std::vector<std::array<float,16>> mvps(scene_->meshes.size());
    for(std::size_t i=0;i<scene_->meshes.size();++i) {
        Mat4 matrix=multiply(pv,scene_->meshes[i].model);
        // Explicit row-major CPU -> column-major GL upload boundary.
        for(int r=0;r<4;++r) for(int c=0;c<4;++c) mvps[i][c*4+r]=static_cast<float>(matrix[r*4+c]);
    }
    double prepared=monotonic_seconds();timing.prep_ms=(prepared-start)*1000;
    gl_.Viewport(0,0,width,height);
    gl_.ClearColor(24/255.f,33/255.f,43/255.f,1);
    gl_.Clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    if(depth) { gl_.Enable(GL_DEPTH_TEST);gl_.DepthFunc(GL_LEQUAL); } else gl_.Disable(GL_DEPTH_TEST);
    gl_.Disable(GL_CULL_FACE);gl_.Disable(GL_BLEND);gl_.Disable(GL_FRAMEBUFFER_SRGB);
    gl_.UseProgram(scene_->program.id());
    for(std::size_t n=0;n<scene_->meshes.size();++n) {
        auto i=reverse?scene_->meshes.size()-1-n:n;
        auto& mesh=scene_->meshes[i];
        gl_.UniformMatrix4fv(scene_->mvp_uniform,1,GL_FALSE,mvps[i].data());
        gl_.Uniform4fv(scene_->color_uniform,1,mesh.color.data());
        gl_.BindVertexArray(mesh.vao.id());
        gl_.DrawElements(GL_TRIANGLES,mesh.indices,GL_UNSIGNED_INT,nullptr);
    }
    gl_.BindVertexArray(0);
    timing.submit_ms=(monotonic_seconds()-prepared)*1000;
    gl_.check("scene draw");return timing;
}
std::vector<unsigned char> Renderer::read_pixels(int w,int h) {
    if(w<=0||h<=0) throw std::invalid_argument("Invalid pixel dimensions");
    std::vector<unsigned char> pixels(static_cast<std::size_t>(w)*static_cast<std::size_t>(h)*4);
    gl_.PixelStorei(GL_PACK_ALIGNMENT,1);
    gl_.ReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    gl_.check("pixel readback");return pixels;
}
void Renderer::save_ppm(const std::filesystem::path& path,int w,int h) {
    auto pixels=read_pixels(w,h);
    std::ofstream out(path,std::ios::binary);
    out<<"P6\n"<<w<<' '<<h<<"\n255\n";
    for(int y=h-1;y>=0;--y) for(int x=0;x<w;++x) {
        auto offset=(static_cast<std::size_t>(y)*static_cast<std::size_t>(w)+static_cast<std::size_t>(x))*4;
        out.write(reinterpret_cast<const char*>(pixels.data()+offset),3);
    }
    if(!out) throw std::runtime_error("PPM write failed");
}
std::string Renderer::version() const { return reinterpret_cast<const char*>(gl_.GetString(GL_VERSION)); }
std::string Renderer::gpu() const { return reinterpret_cast<const char*>(gl_.GetString(GL_RENDERER)); }
int Renderer::profile() const { GLint n=0;gl_.GetIntegerv(GL_CONTEXT_PROFILE_MASK,&n);return n; }
int Renderer::depth_bits() const {
    GLint n=0;
    gl_.GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_DEPTH,GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,&n);
    gl_.check("default framebuffer depth size");return n;
}
}
