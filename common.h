/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


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
  using namespace glm;
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
