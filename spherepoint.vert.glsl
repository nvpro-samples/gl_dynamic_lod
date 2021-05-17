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

#if USE_INDICES
  layout(binding=TEX_PARTICLEINDICES) uniform isamplerBuffer  texParticleIndices;
  layout(binding=TEX_PARTICLES)       uniform samplerBuffer   texParticles;
  
  int particle = texelFetch(texParticleIndices, gl_VertexID).x;
#if USE_COMPACT_PARTICLE
  vec4 inPosColor = texelFetch(texParticles, particle);
  vec4 inPosSize  = vec4(inPosColor.xyz, scene.particleSize);
  vec4 inColor    = unpackUnorm4x8(floatBitsToUint(inPosColor.w));
#else
  vec4 inPosSize  = texelFetch(texParticles, particle*2 + 0);
  vec4 inColor    = texelFetch(texParticles, particle*2 + 1);
#endif
#else
  in layout(location=VERTEX_POS)    vec4 inPosSize;
  in layout(location=VERTEX_COLOR)  vec4 inColor;
#endif

out Interpolants {
  vec4 color;
} OUT;

void main()
{
#if USE_COMPACT_PARTICLE
  float size = scene.particleSize;
#else
  float size = inPosSize.w;
#endif

  vec4 hPos = scene.viewProjMatrix * vec4(inPosSize.xyz,1);
  vec2 pixelsize = 2.0 * size * scene.viewpixelsize / hPos.w;
  
  gl_PointSize = dot(pixelsize,vec2(0.5));
  gl_Position  = hPos;

  vec3 eyePos = vec3(scene.viewMatrixIT[0].w,scene.viewMatrixIT[1].w,scene.viewMatrixIT[2].w);
  
  vec3 normal = (eyePos - inPosSize.xyz);
  
  OUT.color = inColor * shade(normal);
}
