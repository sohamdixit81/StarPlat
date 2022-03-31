// FOR BC: nvcc bc_dsl_v2.cu -arch=sm_60 -std=c++14 -rdc=true # HW must support CC 6.0+ Pascal or after
#ifndef GENCPP_PAGERANK_DSL_V2_H
#define GENCPP_PAGERANK_DSL_V2_H
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <cuda.h>
#include "../graph.hpp"
#include "../libcuda.cuh"
#include <cooperative_groups.h>

void Compute_PR(graph& g,float beta,float delta,int maxIter,
  float* pageRank);

__device__ float num_nodes = g.num_nodes( ); // DEVICE ASSTMENT in .h

__device__ int iterCount = 0; // DEVICE ASSTMENT in .h

__device__ float diff; // DEVICE ASSTMENT in .h

__global__ void Compute_PR_kernel(int V, int E, int* d_meta, int* d_data, int* d_weight ,graph& g, float beta, float delta, int maxIter, 
  float* d_pageRank){ // BEGIN KER FUN via ADDKERNEL
  unsigned v = blockIdx.x * blockDim.x + threadIdx.x;
  if(v >= V) return;
  float sum = 0.000000; // DEVICE ASSTMENT in .h

  for (int edge = d_rev_meta[v]; edge < d_rev_meta[v+1]; edge++)
  {int nbr = d_data[edge] ;
    sum = sum + d_pageRank[nbr] / (d_meta[nbr+1]-d_meta[nbr]);
  } //  end FOR NBR ITR. TMP FIX!
  float val = (1 - delta) / num_nodes + delta * sum; // DEVICE ASSTMENT in .h

  atomicAdd(& diff, (float)val - d_pageRank[v]);
  d_pageRank_nxt[v] = val;
} // end KER FUNC

#endif