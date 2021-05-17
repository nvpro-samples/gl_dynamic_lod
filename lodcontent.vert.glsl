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



#version 430
/**/

#extension GL_ARB_shading_language_include : enable
#include "common.h"

layout(location=UNI_CONTENT_IDX_OFFSET) uniform int idxOffset;

#if USE_COMPUTE

layout(local_size_x=512) in;

int IDX = int(gl_GlobalInvocationID.x) + idxOffset;

layout(location=UNI_CONTENT_IDX_MAX)  uniform int idxMax;
layout(binding=TEX_PARTICLES) uniform samplerBuffer texParticles;

#if USE_COMPACT_PARTICLE
  vec4 inPosColor = texelFetch(texParticles, IDX);
  vec4 inPosSize  = vec4(inPosColor.xyz, scene.particleSize);
  vec4 inColor    = unpackUnorm4x8(floatBitsToUint(inPosColor.w));
#else
  vec4 inPosSize  = texelFetch(texParticles, IDX*2 + 0);
  vec4 inColor    = texelFetch(texParticles, IDX*2 + 1);
#endif

#else

int IDX = gl_VertexID + idxOffset;

in layout(location=VERTEX_POS)    vec4 inPosSize;
in layout(location=VERTEX_COLOR)  vec4 inColor;

#if USE_COMPACT_PARTICLE
vec4 inPosColor = vec4(inPosSize.xyz, uintBitsToFloat(packUnorm4x8(inColor)));
#endif

#endif


layout(binding=ABO_DATA_COUNTS,offset=0)  uniform atomic_uint counterFar;
layout(binding=ABO_DATA_COUNTS,offset=4)  uniform atomic_uint counterMed;
layout(binding=ABO_DATA_COUNTS,offset=8)  uniform atomic_uint counterNear;

#if USE_INDICES

layout(binding=SSBO_DATA_POINTS,std430) buffer pointsBuffer {
  int particlesFar[];
};

layout(binding=SSBO_DATA_BASIC,std430) buffer basicBuffer {
  int particlesMed[];
};

layout(binding=SSBO_DATA_TESS,std430) buffer tessBuffer {
  int particlesNear[];
};

#else

layout(binding=SSBO_DATA_POINTS,std430) buffer pointsBuffer {
  Particle particlesFar[];
};

layout(binding=SSBO_DATA_BASIC,std430) buffer basicBuffer {
  Particle particlesMed[];
};

layout(binding=SSBO_DATA_TESS,std430) buffer tessBuffer {
  Particle particlesNear[];
};

#endif

void main()
{
#if USE_COMPUTE
  if (IDX >= idxMax) return;
#endif

  vec3  pos  = inPosSize.xyz;
#if USE_COMPACT_PARTICLE
  float size = scene.particleSize;
#else
  float size = inPosSize.w;
#endif
  
  for (int i = 0; i < 6; i++){
    if (dot(scene.frustum[i],vec4(pos,1)) < -size){
      return;
    }
  }
  
  vec4 hPos = scene.viewProjMatrix * vec4(inPosSize.xyz,1);
  vec2 pixelsize = 2.0 * size * scene.viewpixelsize / hPos.w;
  
  float coverage = dot(pixelsize,vec2(0.5));
  
#if USE_INDICES
  if (coverage > scene.nearPixels) {
    uint slot = atomicCounterIncrement(counterNear);
    particlesNear[slot] = IDX;
  }
  else if (coverage < scene.farPixels) {
    uint slot = atomicCounterIncrement(counterFar);
    particlesFar[slot] = IDX;
  }
  else {
    uint slot = atomicCounterIncrement(counterMed);
    particlesMed[slot] = IDX;
  }
#else
  if (coverage > scene.nearPixels) {
    uint slot = atomicCounterIncrement(counterNear);
#if USE_COMPACT_PARTICLE
    particlesNear[slot].posColor = inPosColor;
#else
    particlesNear[slot].posSize = inPosSize;
    particlesNear[slot].color   = inColor;
#endif
  }
  else if (coverage < scene.farPixels) {
    uint slot = atomicCounterIncrement(counterFar);
#if USE_COMPACT_PARTICLE
    particlesFar[slot].posColor = inPosColor;
#else
    particlesFar[slot].posSize = inPosSize;
    particlesFar[slot].color   = inColor;
#endif
  }
  else {
    uint slot = atomicCounterIncrement(counterMed);
#if USE_COMPACT_PARTICLE
    particlesMed[slot].posColor = inPosColor;
#else
    particlesMed[slot].posSize = inPosSize;
    particlesMed[slot].color   = inColor;
#endif
  }
#endif
}
