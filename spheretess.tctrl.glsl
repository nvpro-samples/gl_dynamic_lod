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

layout(vertices = 4) out;

layout(binding=TEX_PARTICLEINDICES) uniform isamplerBuffer  texParticleIndices;
layout(binding=TEX_PARTICLES)       uniform samplerBuffer   texParticles;


in Data {
  vec3  offsetPos;
  flat  int particle;
} IN[];

out Data {
  vec3  pos;
} OUT[];

patch out PerPatch {
#if USE_COMPACT_PARTICLE
  vec4  posColor;
#else
  vec4  posSize;
  vec4  color;
#endif
}OUTpatch;

void main()
{
  int     particle = IN[0].particle;
  
#if USE_INDICES
  particle = texelFetch(texParticleIndices, particle).r ;
#endif
  
#if USE_COMPACT_PARTICLE
  vec4    inPosColor = texelFetch(texParticles, particle);
#else
  vec4    inPosSize = texelFetch(texParticles, particle*2 + 0);
  vec4    inColor   = texelFetch(texParticles, particle*2 + 1);
#endif

  OUT[gl_InvocationID].pos = IN[gl_InvocationID].offsetPos;
  
  if (gl_InvocationID == 0){
#if USE_COMPACT_PARTICLE
    OUTpatch.posColor = inPosColor;
    vec4 inPosSize = vec4(inPosColor.xyz, scene.particleSize);
#else
    OUTpatch.posSize = inPosSize;
    OUTpatch.color   = inColor;
#endif
    vec4 hPos = scene.viewProjMatrix * vec4(inPosSize.xyz,1);
    vec2 pixelsize = 2.0 * inPosSize.w * scene.viewpixelsize / hPos.w;
    
    float tess = dot(pixelsize,vec2(0.5)) / scene.tessPixels;
    
    tess = clamp(tess,1.0,128.0);
    
    gl_TessLevelInner[0] = tess;
    
    gl_TessLevelOuter[0] = tess;
    gl_TessLevelOuter[1] = tess;
    gl_TessLevelOuter[2] = tess;
  }
}
