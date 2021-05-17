# gl dynamic lod

With the addition of indirect rendering (```ARB_draw_indirect``` and ```ARB_multi_draw_indirect```), OpenGL got an efficient mechanism that allows the GPU to create or modify its own work without stalling the pipeline. As the CPU and GPU are best used when working asynchronously, avoiding readbacks to CPU to drive decision making is beneficial.

In this sample we use ```ARB_draw_indirect``` and ```ARB_shader_atomic_counters``` to build three distinct render lists for drawing particles as spheres, each using a different shader and representing a different level of detail (LOD):

* Draw as point
* Draw as instanced low resolution mesh
* Draw as instanced adaptively tessellated mesh

![sample screenshot](https://github.com/nvpro-samples/gl_dynamic_lod/blob/master/doc/sample.jpg)

This allows us to limit the total amount of geometry being rasterized, and still benefit from high geometric quality where needed.

![sample screenshot](https://github.com/nvpro-samples/gl_dynamic_lod/blob/master/doc/wireframe.jpg)

The frame timeline is therefore split into two parts:

1. **LOD Classification**:
 - Each particle is put in one of the appropriate lists using global atomics based on projected size in the viewport. Frustum-culling is also applied in advance.
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

When instancing meshes that have only very few triangles, the classic way of using the graphics API's instance counter may not be the most efficient for the hardware. We use batching to improve performance. Instead of drawing all particles at once, we draw them in two steps, which depends on how much we want to draw overall (*listSize*):

 1. **elementCount** = batchSize * meshSize; **instanceCount** = *listSize* / batchSize;
 2. **elementCount** = (*listSize* % batchSize) * meshSize; **instanceCount** = 1;

We first draw _batchSize_ meshes via classic instancing, and then whatever is left.

The instanced mesh is replicated *batchSize* times in the source VBO/IBO, instead of storing it only once. That way each per-instance hardware drawcall does more work, which helps leverage GPU parallelism. The memory cost of this can typically be neglected, as we specifically target low-complexity meshes with just a few triangles & vertices; if we had a lot of triangles per-mesh, then classic instancing would do the trick.

With classic instancing we would simply use **gl_InstanceID** to find out which instance we are, but here we use an alternative formula:

``` cpp
instanceID = batchedID + gl_InstanceID * MESH_BATCHSIZE;
```

**batchedID** represents which of the replicated batched meshes we are currently rendering. While it isn't a built-in vertex shader variable, we can derive it from the **gl_VertexID**, as the index buffer accounts for the vertex data replication in the VBO. The index values (gl_VertexID) of a batched mesh are in the range [MESH_VERTICES * batchedID, (MESH_VERTICES * (batchedID+1)) -1], so

``` cpp
instanceID = (gl_VertexID / MESH_VERTICES) + gl_InstanceID * MESH_BATCHSIZE;
```

When drawing the rest of the meshes with the second drawcall, one has to offset the instanceID by the number of meshes already drawn.

``` cpp
instanceID +=   int(firstCmd.instanceCount) * 
              ( int(firstCmd.elementCount) / MESH_INDICES); 
```

#### Performance

The UI can be used to modify the sample a bit. For example, "invisible rendering" via ```glEnable(GL_RASTERIZER_DISCARD)``` can be used to time the classification or compute shaders alone. The entire task can also be split into multiple jobs, which allows the program to decrease the size of temporary list buffers. Last but not least, one can experiment with recording the particle data directly or indices. The default configuration gives the best performance for higher amounts of particles (compute, single job, indices).

Timings in microseconds via GL timer query taken on a Quadro M6000, 1048574 particles

``` cpp
 Timer Frame;    GL    4206;
  Timer Lod;     GL     151;
   Timer Cont;   GL     139;  // Particle classification (content)
   Timer Cmds;   GL       8;  // DrawIndirect struct (commands)
  Timer Draw;    GL    3888;
   Timer Tess;   GL     256;  // Adaptively-tessellated spheres
   Timer Mesh;   GL    3586;  // Simple sphere mesh
   Timer Pnts;   GL      39;  // Spheres drawn as points
  Timer TwDraw;  GL     160;
```

#### Sample Highlights

The user can influence the classification based on the viewport size using the "pixelsize" parameters. The classification can also be paused and re-used despite camera being changed, which can be useful to see the frustum culling in action, or inspect low-resolution representations.

Key functionality is found in

- Sample::drawLod()

As well as in helper functions

- Sample::initParticleBuffer()
- Sample::initLodBuffers()

In common.h, you can set ```USE_COMPACT_PARTICLE``` to 1 to reduce the size of the particles to a single vec4 by giving all particles the same world size. This mode allows rendering around 130 million particles on NVIDIA hardware, twice as much as the default 0 setting.

#### Building
Ideally, clone this and other interesting [nvpro-samples](https://github.com/nvpro-samples) repositories into a common subdirectory. You will always need [nvpro_core](https://github.com/nvpro-samples/nvpro_core). The nvpro_core is searched either as a subdirectory of the sample, or one directory up.

If you are interested in multiple samples, you can use the [build_all](https://github.com/nvpro-samples/build_all) CMAKE as an entry point. It will also give you options to enable or disable individual samples when creating the solutions.

#### Related Samples
[gl_occlusion_culling](https://github.com/nvpro-samples/gl_occlusion_culling) makes use of similar OpenGL functionality to perform more accurate visibility culling.

