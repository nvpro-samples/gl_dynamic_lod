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