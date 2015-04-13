#version 430
/**/

#extension GL_ARB_shading_language_include : enable
#include "common.h"

#if USE_COMPUTE
layout(local_size_x=32) in;
#endif

layout(binding=SSBO_DATA_INDIRECTS,std430) buffer indirectsBuffer {
  DrawIndirects cmd;
};

void main()
{
#if USE_COMPUTE
  if (gl_GlobalInvocationID.x > 0u) return;
#endif

  {
    uint cnt = cmd.counters.farCnt;
    cmd.farArray.count         = cnt;
    cmd.farArray.instanceCount = 1;
    cmd.farArray.first         = 0;
    cmd.farArray.baseInstance  = 0;
    
    cmd.farIndexed.count         = cnt;
    cmd.farIndexed.instanceCount = 1;
    cmd.farIndexed.first         = 0;
    cmd.farIndexed.baseVertex    = 0;
    cmd.farIndexed.baseInstance  = 0;
    cmd.farIndexed._pad          = uvec2(0);
  }
  
  // med and far use a combination of replicated vertices + instancing
  {
    uint cnt = cmd.counters.medCnt;
    uint cntFull = cnt / PARTICLE_BATCHSIZE;
    uint cntRest = cnt % PARTICLE_BATCHSIZE;
    
    cmd.medFull.count          = PARTICLE_BATCHSIZE * PARTICLE_BASICINDICES;
    cmd.medFull.instanceCount  = cntFull;
    cmd.medFull.first          = 0;
    cmd.medFull.baseVertex     = 0;
    cmd.medFull.baseInstance   = 0;
    cmd.medFull._pad           = uvec2(0);
    
    cmd.medRest.count          = cntRest * PARTICLE_BASICINDICES;
    cmd.medRest.instanceCount  = 1;
    cmd.medRest.first          = 0;
    cmd.medRest.baseVertex     = 0;
    cmd.medRest.baseInstance   = 0;
    cmd.medRest._pad           = uvec2(0);
  }

  {
    uint cnt = cmd.counters.nearCnt;
    uint cntFull = cnt / PARTICLE_BATCHSIZE;
    uint cntRest = cnt % PARTICLE_BATCHSIZE;
    
    cmd.nearFull.count         = PARTICLE_BATCHSIZE * PARTICLE_BASICINDICES;
    cmd.nearFull.instanceCount = cntFull;
    cmd.nearFull.first          = 0;
    cmd.nearFull.baseVertex     = 0;
    cmd.nearFull.baseInstance   = 0;
    cmd.nearFull._pad           = uvec2(0);
    
    cmd.nearRest.count         = cntRest * PARTICLE_BASICINDICES;
    cmd.nearRest.instanceCount = 1;
    cmd.nearRest.first          = 0;
    cmd.nearRest.baseVertex     = 0;
    cmd.nearRest.baseInstance   = 0;
    cmd.nearRest._pad           = uvec2(0);
  }

  cmd.counters.farCnt  = 0;
  cmd.counters.medCnt  = 0;
  cmd.counters.nearCnt = 0;
  cmd.counters._pad    = 0;
  
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