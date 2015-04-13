#version 430
/**/

#extension GL_ARB_shading_language_include : enable
#include "common.h"

layout(location=UNI_CONTENT_IDX_OFFSET) uniform int idxOffset;

#if USE_COMPUTE

layout(local_size_x=512) in;

int IDX = int(gl_GlobalInvocationID.x) + idxOffset;

layout(location=UNI_CONTENT_IDX_MAX)  uniform int idxMax;
layout(binding=TEX_PARTICLES) uniform samplerBuffer texVertices;

vec4 inPosSize = texelFetch(texVertices, (IDX)*VERTEX_ATTRIBS + VERTEX_POS);
vec4 inColor   = texelFetch(texVertices, (IDX)*VERTEX_ATTRIBS + VERTEX_COLOR);

#else

int IDX = gl_VertexID + idxOffset;

in layout(location=VERTEX_POS)    vec4 inPosSize;
in layout(location=VERTEX_COLOR)  vec4 inColor;

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
  float size = inPosSize.w;
  
  for (int i = 0; i < 6; i++){
    if (dot(scene.frustum[i],vec4(pos,1)) < -size){
      return;
    }
  }
  
  vec4 hPos = scene.viewProjMatrix * vec4(inPosSize.xyz,1);
  vec2 pixelsize = 2.0 * inPosSize.w * scene.viewpixelsize / hPos.w;
  
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
    particlesNear[slot].posSize = inPosSize;
    particlesNear[slot].color   = inColor;
  }
  else if (coverage < scene.farPixels) {
    uint slot = atomicCounterIncrement(counterFar);
    particlesFar[slot].posSize = inPosSize;
    particlesFar[slot].color   = inColor;
  }
  else {
    uint slot = atomicCounterIncrement(counterMed);
    particlesMed[slot].posSize = inPosSize;
    particlesMed[slot].color   = inColor;
  }
#endif
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