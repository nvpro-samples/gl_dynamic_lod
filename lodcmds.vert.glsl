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
