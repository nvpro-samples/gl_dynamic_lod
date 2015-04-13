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

#define DEBUG_FILTER     1

#if 0
#undef GLEW_USER_ASSERT
#define GLEW_USER_ASSERT(fn)  \
  if (!fn) { int* blah = 0; *blah = 1; }
#endif

#include <GL/glew.h>
#include <nv_helpers/anttweakbar.hpp>
#include <nv_helpers_gl/WindowProfiler.hpp>
#include <nv_math/nv_math_glsltypes.h>

#include <nv_helpers_gl/error.hpp>
#include <nv_helpers_gl/programmanager.hpp>
#include <nv_helpers/geometry.hpp>
#include <nv_helpers/misc.hpp>
#include <nv_helpers_gl/glresources.hpp>
#include <nv_helpers/cameracontrol.hpp>



using namespace nv_helpers;
using namespace nv_helpers_gl;
using namespace nv_math;
#include "common.h"

namespace dynlod
{
  int const SAMPLE_SIZE_WIDTH(1024);
  int const SAMPLE_SIZE_HEIGHT(768);
  int const SAMPLE_MAJOR_VERSION(4);
  int const SAMPLE_MINOR_VERSION(3);

  class Sample : public nv_helpers_gl::WindowProfiler
  {
    ProgramManager progManager;

    struct {
      ProgramManager::ProgramID
        draw_sphere_point,
        draw_sphere,
        draw_sphere_tess,
        lodcontent,
        lodcmds,
        lodcontent_comp,
        lodcmds_comp;
    } programs;

    struct {
      ResourceGLuint  
        sphere_vbo,
        sphere_ibo,
        scene_ubo,
        particles,
        particleindices,
        lodparticles0,
        lodparticles1,
        lodparticles2,
        lodcmds;
    } buffers;

    struct {
      ResourceGLuint
        particles,
        lodparticles;
    } textures;

    struct Tweak {
      Tweak() 
        : particleCount(0xFFFFF)
        , jobCount(1)
        , pause(false)
        , uselod(true)
        , nolodtess(false)
        , wireframe(false)
        , useindices(true)
        , usecompute(true)
        , fov(60.0f)
      {}

      int       particleCount;
      int       jobCount;
      float     fov;
      bool      pause;
      bool      uselod;
      bool      nolodtess;
      bool      wireframe;
      bool      useindices;
      bool      usecompute;
    };

    Tweak    tweak;
    Tweak    lastTweak;

    GLuint    workGroupSize[3];
    SceneData sceneUbo;


    bool  begin();
    void  think(double time);
    void  resize(int width, int height);
    void  drawLod();

    void updateProgramDefines();
    bool initProgram();
    bool initParticleBuffer();
    bool initLodBuffers();
    bool initScene();

    CameraControl m_control;

    void end() {
      TwTerminate();
    }
    // return true to prevent m_window updates
    bool mouse_pos    (int x, int y) {
      return !!TwEventMousePosGLFW(x,y); 
    }
    bool mouse_button (int button, int action) {
      return !!TwEventMouseButtonGLFW(button, action);
    }
    bool mouse_wheel  (int wheel) {
      return !!TwEventMouseWheelGLFW(wheel); 
    }
    bool key_button   (int button, int action, int mods) {
      return handleTwKeyPressed(button,action,mods);
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
    progManager.m_prepend = std::string("");
    progManager.m_prepend += ProgramManager::format("#define USE_INDICES %d\n", tweak.useindices ? 1 : 0);
  }

  bool Sample::initProgram()
  {
    bool validated(true);
    progManager.addDirectory( std::string(PROJECT_NAME));
    progManager.addDirectory( sysExePath() + std::string(PROJECT_RELDIRECTORY));
    progManager.addDirectory( std::string(PROJECT_ABSDIRECTORY));

    progManager.registerInclude("common.h", "common.h");

    updateProgramDefines();

    programs.draw_sphere_point = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,   "spherepoint.vert.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER, "spherepoint.frag.glsl"));
    programs.draw_sphere = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,   "sphere.vert.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER, "sphere.frag.glsl"));

    programs.draw_sphere_tess = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,          "spheretess.vert.glsl"),
      ProgramManager::Definition(GL_TESS_CONTROL_SHADER,    "spheretess.tctrl.glsl"),
      ProgramManager::Definition(GL_TESS_EVALUATION_SHADER, "spheretess.teval.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER,        "sphere.frag.glsl"));

    programs.lodcontent = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,  "lodcontent.vert.glsl"));

    programs.lodcmds = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,  "lodcmds.vert.glsl"));

    programs.lodcontent_comp = progManager.createProgram(
      ProgramManager::Definition(GL_COMPUTE_SHADER,  "#define USE_COMPUTE 1\n","lodcontent.vert.glsl"));

    programs.lodcmds_comp = progManager.createProgram(
      ProgramManager::Definition(GL_COMPUTE_SHADER,  "#define USE_COMPUTE 1\n","lodcmds.vert.glsl"));

    validated = progManager.areProgramsValid();

    if (validated){
      glGetProgramiv(progManager.get(programs.lodcontent_comp),GL_COMPUTE_WORK_GROUP_SIZE,(GLint*)workGroupSize);
    }

    return validated;
  }

  bool Sample::initScene()
  {
    {
      // Sphere VBO/IBO
      const int Faces[] = {
          2, 1, 0,
          3, 2, 0,
          4, 3, 0,
          5, 4, 0,
          1, 5, 0,
          11, 6,  7,
          11, 7,  8,
          11, 8,  9,
          11, 9,  10,
          11, 10, 6,
          1, 2, 6,
          2, 3, 7,
          3, 4, 8,
          4, 5, 9,
          5, 1, 10,
          2,  7, 6,
          3,  8, 7,
          4,  9, 8,
          5, 10, 9,
          1, 6, 10 };
    
      const float Verts[] = {
           0.000f,  0.000f,  1.000f, 1.0f,
           0.894f,  0.000f,  0.447f, 1.0f,
           0.276f,  0.851f,  0.447f, 1.0f,
          -0.724f,  0.526f,  0.447f, 1.0f,
          -0.724f, -0.526f,  0.447f, 1.0f,
           0.276f, -0.851f,  0.447f, 1.0f,
           0.724f,  0.526f, -0.447f, 1.0f,
          -0.276f,  0.851f, -0.447f, 1.0f,
          -0.894f,  0.000f, -0.447f, 1.0f,
          -0.276f, -0.851f, -0.447f, 1.0f,
           0.724f, -0.526f, -0.447f, 1.0f,
           0.000f,  0.000f, -1.000f, 1.0f };
    
      int IndexCount  = sizeof(Faces) / sizeof(Faces[0]);
      int VertexCount = sizeof(Verts) / sizeof(Verts[0]);

      assert(IndexCount/3  == PARTICLE_BASICPRIMS);
      assert(VertexCount/4 == PARTICLE_BASICVERTICES);

      geometry::Mesh<vec4> icosahedron;
      icosahedron.m_vertices.resize( VertexCount/4 );
      memcpy(&icosahedron.m_vertices[0], Verts, sizeof(Verts));
      icosahedron.m_indicesTriangles.resize( IndexCount/3 );
      memcpy(&icosahedron.m_indicesTriangles[0], Faces, sizeof(Faces));

      icosahedron.flipWinding();

      geometry::Mesh<vec4> batched;
      for (int i = 0; i < PARTICLE_BATCHSIZE; i++){
        batched.append(icosahedron);
      }

      newBuffer(buffers.sphere_ibo);
      glNamedBufferDataEXT(buffers.sphere_ibo, batched.getTriangleIndicesSize(), &batched.m_indicesTriangles[0], GL_STATIC_DRAW);

      newBuffer(buffers.sphere_vbo);
      glNamedBufferDataEXT(buffers.sphere_vbo, batched.getVerticesSize(), &batched.m_vertices[0], GL_STATIC_DRAW);

      glVertexAttribFormat(VERTEX_POS,  4,GL_FLOAT,GL_FALSE,0);
      glVertexAttribFormat(VERTEX_COLOR,4,GL_FLOAT,GL_FALSE,offsetof(Particle,color));
      glVertexAttribBinding(VERTEX_POS,   0);
      glVertexAttribBinding(VERTEX_COLOR, 0);
    }


    { // Scene UBO
      newBuffer(buffers.scene_ubo);
      glNamedBufferDataEXT(buffers.scene_ubo, sizeof(SceneData), NULL, GL_DYNAMIC_DRAW);
    }

    return true;
  }
  bool Sample::initParticleBuffer()
  {
    {
      std::vector<Particle> particles(tweak.particleCount);
      std::vector<int>      particleindices(tweak.particleCount);

      int cube = 1;
      while (cube * cube * (cube/4) < tweak.particleCount){
        cube++;
      }

      float scale = 128.0f/float(cube);

      srand(47345356);

      for (int i = 0; i < tweak.particleCount; i++)
      {
        int x = i % cube;
        int z = (i / cube) % (cube);
        int y = i / (cube*cube);

        vec3 pos = (vec3(0,frand(),0) - 0.5f) * 0.1f;
        pos += vec3(x,y,z);
        pos -= vec3(cube,cube/4,cube) * 0.5f;
        pos *= vec3(1,4,1);
        float size = (1.0f + frand()*1.0f) * 0.25f;

        particles[i].posSize  = vec4(pos,size) * scale;
        particles[i].color    = vec4(frand(),frand(),frand(),1.0f);
        particleindices[i]    = i;
      }

      newBuffer(buffers.particles);
      glNamedBufferDataEXT(buffers.particles, sizeof(Particle) * tweak.particleCount, &particles[0], GL_STATIC_DRAW);

      newBuffer(buffers.particleindices);
      glNamedBufferDataEXT(buffers.particleindices, sizeof(int) * tweak.particleCount, &particleindices[0], GL_STATIC_DRAW);

      newTexture(textures.particles);
      glTextureBufferEXT(textures.particles,GL_TEXTURE_BUFFER,GL_RGBA32F, buffers.particles);

      GLint maxtexels = 1;
      GLint texels    = tweak.particleCount * 2;
      glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE_ARB, &maxtexels );
      if ( texels > maxtexels ){
        printf("\nWARNING: buffer size too big for texturebuffer: %d max %d\n", texels, maxtexels);
      }
    }

    return true;
  }

  bool Sample::initLodBuffers()
  {
    size_t  itemSize;
    GLenum  itemFormat;
    int     itemTexels;

    if (tweak.useindices){
      itemSize    = sizeof(int);
      itemFormat  = GL_R32I;
      itemTexels  = 1;
    }
    else{
      itemSize    = sizeof(Particle);
      itemFormat  = GL_RGBA32F;
      itemTexels  = 2;
    }
    //size_t size   = snapsize(itemSize * tweak.particleCount, 256);
    size_t size  = snapsize(itemSize * (tweak.particleCount / tweak.jobCount), 256);

    GLint maxtexels = 1;
    GLint texels    = int(size / itemSize) * itemTexels;
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE_ARB, &maxtexels );
    if ( texels > maxtexels ){
      printf("\nWARNING: buffer size too big for texturebuffer: %d max %d\n", texels, maxtexels);
    }

    newBuffer(buffers.lodparticles0);
    glNamedBufferDataEXT(buffers.lodparticles0, size, NULL, GL_DYNAMIC_COPY);
    newBuffer(buffers.lodparticles1);
    glNamedBufferDataEXT(buffers.lodparticles1, size, NULL, GL_DYNAMIC_COPY);
    newBuffer(buffers.lodparticles2);
    glNamedBufferDataEXT(buffers.lodparticles2, size, NULL, GL_DYNAMIC_COPY);
    
    newTexture(textures.lodparticles);
    glTextureBufferEXT(textures.lodparticles,GL_TEXTURE_BUFFER, itemFormat, buffers.lodparticles0);

    newBuffer(buffers.lodcmds);
    glNamedBufferDataEXT(buffers.lodcmds, snapsize(sizeof(DrawIndirects),256) * tweak.jobCount, NULL, GL_DYNAMIC_COPY);
    glClearNamedBufferDataEXT(buffers.lodcmds,GL_RGBA32F,GL_RGBA, GL_FLOAT, NULL);

    return true;
  }

  bool Sample::begin()
  {
    TwInit(TW_OPENGL_CORE,NULL);
    TwWindowSize(m_window.m_viewsize[0],m_window.m_viewsize[1]);

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

    TwBar *bar = TwNewBar("mainbar");
    TwDefine(" GLOBAL contained=true help='OpenGL samples.\nCopyright NVIDIA Corporation 2013-2014' ");
    TwDefine(" mainbar position='0 0' size='250 250' color='0 0 0' alpha=128 ");
    TwDefine((std::string(" mainbar label='") + PROJECT_NAME + "'").c_str());

    TwAddVarRW(bar, "uselod",   TW_TYPE_BOOLCPP, &tweak.uselod, " label='use lod' ");
    TwAddVarRW(bar, "nolodtess",   TW_TYPE_BOOLCPP, &tweak.nolodtess, " label='use tess (if no lod)' ");
    TwAddVarRW(bar, "wireframe",   TW_TYPE_BOOLCPP, &tweak.wireframe, " label='wireframe' ");
    TwAddVarRW(bar, "indices",   TW_TYPE_BOOLCPP, &tweak.useindices, " label='use indexing' ");
    TwAddVarRW(bar, "compute",   TW_TYPE_BOOLCPP, &tweak.usecompute, " label='use compute' ");
    TwAddVarRW(bar, "pause",   TW_TYPE_BOOLCPP, &tweak.pause, " label='pause lod' ");
    TwAddVarRW(bar, "count",  TW_TYPE_INT32, &tweak.particleCount, " label='num particles' min=1 ");
    TwAddVarRW(bar, "jobs",   TW_TYPE_INT32, &tweak.jobCount, " label='num jobs' min=1 ");
    TwAddSeparator(bar,NULL,NULL);
    TwAddVarRW(bar, "near",   TW_TYPE_FLOAT, &sceneUbo.nearPixels, " label='lod near pixelsize' min=1 max=1000");
    TwAddVarRW(bar, "far",    TW_TYPE_FLOAT, &sceneUbo.farPixels, " label='lod far pixelsize' min=1 max=1000");
    TwAddVarRW(bar, "tess",   TW_TYPE_FLOAT, &sceneUbo.tessPixels, " label='tess pixelsize' min=1");
    TwAddSeparator(bar,NULL,NULL);
    TwAddVarRW(bar, "fov",   TW_TYPE_FLOAT, &tweak.fov, " label='fov degrees' min=1 max=90");

    sceneUbo.nearPixels = 10.0f;
    sceneUbo.farPixels  = 1.5f;
    sceneUbo.tessPixels   = 10.0f;

    m_control.m_sceneOrbit = vec3(0.0f);
    m_control.m_sceneDimension = 256.0f;
    m_control.m_viewMatrix = nv_math::look_at(m_control.m_sceneOrbit + vec3(0.9,0.9,1)*m_control.m_sceneDimension*0.3f, m_control.m_sceneOrbit, vec3(0,1,0));

    return validated;
  }

  void Sample::drawLod()
  {
    NV_PROFILE_SPLIT();

    // due to SSBO alignment (256 bytes) we need to calculate some counts
    // dynamically
    size_t itemSize;
    GLenum itemFormat;

    if (tweak.useindices){
      itemSize    = sizeof(int);
      itemFormat  = GL_R32I;
    }
    else{
      itemSize    = sizeof(Particle);
      itemFormat  = GL_RGBA32F;
    }

    size_t jobSize    = snapsize(sizeof(DrawIndirects),256);
    int jobCount      = (int)(snapsize(itemSize * (tweak.particleCount / tweak.jobCount), 256) / itemSize);
    size_t jobOffset  = snapsize(itemSize * tweak.particleCount, 256);
    int jobs          = (int)snapdiv(tweak.particleCount, jobCount);
    int jobRest       = tweak.particleCount - (jobs - 1) * jobCount;

    int offset = 0;
    for (int i = 0; i < jobs; i++){
      int cnt = i == jobs-1 ? jobRest : jobCount;

      if (!tweak.pause || jobs > 1)
      {
        NV_PROFILE_SECTION("Lod");
        glEnable(GL_RASTERIZER_DISCARD);

        {
          NV_PROFILE_SECTION("Cont");

          glUseProgram(progManager.get(tweak.usecompute ? programs.lodcontent_comp : programs.lodcontent));

          if (tweak.usecompute){
            glUniform1i(UNI_CONTENT_IDX_MAX, offset + cnt);
            glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLES,        GL_TEXTURE_BUFFER, textures.particles);
          }
          else{
            glEnableVertexAttribArray(VERTEX_POS);
            glEnableVertexAttribArray(VERTEX_COLOR);

            glBindVertexBuffer(0, buffers.particles,sizeof(Particle) * offset,sizeof(Particle));
          }

          glUniform1i(UNI_CONTENT_IDX_OFFSET, offset);

          glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, ABO_DATA_COUNTS, 
            buffers.lodcmds, jobSize * i, sizeof(DrawCounters));
          glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBO_DATA_POINTS,
            buffers.lodparticles0);
          glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBO_DATA_BASIC, 
            buffers.lodparticles1);
          glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBO_DATA_TESS,
            buffers.lodparticles2);

          if (tweak.usecompute){
            GLuint numGroups = (cnt+workGroupSize[0]-1)/workGroupSize[0];

            glDispatchCompute(numGroups,1,1);
          }
          else{
            glDrawArrays(GL_POINTS, 0, cnt);

            glDisableVertexAttribArray(VERTEX_POS);
            glDisableVertexAttribArray(VERTEX_COLOR);
          }
          
        }

        {
          NV_PROFILE_SECTION("Cmds");

          glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

          glUseProgram(progManager.get(tweak.usecompute ? programs.lodcmds_comp : programs.lodcmds));

          glBindBufferRange(GL_SHADER_STORAGE_BUFFER, SSBO_DATA_INDIRECTS, buffers.lodcmds, jobSize * i, sizeof(DrawIndirects));
          if (tweak.usecompute){
            glDispatchCompute(1,1,1);
          }
          else{
            glDrawArrays(GL_POINTS,0,1);
          }

          glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
        }

        glDisable(GL_RASTERIZER_DISCARD);
      }

      {
        NV_PROFILE_SECTION("Draw");
        // the following drawcalls all source the amount of works from drawindirect buffers
        // generated above
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffers.lodcmds);

        {
          NV_PROFILE_SECTION("Tess");

          glUseProgram(progManager.get(programs.draw_sphere_tess));
          glPatchParameteri(GL_PATCH_VERTICES,3);

          glBindVertexBuffer(0, buffers.sphere_vbo,0,sizeof(vec4));
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers.sphere_ibo);
          glEnableVertexAttribArray(VERTEX_POS);

          if (tweak.useindices){
            glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLES,        GL_TEXTURE_BUFFER, textures.particles);
            glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLEINDICES,  GL_TEXTURE_BUFFER, textures.lodparticles);
          }
          else{
            glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLES,        GL_TEXTURE_BUFFER, textures.lodparticles);
            glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLEINDICES,  GL_TEXTURE_BUFFER, 0);
          }

          glTextureBufferEXT(textures.lodparticles, GL_TEXTURE_BUFFER, itemFormat,
            buffers.lodparticles2);

          glBindBufferRange(GL_UNIFORM_BUFFER,UBO_CMDS, buffers.lodcmds, 
            (i * jobSize), jobSize);
        
          glUniform1i(UNI_USE_CMDOFFSET, 0);
          glDrawElementsIndirect(GL_PATCHES,GL_UNSIGNED_INT, 
            NV_BUFFER_OFFSET(offsetof(DrawIndirects,nearFull) + (i * jobSize)));

          glUniform1i(UNI_USE_CMDOFFSET, 1);
          glDrawElementsIndirect(GL_PATCHES,GL_UNSIGNED_INT, 
            NV_BUFFER_OFFSET(offsetof(DrawIndirects,nearRest) + (i * jobSize)));

          glDisableVertexAttribArray(VERTEX_POS);
          glBindVertexBuffer(0,0,0,0);
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
          glBindBufferBase(GL_UNIFORM_BUFFER,UBO_CMDS,0);
        }

        {
          NV_PROFILE_SECTION("Mesh");

          glUseProgram(progManager.get(programs.draw_sphere));

          glBindVertexBuffer(0, buffers.sphere_vbo,0,sizeof(vec4));
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers.sphere_ibo);
          glEnableVertexAttribArray(VERTEX_POS);

          if (tweak.useindices){
            glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLES,        GL_TEXTURE_BUFFER, textures.particles);
            glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLEINDICES,  GL_TEXTURE_BUFFER, textures.lodparticles);
          }
          else{
            glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLES,        GL_TEXTURE_BUFFER, textures.lodparticles);
            glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLEINDICES,  GL_TEXTURE_BUFFER, 0);
          }

          glTextureBufferEXT(textures.lodparticles, GL_TEXTURE_BUFFER, itemFormat,
            buffers.lodparticles1);

          glBindBufferRange(GL_UNIFORM_BUFFER,UBO_CMDS, buffers.lodcmds, 
            (i * jobSize), jobSize);

          glUniform1i(UNI_USE_CMDOFFSET, 0);
          glDrawElementsIndirect(GL_TRIANGLES,GL_UNSIGNED_INT, 
            NV_BUFFER_OFFSET(offsetof(DrawIndirects,medFull) + (i * jobSize)));

          glUniform1i(UNI_USE_CMDOFFSET, 1);
          glDrawElementsIndirect(GL_TRIANGLES,GL_UNSIGNED_INT, 
            NV_BUFFER_OFFSET(offsetof(DrawIndirects,medRest) + (i * jobSize)));

          glDisableVertexAttribArray(VERTEX_POS);
          glBindVertexBuffer(0,0,0,0);
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
          glBindBufferBase(GL_UNIFORM_BUFFER,UBO_CMDS,0);
        }

        {
          NV_PROFILE_SECTION("Pnts");

          glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

          glUseProgram(progManager.get(programs.draw_sphere_point));

          if (tweak.useindices){
            glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLES,        GL_TEXTURE_BUFFER, textures.particles);
            glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLEINDICES,  GL_TEXTURE_BUFFER, textures.lodparticles);
          }
          else{
            glEnableVertexAttribArray(VERTEX_POS);
            glEnableVertexAttribArray(VERTEX_COLOR);
          }

          if (tweak.useindices){
            glTextureBufferEXT(textures.lodparticles, GL_TEXTURE_BUFFER, itemFormat,
              buffers.lodparticles0);
            glDrawArraysIndirect(GL_POINTS, NV_BUFFER_OFFSET(offsetof(DrawIndirects,farArray) + (i * jobSize)));
          }
          else {
            glBindVertexBuffer(0, buffers.lodparticles0, 0, (GLsizei)itemSize);
            glDrawArraysIndirect(GL_POINTS, NV_BUFFER_OFFSET(offsetof(DrawIndirects,farArray) + (i * jobSize)));
          }

          if (!tweak.useindices){
            glDisableVertexAttribArray(VERTEX_POS);
            glDisableVertexAttribArray(VERTEX_COLOR);
          }

          glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);

          glBindVertexBuffer(0,0,0,0);
        }

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
      }

      offset += cnt;
    }

    NV_PROFILE_SPLIT();
  }

  void Sample::think(double time)
  {
    m_control.processActions(m_window.m_viewsize,
      nv_math::vec2f(m_window.m_mouseCurrent[0],m_window.m_mouseCurrent[1]),
      m_window.m_mouseButtonFlags, m_window.m_wheel);

    tweak.jobCount = std::min(tweak.particleCount,tweak.jobCount);

    if (lastTweak.useindices != tweak.useindices)
    {
      updateProgramDefines();
      progManager.reloadPrograms();
    }

    if (lastTweak.particleCount != tweak.particleCount){
      initParticleBuffer();
      initLodBuffers();
    }

    if (lastTweak.jobCount != tweak.jobCount ||
        lastTweak.useindices != tweak.useindices )
    {
      initLodBuffers();
    }

    if (m_window.onPress(KEY_R)){
      progManager.reloadPrograms();
      glGetProgramiv(progManager.get(programs.lodcontent_comp),GL_COMPUTE_WORK_GROUP_SIZE,(GLint*)workGroupSize);
    }
    if (!progManager.areProgramsValid()){
      waitEvents();
      return;
    }

    int width   = m_window.m_viewsize[0];
    int height  = m_window.m_viewsize[1];

    glViewport(0, 0, width, height);

    glClearColor(0.1f,0.1f,0.1f,0.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    { // Update UBO
      glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SCENE, buffers.scene_ubo);

      sceneUbo.viewport = uvec2(width,height);

      float farplane = 1000.0f;

      nv_math::mat4 projection = nv_math::perspective( (tweak.fov), float(width)/float(height), 0.1f, farplane);
      nv_math::mat4 view = m_control.m_viewMatrix;

      vec4  hPos = projection * nv_math::vec4(1.0f,1.0f,-1000.0f,1.0f);
      vec2  hCoord = vec2(hPos.x/hPos.w, hPos.y/hPos.w);
      vec2  dim  = nv_math::nv_abs(hCoord);
      sceneUbo.viewpixelsize = dim * vec2(float(width),float(height)) * farplane * 0.5f;

      sceneUbo.viewProjMatrix = projection * view;
      sceneUbo.viewMatrix = view;
      sceneUbo.viewMatrixIT = nv_math::transpose(nv_math::invert(view));

      Frustum::init((float (*)[4])&sceneUbo.frustum[0].x,sceneUbo.viewProjMatrix.mat_array);

      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneData), &sceneUbo);
    }

    glPolygonMode(GL_FRONT_AND_BACK, tweak.wireframe ? GL_LINE : GL_FILL );

    if (tweak.uselod){
      drawLod();
    }
    else{
      NV_PROFILE_SECTION("NoLod");

      bool useTess = tweak.nolodtess;

      glUseProgram(progManager.get(useTess ? programs.draw_sphere_tess : programs.draw_sphere));
      glPatchParameteri(GL_PATCH_VERTICES,3);

      glBindVertexBuffer(0, buffers.sphere_vbo,0,sizeof(vec4));
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers.sphere_ibo);

      glEnableVertexAttribArray(VERTEX_POS);

      int fullCnt = tweak.particleCount / PARTICLE_BATCHSIZE;
      int restCnt = tweak.particleCount % PARTICLE_BATCHSIZE;

      GLenum prim = useTess ? GL_PATCHES: GL_TRIANGLES;
      GLenum  itemFormat;
      int     itemSize;
      GLuint  itemBuffer;

      if (tweak.useindices){
        itemFormat  = GL_R32I;
        itemSize    = sizeof(uint);
        itemBuffer  = buffers.particleindices;

        glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLES, GL_TEXTURE_BUFFER,       textures.particles);
        glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLEINDICES, GL_TEXTURE_BUFFER, textures.lodparticles);
      }
      else{
        itemFormat  = GL_RGBA32F;
        itemSize    = sizeof(Particle);
        itemBuffer  = buffers.particles;

        glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLES, GL_TEXTURE_BUFFER,       textures.lodparticles);
        glBindMultiTextureEXT(GL_TEXTURE0 + TEX_PARTICLEINDICES, GL_TEXTURE_BUFFER, 0);
      }

      glTextureBufferEXT(textures.lodparticles, GL_TEXTURE_BUFFER, itemFormat,  itemBuffer);
      glDrawElementsInstanced(prim, PARTICLE_BATCHSIZE * PARTICLE_BASICINDICES, GL_UNSIGNED_INT, 0, fullCnt);

      if (restCnt){
        glTextureBufferRangeEXT(textures.lodparticles, GL_TEXTURE_BUFFER, itemFormat, itemBuffer,itemSize * fullCnt * PARTICLE_BATCHSIZE, restCnt * itemSize);
        glDrawElementsInstanced(prim, restCnt * PARTICLE_BASICINDICES,GL_UNSIGNED_INT, 0, 1);
      }

      glDisableVertexAttribArray(VERTEX_POS);

      glBindVertexBuffer(0,0,0,0);
      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

    glBindBufferBase( GL_UNIFORM_BUFFER, UBO_SCENE, 0);

    {
      NV_PROFILE_SECTION("TwDraw");
      TwDraw();
    }

    lastTweak = tweak;
  }

  void Sample::resize(int width, int height)
  {
    TwWindowSize(width,height);
  }

}//namespace

using namespace dynlod;

int sample_main(int argc, const char** argv)
{
  Sample sample;
  return sample.run(
    PROJECT_NAME,
    argc, argv,
    SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT,
    SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);
}

void sample_print(int level, const char * fmt)
{

}

