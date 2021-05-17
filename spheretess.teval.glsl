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

layout(triangles, fractional_even_spacing, ccw) in;

in Data {
  vec3  pos;
} IN[];

patch in PerPatch {
#if USE_COMPACT_PARTICLE
  vec4  posColor;
#else
  vec4  posSize;
  vec4  color;
#endif
}INpatch;

out Interpolants {
  vec3 normal;
  vec4 color;
} OUT;

void main()
{
  vec3 p0 = gl_TessCoord.x * IN[0].pos;
  vec3 p1 = gl_TessCoord.y * IN[1].pos;
  vec3 p2 = gl_TessCoord.z * IN[2].pos;
  
  vec3 normal = normalize(p0 + p1 + p2);
#if USE_COMPACT_PARTICLE
  vec3 pos    = INpatch.posColor.xyz + normal * scene.particleSize;
  OUT.color   = unpackUnorm4x8(floatBitsToUint(INpatch.posColor.w));
#else
  vec3 pos    = INpatch.posSize.xyz + normal * INpatch.posSize.w;
  OUT.color   = INpatch.color;
#endif
  
  
  gl_Position = scene.viewProjMatrix * vec4(pos,1);
  OUT.normal  = normal;
  
}
