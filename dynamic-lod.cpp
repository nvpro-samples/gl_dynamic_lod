/*-----------------------------------------------------------------------
  Copyright (c) 2014, NVIDIA. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Neither the name of its contributors may be used to endorse 
     or promote products derived from this software without specific
     prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------*/
/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#define DEBUG_FILTER 1


#include <nvgl/extensions_gl.hpp>

#include <imgui/backends/imgui_impl_gl.h>
#include <imgui/extras/imgui_helper.h>

#include <nvmath/nvmath_glsltypes.h>

#include <nvh/cameracontrol.hpp>
#include <nvh/geometry.hpp>
#include <nvh/misc.hpp>

#include <nvgl/appwindowprofiler_gl.hpp>
#include <nvgl/base_gl.hpp>
#include <nvgl/error_gl.hpp>
#include <nvgl/programmanager_gl.hpp>

#include "common.h"

namespace dynlod {
int const SAMPLE_SIZE_WIDTH(1024);
int const SAMPLE_SIZE_HEIGHT(768);
int const SAMPLE_MAJOR_VERSION(4);
int const SAMPLE_MINOR_VERSION(5);

class Frustum
{
public:
  enum
  {
    PLANE_NEAR,
    PLANE_FAR,
    PLANE_LEFT,
    PLANE_RIGHT,
    PLANE_TOP,
    PLANE_BOTTOM,
    NUM_PLANES
  };

  static inline void init(float planes[Frustum::NUM_PLANES][4], const float viewProj[16])
  {
    const float* clip = viewProj;

    planes[PLANE_RIGHT][0] = clip[3] - clip[0];
    planes[PLANE_RIGHT][1] = clip[7] - clip[4];
    planes[PLANE_RIGHT][2] = clip[11] - clip[8];
    planes[PLANE_RIGHT][3] = clip[15] - clip[12];

    planes[PLANE_LEFT][0] = clip[3] + clip[0];
    planes[PLANE_LEFT][1] = clip[7] + clip[4];
    planes[PLANE_LEFT][2] = clip[11] + clip[8];
    planes[PLANE_LEFT][3] = clip[15] + clip[12];

    planes[PLANE_BOTTOM][0] = clip[3] + clip[1];
    planes[PLANE_BOTTOM][1] = clip[7] + clip[5];
    planes[PLANE_BOTTOM][2] = clip[11] + clip[9];
    planes[PLANE_BOTTOM][3] = clip[15] + clip[13];

    planes[PLANE_TOP][0] = clip[3] - clip[1];
    planes[PLANE_TOP][1] = clip[7] - clip[5];
    planes[PLANE_TOP][2] = clip[11] - clip[9];
    planes[PLANE_TOP][3] = clip[15] - clip[13];

    planes[PLANE_FAR][0] = clip[3] - clip[2];
    planes[PLANE_FAR][1] = clip[7] - clip[6];
    planes[PLANE_FAR][2] = clip[11] - clip[10];
    planes[PLANE_FAR][3] = clip[15] - clip[14];

    planes[PLANE_NEAR][0] = clip[3] + clip[2];
    planes[PLANE_NEAR][1] = clip[7] + clip[6];
    planes[PLANE_NEAR][2] = clip[11] + clip[10];
    planes[PLANE_NEAR][3] = clip[15] + clip[14];

    for(int i = 0; i < NUM_PLANES; i++)
    {
      float length    = sqrtf(planes[i][0] * planes[i][0] + planes[i][1] * planes[i][1] + planes[i][2] * planes[i][2]);
      float magnitude = 1.0f / length;

      for(int n = 0; n < 4; n++)
      {
        planes[i][n] *= magnitude;
      }
    }
  }

  Frustum() {}
  Frustum(const float viewProj[16]) { init(m_planes, viewProj); }

  float m_planes[NUM_PLANES][4];
};

class Sample : public nvgl::AppWindowProfilerGL
{
  struct
  {
    nvgl::ProgramID draw_sphere_point, draw_sphere, draw_sphere_tess, lodcontent, lodcmds, lodcontent_comp, lodcmds_comp;
  } programs;

  struct
  {
    GLuint sphere_vbo      = 0;
    GLuint sphere_ibo      = 0;
    GLuint scene_ubo       = 0;
    GLuint particles       = 0;
    GLuint particleindices = 0;
    GLuint lodparticles0   = 0;
    GLuint lodparticles1   = 0;
    GLuint lodparticles2   = 0;
    GLuint lodcmds;
  } buffers;

  struct
  {
    GLuint particles    = 0;
    GLuint lodparticles = 0;
  } textures;

  struct Tweak
  {
    int   particleCount = 0xFFFFF;
    int   jobCount      = 1;
    float fov           = 60.0f;
    bool  pause         = false;
    bool  uselod        = true;
    bool  nolodtess     = false;
    bool  wireframe     = false;
    bool  useindices    = true;
    bool  usecompute    = true;
  };

  nvgl::ProgramManager m_progManager;

  ImGuiH::Registry m_ui;
  double           m_uiTime;

  Tweak m_tweak;
  Tweak m_lastTweak;

  GLuint    m_workGroupSize[3];
  SceneData m_sceneUbo;

  nvh::CameraControl m_control;

  bool begin();
  void processUI(double time);
  void think(double time);
  void resize(int width, int height);
  void drawLod();

  void updateProgramDefines();
  bool initProgram();
  bool initParticleBuffer();
  bool initLodBuffers();
  bool initScene();

  void end() { ImGui::ShutdownGL(); }
  // return true to prevent m_windowState updates
  bool mouse_pos(int x, int y) { return ImGuiH::mouse_pos(x, y); }
  bool mouse_button(int button, int action) { return ImGuiH::mouse_button(button, action); }
  bool mouse_wheel(int wheel) { return ImGuiH::mouse_wheel(wheel); }
  bool key_char(int button) { return ImGuiH::key_char(button); }
  bool key_button(int button, int action, int mods) { return ImGuiH::key_button(button, action, mods); }

public:
  Sample()
  {
    m_parameterList.add("jobcount", &m_tweak.jobCount);
    m_parameterList.add("particlecount", &m_tweak.particleCount);
    m_parameterList.add("uselod", &m_tweak.uselod);
    m_parameterList.add("usecompute", &m_tweak.usecompute);
    m_parameterList.add("useindices", &m_tweak.useindices);
    m_parameterList.add("nolodtess", &m_tweak.nolodtess);
    m_parameterList.add("fov", &m_tweak.fov);
  }
};

static size_t snapdiv(size_t input, size_t align)
{
  return (input + align - 1) / align;
}

static size_t snapsize(size_t input, size_t align)
{
  return ((input + align - 1) / align) * align;
}

void Sample::updateProgramDefines()
{
  m_progManager.m_prepend = std::string("");
  m_progManager.m_prepend += nvgl::ProgramManager::format("#define USE_INDICES %d\n", m_tweak.useindices ? 1 : 0);
}

bool Sample::initProgram()
{
  bool validated(true);
  m_progManager.m_filetype = nvh::ShaderFileManager::FILETYPE_GLSL;
  m_progManager.addDirectory(std::string("GLSL_" PROJECT_NAME));
  m_progManager.addDirectory(exePath() + std::string(PROJECT_RELDIRECTORY));

  m_progManager.registerInclude("common.h");

  updateProgramDefines();

  programs.draw_sphere_point =
      m_progManager.createProgram(nvgl::ProgramManager::Definition(GL_VERTEX_SHADER, "spherepoint.vert.glsl"),
                                  nvgl::ProgramManager::Definition(GL_FRAGMENT_SHADER, "spherepoint.frag.glsl"));
  programs.draw_sphere =
      m_progManager.createProgram(nvgl::ProgramManager::Definition(GL_VERTEX_SHADER, "sphere.vert.glsl"),
                                  nvgl::ProgramManager::Definition(GL_FRAGMENT_SHADER, "sphere.frag.glsl"));

  programs.draw_sphere_tess =
      m_progManager.createProgram(nvgl::ProgramManager::Definition(GL_VERTEX_SHADER, "spheretess.vert.glsl"),
                                  nvgl::ProgramManager::Definition(GL_TESS_CONTROL_SHADER, "spheretess.tctrl.glsl"),
                                  nvgl::ProgramManager::Definition(GL_TESS_EVALUATION_SHADER, "spheretess.teval.glsl"),
                                  nvgl::ProgramManager::Definition(GL_FRAGMENT_SHADER, "sphere.frag.glsl"));

  programs.lodcontent =
      m_progManager.createProgram(nvgl::ProgramManager::Definition(GL_VERTEX_SHADER, "lodcontent.vert.glsl"));

  programs.lodcmds =
      m_progManager.createProgram(nvgl::ProgramManager::Definition(GL_VERTEX_SHADER, "lodcmds.vert.glsl"));

  programs.lodcontent_comp = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_COMPUTE_SHADER, "#define USE_COMPUTE 1\n", "lodcontent.vert.glsl"));

  programs.lodcmds_comp = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_COMPUTE_SHADER, "#define USE_COMPUTE 1\n", "lodcmds.vert.glsl"));

  validated = m_progManager.areProgramsValid();

  if(validated)
  {
    glGetProgramiv(m_progManager.get(programs.lodcontent_comp), GL_COMPUTE_WORK_GROUP_SIZE, (GLint*)m_workGroupSize);
  }

  return validated;
}

bool Sample::initScene()
{
  {
    // Sphere VBO/IBO
    const int Faces[] = {2, 1,  0, 3, 2,  0, 4,  3,  0,  5, 4, 0, 1, 5, 0, 11, 6, 7, 11, 7,
                         8, 11, 8, 9, 11, 9, 10, 11, 10, 6, 1, 2, 6, 2, 3, 7,  3, 4, 8,  4,
                         5, 9,  5, 1, 10, 2, 7,  6,  3,  8, 7, 4, 9, 8, 5, 10, 9, 1, 6,  10};

    const float Verts[] = {0.000f,  0.000f,  1.000f,  1.0f,   0.894f,  0.000f, 0.447f,  1.0f,    0.276f,  0.851f,
                           0.447f,  1.0f,    -0.724f, 0.526f, 0.447f,  1.0f,   -0.724f, -0.526f, 0.447f,  1.0f,
                           0.276f,  -0.851f, 0.447f,  1.0f,   0.724f,  0.526f, -0.447f, 1.0f,    -0.276f, 0.851f,
                           -0.447f, 1.0f,    -0.894f, 0.000f, -0.447f, 1.0f,   -0.276f, -0.851f, -0.447f, 1.0f,
                           0.724f,  -0.526f, -0.447f, 1.0f,   0.000f,  0.000f, -1.000f, 1.0f};

    int IndexCount  = sizeof(Faces) / sizeof(Faces[0]);
    int VertexCount = sizeof(Verts) / sizeof(Verts[0]);

    assert(IndexCount / 3 == PARTICLE_BASICPRIMS);
    assert(VertexCount / 4 == PARTICLE_BASICVERTICES);

    nvh::geometry::Mesh<vec4> icosahedron;
    icosahedron.m_vertices.resize(VertexCount / 4);
    memcpy(&icosahedron.m_vertices[0], Verts, sizeof(Verts));
    icosahedron.m_indicesTriangles.resize(IndexCount / 3);
    memcpy(&icosahedron.m_indicesTriangles[0], Faces, sizeof(Faces));

    icosahedron.flipWinding();

    nvh::geometry::Mesh<vec4> batched;
    for(int i = 0; i < PARTICLE_BATCHSIZE; i++)
    {
      batched.append(icosahedron);
    }

    nvgl::newBuffer(buffers.sphere_ibo);
    glNamedBufferData(buffers.sphere_ibo, batched.getTriangleIndicesSize(), &batched.m_indicesTriangles[0], GL_STATIC_DRAW);

    nvgl::newBuffer(buffers.sphere_vbo);
    glNamedBufferData(buffers.sphere_vbo, batched.getVerticesSize(), &batched.m_vertices[0], GL_STATIC_DRAW);

#if USE_COMPACT_PARTICLE
    glVertexAttribFormat(VERTEX_POS, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribFormat(VERTEX_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(Particle, posColor.w));
#else
    glVertexAttribFormat(VERTEX_POS, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribFormat(VERTEX_COLOR, 4, GL_FLOAT, GL_FALSE, offsetof(Particle, color));
#endif
    glVertexAttribBinding(VERTEX_POS, 0);
    glVertexAttribBinding(VERTEX_COLOR, 0);
  }


  {  // Scene UBO
    nvgl::newBuffer(buffers.scene_ubo);
    glNamedBufferData(buffers.scene_ubo, sizeof(SceneData), NULL, GL_DYNAMIC_DRAW);
  }

  return true;
}
bool Sample::initParticleBuffer()
{
  {
    std::vector<Particle> particles(m_tweak.particleCount);
    std::vector<int>      particleindices(m_tweak.particleCount);

    int cube = 1;
    while(cube * cube * (cube / 4) < m_tweak.particleCount)
    {
      cube++;
    }

    float scale             = 128.0f / float(cube);
    m_sceneUbo.particleSize = scale * 0.375f;

    srand(47345356);

    for(int i = 0; i < m_tweak.particleCount; i++)
    {
      int x = i % cube;
      int z = (i / cube) % (cube);
      int y = i / (cube * cube);

      vec3 pos = (vec3(0, nvh::frand(), 0) - 0.5f) * 0.1f;
      pos += vec3(x, y, z);
      pos -= vec3(cube, cube / 4, cube) * 0.5f;
      pos *= vec3(1, 4, 1);
      float size = (1.0f + nvh::frand() * 1.0f) * 0.25f;

      vec4 color = vec4(nvh::frand(), nvh::frand(), nvh::frand(), 1.0f);
#if USE_COMPACT_PARTICLE
      union
      {
        GLubyte color[4];
        float   rawFloat;
      } packed;
      packed.color[0] = GLubyte(color.x * 255.0);
      packed.color[1] = GLubyte(color.y * 255.0);
      packed.color[2] = GLubyte(color.z * 255.0);
      packed.color[3] = GLubyte(color.w * 255.0);

      particles[i].posColor = vec4(pos * scale, packed.rawFloat);
#else
      particles[i].posSize = vec4(pos, size) * scale;
      particles[i].color   = color;
#endif
      particleindices[i] = i;
    }

    nvgl::newBuffer(buffers.particles);
    glNamedBufferData(buffers.particles, sizeof(Particle) * m_tweak.particleCount, &particles[0], GL_STATIC_DRAW);

    nvgl::newBuffer(buffers.particleindices);
    glNamedBufferData(buffers.particleindices, sizeof(int) * m_tweak.particleCount, &particleindices[0], GL_STATIC_DRAW);

    nvgl::newTexture(textures.particles, GL_TEXTURE_BUFFER);
    glTextureBuffer(textures.particles, GL_RGBA32F, buffers.particles);

    GLint maxtexels = 1;
    GLint texels    = m_tweak.particleCount * (sizeof(Particle) / sizeof(vec4));
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxtexels);
    if(texels > maxtexels)
    {
      LOGI("\nWARNING: buffer size too big for texturebuffer: %d max %d\n", texels, maxtexels);
    }
  }

  return true;
}

bool Sample::initLodBuffers()
{
  size_t itemSize;
  GLenum itemFormat;
  int    itemTexels;

  if(m_tweak.useindices)
  {
    itemSize   = sizeof(int);
    itemFormat = GL_R32I;
    itemTexels = 1;
  }
  else
  {
    itemSize   = sizeof(Particle);
    itemFormat = GL_RGBA32F;
    itemTexels = sizeof(Particle) / sizeof(vec4);
  }
  //size_t size   = snapsize(itemSize * tweak.particleCount, 256);
  size_t size = snapsize(itemSize * (m_tweak.particleCount / m_tweak.jobCount), 256);

  GLint maxtexels = 1;
  GLint texels    = int(size / itemSize) * itemTexels;
  glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxtexels);
  if(texels > maxtexels)
  {
    LOGI("\nWARNING: buffer size too big for texturebuffer: %d max %d\n", texels, maxtexels);
  }

  nvgl::newBuffer(buffers.lodparticles0);
  glNamedBufferData(buffers.lodparticles0, size, NULL, GL_DYNAMIC_COPY);
  nvgl::newBuffer(buffers.lodparticles1);
  glNamedBufferData(buffers.lodparticles1, size, NULL, GL_DYNAMIC_COPY);
  nvgl::newBuffer(buffers.lodparticles2);
  glNamedBufferData(buffers.lodparticles2, size, NULL, GL_DYNAMIC_COPY);

  nvgl::newTexture(textures.lodparticles, GL_TEXTURE_BUFFER);
  glTextureBuffer(textures.lodparticles, itemFormat, buffers.lodparticles0);

  nvgl::newBuffer(buffers.lodcmds);
  glNamedBufferData(buffers.lodcmds, snapsize(sizeof(DrawIndirects), 256) * m_tweak.jobCount, NULL, GL_DYNAMIC_COPY);
  glClearNamedBufferData(buffers.lodcmds, GL_RGBA32F, GL_RGBA, GL_FLOAT, NULL);

  return true;
}

bool Sample::begin()
{
  ImGuiH::Init(m_windowState.m_winSize[0], m_windowState.m_winSize[1], this);
  ImGui::InitGL();

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);

  bool validated(true);

  GLuint defaultVAO;
  glGenVertexArrays(1, &defaultVAO);
  glBindVertexArray(defaultVAO);

  validated = validated && initProgram();
  validated = validated && initScene();
  validated = validated && initParticleBuffer();
  validated = validated && initLodBuffers();

  m_sceneUbo.nearPixels = 10.0f;
  m_sceneUbo.farPixels  = 1.5f;
  m_sceneUbo.tessPixels = 10.0f;

  m_control.m_sceneOrbit     = vec3(0.0f);
  m_control.m_sceneDimension = 256.0f;
  m_control.m_viewMatrix = nvmath::look_at(m_control.m_sceneOrbit + vec3(0.9, 0.9, 1) * m_control.m_sceneDimension * 0.3f,
                                           m_control.m_sceneOrbit, vec3(0, 1, 0));

  return validated;
}

void Sample::processUI(double time)
{
  int width  = m_windowState.m_winSize[0];
  int height = m_windowState.m_winSize[1];

  // Update imgui configuration
  auto& imgui_io       = ImGui::GetIO();
  imgui_io.DeltaTime   = static_cast<float>(time - m_uiTime);
  imgui_io.DisplaySize = ImVec2(width, height);

  m_uiTime = time;

  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImVec2(350, 0), ImGuiCond_FirstUseEver);
  if(ImGui::Begin("NVIDIA " PROJECT_NAME, nullptr))
  {
    ImGui::Checkbox("use lod", &m_tweak.uselod);
    ImGui::Checkbox("use tess (if no lod)", &m_tweak.nolodtess);
    ImGui::Checkbox("wireframe", &m_tweak.wireframe);
    ImGui::Checkbox("use indexing", &m_tweak.useindices);
    ImGui::Checkbox("use compute", &m_tweak.usecompute);
    ImGui::Checkbox("pause lod", &m_tweak.pause);
    ImGuiH::InputIntClamped("num partices", &m_tweak.particleCount, 1, 1024 * 1024 * 1024, 1024 * 512, 1024 * 1024,
                            ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::Separator();
    ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.585f);
    ImGui::DragFloat("lod near pixelsize", &m_sceneUbo.nearPixels, 0.1f, 1, 1000);
    ImGui::DragFloat("lod far pixelsize", &m_sceneUbo.farPixels, 0.1f, 1, 1000);
    ImGui::DragFloat("tess pixelsize", &m_sceneUbo.tessPixels, 0.1f, 1, 1000);
    ImGui::Separator();
    ImGui::SliderFloat("fov", &m_tweak.fov, 1, 90.0f);
    ImGui::PopItemWidth();
  }
  ImGui::End();
}

void Sample::drawLod()
{
  NV_PROFILE_GL_SPLIT();

  // due to SSBO alignment (256 bytes) we need to calculate some counts
  // dynamically
  size_t itemSize;
  GLenum itemFormat;

  if(m_tweak.useindices)
  {
    itemSize   = sizeof(int);
    itemFormat = GL_R32I;
  }
  else
  {
    itemSize   = sizeof(Particle);
    itemFormat = GL_RGBA32F;
  }

  size_t jobSize   = snapsize(sizeof(DrawIndirects), 256);
  int    jobCount  = (int)(snapsize(itemSize * (m_tweak.particleCount / m_tweak.jobCount), 256) / itemSize);
  size_t jobOffset = snapsize(itemSize * m_tweak.particleCount, 256);
  int    jobs      = (int)snapdiv(m_tweak.particleCount, jobCount);
  int    jobRest   = m_tweak.particleCount - (jobs - 1) * jobCount;

  int offset = 0;
  for(int i = 0; i < jobs; i++)
  {
    int cnt = i == jobs - 1 ? jobRest : jobCount;

    if(!m_tweak.pause || jobs > 1)
    {
      NV_PROFILE_GL_SECTION("Lod");
      glEnable(GL_RASTERIZER_DISCARD);

      {
        NV_PROFILE_GL_SECTION("Cont");

        glUseProgram(m_progManager.get(m_tweak.usecompute ? programs.lodcontent_comp : programs.lodcontent));

        if(m_tweak.usecompute)
        {
          glUniform1i(UNI_CONTENT_IDX_MAX, offset + cnt);
          nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLES, GL_TEXTURE_BUFFER, textures.particles);
        }
        else
        {
          glEnableVertexAttribArray(VERTEX_POS);
          glEnableVertexAttribArray(VERTEX_COLOR);

          glBindVertexBuffer(0, buffers.particles, sizeof(Particle) * offset, sizeof(Particle));
        }

        glUniform1i(UNI_CONTENT_IDX_OFFSET, offset);

        glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, ABO_DATA_COUNTS, buffers.lodcmds, jobSize * i, sizeof(DrawCounters));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBO_DATA_POINTS, buffers.lodparticles0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBO_DATA_BASIC, buffers.lodparticles1);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBO_DATA_TESS, buffers.lodparticles2);

        if(m_tweak.usecompute)
        {
          GLuint numGroups = (cnt + m_workGroupSize[0] - 1) / m_workGroupSize[0];

          glDispatchCompute(numGroups, 1, 1);
        }
        else
        {
          glDrawArrays(GL_POINTS, 0, cnt);

          glDisableVertexAttribArray(VERTEX_POS);
          glDisableVertexAttribArray(VERTEX_COLOR);
        }
      }

      {
        NV_PROFILE_GL_SECTION("Cmds");

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        glUseProgram(m_progManager.get(m_tweak.usecompute ? programs.lodcmds_comp : programs.lodcmds));

        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, SSBO_DATA_INDIRECTS, buffers.lodcmds, jobSize * i, sizeof(DrawIndirects));
        if(m_tweak.usecompute)
        {
          glDispatchCompute(1, 1, 1);
        }
        else
        {
          glDrawArrays(GL_POINTS, 0, 1);
        }

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT
                        | GL_COMMAND_BARRIER_BIT);
      }

      glDisable(GL_RASTERIZER_DISCARD);
    }

    {
      NV_PROFILE_GL_SECTION("Draw");
      // the following drawcalls all source the amount of works from drawindirect buffers
      // generated above
      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffers.lodcmds);
      //glEnable(GL_RASTERIZER_DISCARD);
      {
        NV_PROFILE_GL_SECTION("Tess");

        glUseProgram(m_progManager.get(programs.draw_sphere_tess));
        glPatchParameteri(GL_PATCH_VERTICES, 3);

        glBindVertexBuffer(0, buffers.sphere_vbo, 0, sizeof(vec4));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers.sphere_ibo);
        glEnableVertexAttribArray(VERTEX_POS);

        if(m_tweak.useindices)
        {
          nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLES, GL_TEXTURE_BUFFER, textures.particles);
          nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLEINDICES, GL_TEXTURE_BUFFER, textures.lodparticles);
        }
        else
        {
          nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLES, GL_TEXTURE_BUFFER, textures.lodparticles);
          nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLEINDICES, GL_TEXTURE_BUFFER, 0);
        }

        glTextureBuffer(textures.lodparticles, itemFormat, buffers.lodparticles2);

        glBindBufferRange(GL_UNIFORM_BUFFER, UBO_CMDS, buffers.lodcmds, (i * jobSize), jobSize);

        glUniform1i(UNI_USE_CMDOFFSET, 0);
        glDrawElementsIndirect(GL_PATCHES, GL_UNSIGNED_INT, NV_BUFFER_OFFSET(offsetof(DrawIndirects, nearFull) + (i * jobSize)));

        glUniform1i(UNI_USE_CMDOFFSET, 1);
        glDrawElementsIndirect(GL_PATCHES, GL_UNSIGNED_INT, NV_BUFFER_OFFSET(offsetof(DrawIndirects, nearRest) + (i * jobSize)));

        glDisableVertexAttribArray(VERTEX_POS);
        glBindVertexBuffer(0, 0, 0, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindBufferBase(GL_UNIFORM_BUFFER, UBO_CMDS, 0);
      }

      {
        NV_PROFILE_GL_SECTION("Mesh");

        glUseProgram(m_progManager.get(programs.draw_sphere));

        glBindVertexBuffer(0, buffers.sphere_vbo, 0, sizeof(vec4));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers.sphere_ibo);
        glEnableVertexAttribArray(VERTEX_POS);

        if(m_tweak.useindices)
        {
          nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLES, GL_TEXTURE_BUFFER, textures.particles);
          nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLEINDICES, GL_TEXTURE_BUFFER, textures.lodparticles);
        }
        else
        {
          nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLES, GL_TEXTURE_BUFFER, textures.lodparticles);
          nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLEINDICES, GL_TEXTURE_BUFFER, 0);
        }

        glTextureBuffer(textures.lodparticles, itemFormat, buffers.lodparticles1);

        glBindBufferRange(GL_UNIFORM_BUFFER, UBO_CMDS, buffers.lodcmds, (i * jobSize), jobSize);

        glUniform1i(UNI_USE_CMDOFFSET, 0);
        glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, NV_BUFFER_OFFSET(offsetof(DrawIndirects, medFull) + (i * jobSize)));

        glUniform1i(UNI_USE_CMDOFFSET, 1);
        glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, NV_BUFFER_OFFSET(offsetof(DrawIndirects, medRest) + (i * jobSize)));

        glDisableVertexAttribArray(VERTEX_POS);
        glBindVertexBuffer(0, 0, 0, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindBufferBase(GL_UNIFORM_BUFFER, UBO_CMDS, 0);
      }

      {
        NV_PROFILE_GL_SECTION("Pnts");

        glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

        glUseProgram(m_progManager.get(programs.draw_sphere_point));

        if(m_tweak.useindices)
        {
          nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLES, GL_TEXTURE_BUFFER, textures.particles);
          nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLEINDICES, GL_TEXTURE_BUFFER, textures.lodparticles);
        }
        else
        {
          glEnableVertexAttribArray(VERTEX_POS);
          glEnableVertexAttribArray(VERTEX_COLOR);
        }

        if(m_tweak.useindices)
        {
          glTextureBuffer(textures.lodparticles, itemFormat, buffers.lodparticles0);
          glDrawArraysIndirect(GL_POINTS, NV_BUFFER_OFFSET(offsetof(DrawIndirects, farArray) + (i * jobSize)));
        }
        else
        {
          glBindVertexBuffer(0, buffers.lodparticles0, 0, (GLsizei)itemSize);
          glDrawArraysIndirect(GL_POINTS, NV_BUFFER_OFFSET(offsetof(DrawIndirects, farArray) + (i * jobSize)));
        }

        if(!m_tweak.useindices)
        {
          glDisableVertexAttribArray(VERTEX_POS);
          glDisableVertexAttribArray(VERTEX_COLOR);
        }

        glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);

        glBindVertexBuffer(0, 0, 0, 0);
      }

      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
      //glDisable(GL_RASTERIZER_DISCARD);
    }

    offset += cnt;
  }

  NV_PROFILE_GL_SPLIT();
}

void Sample::think(double time)
{
  NV_PROFILE_GL_SECTION("Frame");

  processUI(time);

  m_control.processActions(m_windowState.m_winSize,
                           nvmath::vec2f(m_windowState.m_mouseCurrent[0], m_windowState.m_mouseCurrent[1]),
                           m_windowState.m_mouseButtonFlags, m_windowState.m_mouseWheel);

  m_tweak.jobCount = std::min(m_tweak.particleCount, m_tweak.jobCount);

  if(m_lastTweak.useindices != m_tweak.useindices)
  {
    updateProgramDefines();
    m_progManager.reloadPrograms();
  }

  if(m_lastTweak.particleCount != m_tweak.particleCount)
  {
    initParticleBuffer();
    initLodBuffers();
  }

  if(m_lastTweak.jobCount != m_tweak.jobCount || m_lastTweak.useindices != m_tweak.useindices)
  {
    initLodBuffers();
  }

  if(m_windowState.onPress(KEY_R))
  {
    m_progManager.reloadPrograms();
    glGetProgramiv(m_progManager.get(programs.lodcontent_comp), GL_COMPUTE_WORK_GROUP_SIZE, (GLint*)m_workGroupSize);
  }
  if(!m_progManager.areProgramsValid())
  {
    waitEvents();
    return;
  }

  int width  = m_windowState.m_winSize[0];
  int height = m_windowState.m_winSize[1];

  glViewport(0, 0, width, height);

  glClearColor(0.1f, 0.1f, 0.1f, 0.0f);
  glClearDepth(1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  {  // Update UBO
    glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SCENE, buffers.scene_ubo);

    m_sceneUbo.viewport = uvec2(width, height);

    float farplane = 1000.0f;

    nvmath::mat4 projection = nvmath::perspective((m_tweak.fov), float(width) / float(height), 0.1f, farplane);
    nvmath::mat4 view       = m_control.m_viewMatrix;

    vec4 hPos                = projection * nvmath::vec4(1.0f, 1.0f, -1000.0f, 1.0f);
    vec2 hCoord              = vec2(hPos.x / hPos.w, hPos.y / hPos.w);
    vec2 dim                 = nvmath::nv_abs(hCoord);
    m_sceneUbo.viewpixelsize = dim * vec2(float(width), float(height)) * farplane * 0.5f;

    m_sceneUbo.viewProjMatrix = projection * view;
    m_sceneUbo.viewMatrix     = view;
    m_sceneUbo.viewMatrixIT   = nvmath::transpose(nvmath::invert(view));

    Frustum::init((float(*)[4]) & m_sceneUbo.frustum[0].x, m_sceneUbo.viewProjMatrix.mat_array);

    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneData), &m_sceneUbo);
  }

  glPolygonMode(GL_FRONT_AND_BACK, m_tweak.wireframe ? GL_LINE : GL_FILL);

  if(m_tweak.uselod)
  {
    drawLod();
  }
  else
  {
    NV_PROFILE_GL_SECTION("NoLod");

    bool useTess = m_tweak.nolodtess;

    glUseProgram(m_progManager.get(useTess ? programs.draw_sphere_tess : programs.draw_sphere));
    glPatchParameteri(GL_PATCH_VERTICES, 3);

    glBindVertexBuffer(0, buffers.sphere_vbo, 0, sizeof(vec4));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers.sphere_ibo);

    glEnableVertexAttribArray(VERTEX_POS);

    int fullCnt = m_tweak.particleCount / PARTICLE_BATCHSIZE;
    int restCnt = m_tweak.particleCount % PARTICLE_BATCHSIZE;

    GLenum prim = useTess ? GL_PATCHES : GL_TRIANGLES;
    GLenum itemFormat;
    int    itemSize;
    GLuint itemBuffer;

    if(m_tweak.useindices)
    {
      itemFormat = GL_R32I;
      itemSize   = sizeof(uint);
      itemBuffer = buffers.particleindices;

      nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLES, GL_TEXTURE_BUFFER, textures.particles);
      nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLEINDICES, GL_TEXTURE_BUFFER, textures.lodparticles);
    }
    else
    {
      itemFormat = GL_RGBA32F;
      itemSize   = sizeof(Particle);
      itemBuffer = buffers.particles;

      nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLES, GL_TEXTURE_BUFFER, textures.lodparticles);
      nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_PARTICLEINDICES, GL_TEXTURE_BUFFER, 0);
    }

    glTextureBuffer(textures.lodparticles, itemFormat, itemBuffer);
    glDrawElementsInstanced(prim, PARTICLE_BATCHSIZE * PARTICLE_BASICINDICES, GL_UNSIGNED_INT, 0, fullCnt);

    if(restCnt)
    {
      glTextureBufferRange(textures.lodparticles, itemFormat, itemBuffer, itemSize * fullCnt * PARTICLE_BATCHSIZE, restCnt * itemSize);
      glDrawElementsInstanced(prim, restCnt * PARTICLE_BASICINDICES, GL_UNSIGNED_INT, 0, 1);
    }

    glDisableVertexAttribArray(VERTEX_POS);

    glBindVertexBuffer(0, 0, 0, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  }

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SCENE, 0);

  {
    NV_PROFILE_GL_SECTION("GUI");
    ImGui::Render();
    ImGui::RenderDrawDataGL(ImGui::GetDrawData());
  }

  ImGui::EndFrame();

  m_lastTweak = m_tweak;
}

void Sample::resize(int width, int height) {}

}  // namespace dynlod

using namespace dynlod;

int main(int argc, const char** argv)
{
  NVPSystem system(PROJECT_NAME);

  Sample sample;
  return sample.run(PROJECT_NAME, argc, argv, SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT);
}
