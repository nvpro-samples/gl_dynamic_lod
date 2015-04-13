#version 430
/**/

#extension GL_ARB_shading_language_include : enable
#include "common.h"

#if USE_INDICES
  layout(binding=TEX_PARTICLEINDICES) uniform isamplerBuffer  texParticleIndices;
  layout(binding=TEX_PARTICLES)       uniform samplerBuffer   texParticles;
  
  int particle = texelFetch(texParticleIndices, gl_VertexID).x;
  vec4 inPosSize  = texelFetch(texParticles, particle*2+0);
  vec4 inColor    = texelFetch(texParticles, particle*2+1);
#else
  in layout(location=VERTEX_POS)    vec4 inPosSize;
  in layout(location=VERTEX_COLOR)  vec4 inColor;
#endif

out Interpolants {
  vec4 color;
} OUT;

void main()
{


  vec4 hPos = scene.viewProjMatrix * vec4(inPosSize.xyz,1);
  vec2 pixelsize = 2.0 * inPosSize.w * scene.viewpixelsize / hPos.w;
  
  gl_PointSize = dot(pixelsize,vec2(0.5));
  gl_Position  = hPos;

  vec3 eyePos = vec3(scene.viewMatrixIT[0].w,scene.viewMatrixIT[1].w,scene.viewMatrixIT[2].w);
  
  vec3 normal = (eyePos - inPosSize.xyz);
  
  OUT.color = inColor * shade(normal);
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