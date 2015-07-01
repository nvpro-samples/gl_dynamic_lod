# gl dynamic lod

With the addition of indirect rendering (```ARB_draw_indirect``` and ```ARB_multi_draw_indirect```) OpenGL got an efficient mechanism that allows the GPU to create or modify its own work without stalling the pipeline. As CPU and GPU are best used when working asynchronously, avoiding readbacks to CPU to drive decision making is beneficial.

In this sample we use ```ARB_draw_indirect``` and ```ARB_shader_atomic_counters``` to build three distinct render lists for drawing particles as spheres, each using a different shader and representing a different level of detail (LOD):

* Draw as point
* Draw as instanced low resolution mesh
* Draw as instanced adaptive tessellated mesh

![sample screenshot](https://github.com/nvpro-samples/gl_dynamic_lod/blob/master/doc/sample.jpg)

This allows us to limit the number of total geometry being rasterized and still benefit from high geometric quality were needed.

![sample screenshot](https://github.com/nvpro-samples/gl_dynamic_lod/blob/master/doc/wireframe.jpg)

The frame is therefore split into two parts:

1. **LOD Classification**:
 - Each particle is put in one of the appropriate lists using global atomics based on projected size in the viewport, frustum-culling is also applied in advance.
 - A single shader invocation manipulates the DrawIndirect commands based on the atomic counter values. This step is required as the sample uses an alternative way to classic instancing.
2. **Rendering**:
 - Every list is drawn by one or two ```glDrawElementsIndirect``` calls to render the particles. 
 - Instancing is done via batching in two steps (see later).

``` cpp
struct DrawElementsIndirect {
  uint  elementCount;   // modified at runtime
  uint  instanceCount;  // modified at runtime
  uint  first;          // 0
  uint  baseVertex;     // 0
  uint  baseInstance;   // 0
};
```

#### Batched low complexity mesh instancing

When instancing meshes that have only very few triangles, the classic way of using the graphics api's instance counter may not be the most efficient for the hardware. We use batching to improve the performance. Instead of drawing all particles at once, we draw them in two steps, which depends on how much we want to draw overall (*listSize*):

 1. **elementCount** = batchSize * meshSize; **instanceCount** = *listSize* / batchSize;
 2. **elementCount** = (*listSize* % batchSize) * meshSize; **instanceCount** = 1;

First we draw via classic instancing batchSize many meshes, and then whatever is left.

The instanced mesh is replicated *batchSize*-often in the source VBO/IBO, instead of having it stored only once. That way the per-instance drawcall for the hardware does more work, which helps leveraging parallelism on the GPU. The memory cost of this can typically be neglected as we specifically target low-complexity meshes with just a few triangles & vertices, if we had a lot of triangles per-mesh than classic instancing would do the trick.

With classic instancing we simply would use the **gl_InstanceID** to find out which instance we are, but here we use an alternative formula:

``` cpp
instanceID = batchedID + gl_InstanceID * MESH_BATCHSIZE;
```

**batchedID** represents which of the replicated batched mesh we currently render. While it doesn't exist, we can derive it from the **gl_VertexID**, as the index buffer accounts for the vertex data replication in the VBO. The index values (gl_VertexID) of a batched mesh are in the range [MESH_VERTICES * batchedID, (MESH_VERTICES * (batchedID+1)) -1]

``` cpp
instanceID = (gl_VertexID / MESH_VERTICES) + gl_InstanceID * MESH_BATCHSIZE;
```

When drawing the rest of the meshes with the second drawcall, one has to offset the instanceID by the meshes already drawn by first.

``` cpp
instanceID +=   int(firstCmd.instanceCount) * 
              ( int(firstCmd.elementCount) / MESH_INDICES); 
```

#### Performance

Through the ui the sample can be modified a bit, for example "invisible rendering" via ```glEnable(GL_RASTERIZER_DISCARD)``` can be used to drive the classification or compute shaders. The entire task can also be split into multiple jobs, which allows to decrease the size of temporary list buffers. Last but not least one can experiment with recording the particle data directly or indices. The default configuration gives the best performance for higher amounts of particles (compute, single job, indices).

Timings in microseconds via GL timer query taken on a Quadro M6000, 1048574 particles

``` cpp
 Timer Frame;    GL    4206;
  Timer Lod;     GL     151;
   Timer Cont;   GL     139;  // Particle classification (content)
   Timer Cmds;   GL       8;  // DrawIndirect struct (commands)
  Timer Draw;    GL    3888;
   Timer Tess;   GL     256;  // Adpatively-tessellated spheres
   Timer Mesh;   GL    3586;  // Simple sphere mesh
   Timer Pnts;   GL      39;  // Spheres drawn as points
  Timer TwDraw;  GL     160;
``` 

#### Sample Highlights

The user can influence the classification based on the viewport size using the "pixelsize" parameters. The classification can also be paused and re-used despite camera being changed, which can be useful to see the frustum culling in action, or inspect the low-resolution representations. 

Key functionality is found in

- Sample::drawLod()

As well as in helper functions

- Sample::initParticleBuffer()
- Sample::initLodBuffers()

In common.h you can set ```USE_COMPACT_PARTICLE``` to 1 to reduce the size of the particles to a single vec4 by giving all particles the same world size. This mode allows rendering around 130 million particles on NVIDIA hardware, twice as much as the default 0 setting.

#### Building
Ideally clone this and other interesting [nvpro-samples](https://github.com/nvpro-samples) repositories into a common subdirectory. You will always need [shared_sources](https://github.com/nvpro-samples/shared_sources) and on Windows [shared_external](https://github.com/nvpro-samples/shared_external). The shared directories are searched either as subdirectory of the sample or one directory up. It is recommended to use the [build_all](https://github.com/nvpro-samples/build_all) cmake as entry point, it will also give you options to enable/disable individual samples when creating the solutions.

#### Related Samples
[gl_occlusion_culling](https://github.com/nvpro-samples/gl_occlusion_culling) makes use of similar OpenGL functionality to perform more accurate visibility culling.

```
    Copyright (c) 2014-2015, NVIDIA CORPORATION. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Neither the name of NVIDIA CORPORATION nor the names of its
       contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

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
```

