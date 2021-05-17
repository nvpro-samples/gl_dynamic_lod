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

in layout(location=VERTEX_POS)      vec3 offsetPos;

layout(binding=TEX_PARTICLEINDICES) uniform isamplerBuffer  texParticleIndices;
layout(binding=TEX_PARTICLES)       uniform samplerBuffer   texParticles;

layout(binding=UBO_CMDS,std140) uniform prevCmdBuffer {
  DrawIndirects  cmd;
};

layout(location=UNI_USE_CMDOFFSET) uniform int useCmdOffset;

out Interpolants {
  vec3  normal;
  vec4  color;
} OUT;

void main()
{
  int     particle = (gl_VertexID/PARTICLE_BASICVERTICES) + gl_InstanceID * PARTICLE_BATCHSIZE;
  particle += useCmdOffset * (int(cmd.medFull.instanceCount) * (int(cmd.medFull.count)/PARTICLE_BASICINDICES));
  
#if USE_INDICES
  particle = texelFetch(texParticleIndices, particle).r;
#endif
  
#if USE_COMPACT_PARTICLE
  vec4    inPosColor = texelFetch(texParticles, particle);
  vec4    inPosSize  = vec4(inPosColor.xyz, scene.particleSize);
  vec4    inColor    = unpackUnorm4x8(floatBitsToUint(inPosColor.w));
#else
  vec4    inPosSize = texelFetch(texParticles, particle*2 + 0);
  vec4    inColor   = texelFetch(texParticles, particle*2 + 1);
#endif
  vec3    pos = offsetPos * inPosSize.w + inPosSize.xyz;

  gl_Position = scene.viewProjMatrix * vec4(pos,1);
  
  OUT.normal = offsetPos;
  OUT.color = inColor;
}
