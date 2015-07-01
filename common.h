
#define VERTEX_POS      0
#define VERTEX_COLOR    1

#define UBO_SCENE     0
#define UBO_CMDS      1

#define UNI_USE_CMDOFFSET             0
#define UNI_CONTENT_IDX_OFFSET        0
#define UNI_CONTENT_IDX_MAX           1

#define TEX_PARTICLES         0
#define TEX_PARTICLEINDICES   1

#define ABO_DATA_COUNTS       0

#define SSBO_DATA_INDIRECTS   0
#define SSBO_DATA_POINTS      1
#define SSBO_DATA_BASIC       2
#define SSBO_DATA_TESS        3

#define PARTICLE_BATCHSIZE      1024
#define PARTICLE_BASICVERTICES  12
#define PARTICLE_BASICPRIMS     20
#define PARTICLE_BASICINDICES   (PARTICLE_BASICPRIMS*3)

// setting this to 1 will cause all particles to have the same "size"
// and pack color, so that the overall size of the particle is halved 
#define USE_COMPACT_PARTICLE  0


#ifdef __cplusplus
namespace dynlod
{
#endif

struct DrawArrays {
  uint  count;
  uint  instanceCount;
  uint  first;
  uint  baseInstance;
};

struct DrawElements {
  uint  count;
  uint  instanceCount;
  uint  first;
  uint  baseVertex;
  uint  baseInstance;
  uvec2 _pad;
};

struct DrawCounters {
  uint  farCnt;
  uint  medCnt;
  uint  nearCnt;
  uint  _pad;
};

struct DrawIndirects {
  DrawCounters  counters;

  DrawArrays    farArray;
  DrawElements  farIndexed;
  
  DrawElements  medFull;
  DrawElements  medRest;
  
  DrawElements  nearFull;
  DrawElements  nearRest;
};

struct Particle {
#if USE_COMPACT_PARTICLE
  vec4  posColor;
#else
  vec4  posSize;
  vec4  color;
#endif
};

struct SceneData {
  mat4  viewProjMatrix;
  mat4  viewMatrix;
  mat4  viewMatrixIT;
  
  uvec2 viewport;
  vec2  viewpixelsize;
  
  vec4  frustum[6];
  
  float farPixels;
  float nearPixels;
  float tessPixels;
  float particleSize;
};

#ifdef __cplusplus
}
#endif

#if defined(GL_core_profile) || defined(GL_compatibility_profile) || defined(GL_es_profile)
// prevent this to be used by c++

#ifndef USE_INDICES
#define USE_INDICES 1
#endif

vec4  shade(vec3 normal)
{
  vec3  lightDir = normalize(vec3(-1,2,1));
  float intensity = dot(normalize(normal),lightDir) * 0.5 + 0.5;
  return mix(vec4(0,0.25,0.75,0),vec4(1,1,1,0),intensity);
}

layout(std140,binding=UBO_SCENE) uniform sceneBuffer {
  SceneData   scene;
};
#endif

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