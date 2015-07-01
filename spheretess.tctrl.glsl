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