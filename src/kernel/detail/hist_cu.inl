/**
 * @file hist.inl
 * @author Cody Rivera (cjrivera1@crimson.ua.edu), Megan Hickman Fulp
 * (mlhickm@g.clemson.edu)
 * @brief Fast histogramming from [Gómez-Luna et al. 2013]
 * @version 0.1
 * @date 2020-09-20
 * Created on 2020-02-16
 *
 * @copyright (C) 2020 by Washington State University, The University of
 * Alabama, Argonne National Laboratory See LICENSE in top-level directory
 *
 */

#ifndef D69BE972_2A8C_472E_930F_FFAB041F3F2B
#define D69BE972_2A8C_472E_930F_FFAB041F3F2B

#include <cstdio>
#include <limits>

#include "typing.hh"
#include "utils/config.hh"
#include "utils/timer.hh"

#define MIN(a, b) ((a) < (b)) ? (a) : (b)
const static unsigned int WARP_SIZE = 32;

#define tix threadIdx.x
#define tiy threadIdx.y
#define tiz threadIdx.z
#define bix blockIdx.x
#define biy blockIdx.y
#define biz blockIdx.z
#define bdx blockDim.x
#define bdy blockDim.y
#define bdz blockDim.z

namespace kernel {

template <typename Input>
__global__ void NaiveHistogram(
    Input in_data[], int out_freq[], int N, int symbols_per_thread);

/* Copied from J. Gomez-Luna et al */
template <typename T, typename FREQ>
__global__ void p2013Histogram(T*, FREQ*, size_t, int, int);

}  // namespace kernel

template <typename T>
__global__ void kernel::NaiveHistogram(
    T in_data[], int out_freq[], int N, int symbols_per_thread)
{
  unsigned int i = blockDim.x * blockIdx.x + threadIdx.x;
  unsigned int j;
  if (i * symbols_per_thread < N) {  // if there is a symbol to count,
    for (j = i * symbols_per_thread; j < (i + 1) * symbols_per_thread; j++) {
      if (j < N) {
        unsigned int item = in_data[j];  // Symbol to count
        atomicAdd(&out_freq[item], 1);   // update bin count by 1
      }
    }
  }
}

template <typename T, typename FREQ>
__global__ void kernel::p2013Histogram(
    T* in_data, FREQ* out_freq, size_t N, int nbin, int R)
{
  // static_assert(
  //     std::numeric_limits<T>::is_integer and (not
  //     std::numeric_limits<T>::is_signed), "T must be `unsigned integer` type
  //     of {1,2,4} bytes");

  extern __shared__ int Hs[/*(nbin + 1) * R*/];

  const unsigned int warp_id = (int)(tix / WARP_SIZE);
  const unsigned int lane = tix % WARP_SIZE;
  const unsigned int warps_block = bdx / WARP_SIZE;
  const unsigned int off_rep = (nbin + 1) * (tix % R);
  const unsigned int begin =
      (N / warps_block) * warp_id + WARP_SIZE * blockIdx.x + lane;
  unsigned int end = (N / warps_block) * (warp_id + 1);
  const unsigned int step = WARP_SIZE * gridDim.x;

  // final warp handles data outside of the warps_block partitions
  if (warp_id >= warps_block - 1) end = N;

  for (unsigned int pos = tix; pos < (nbin + 1) * R; pos += bdx) Hs[pos] = 0;
  __syncthreads();

  for (unsigned int i = begin; i < end; i += step) {
    int d = in_data[i];
    d = d <= 0 and d >= nbin ? nbin / 2 : d;
    atomicAdd(&Hs[off_rep + d], 1);
  }
  __syncthreads();

  for (unsigned int pos = tix; pos < nbin; pos += bdx) {
    int sum = 0;
    for (int base = 0; base < (nbin + 1) * R; base += nbin + 1) {
      sum += Hs[base + pos];
    }
    atomicAdd(out_freq + pos, sum);
  }
}

namespace psz {
namespace cuda_hip_compat {

template <typename T>
cusz_error_status hist_default(
    T* in, size_t const inlen, uint32_t* out_hist, int const outlen,
    float* milliseconds, GpuStreamT stream)
{
  int device_id, max_bytes, num_SMs;
  int items_per_thread, r_per_block, grid_dim, block_dim, shmem_use;

  GpuGetDevice(&device_id);
  GpuDeviceGetAttribute(&num_SMs, GpuDevAttrMultiProcessorCount, device_id);

  auto query_maxbytes = [&]() {
    int max_bytes_opt_in;
    GpuDeviceGetAttribute(
        &max_bytes, GpuDevAttrMaxSharedMemoryPerBlock, device_id);

    // account for opt-in extra shared memory on certain architectures
    GpuDeviceGetAttribute(
        &max_bytes_opt_in, GpuDevAttrMaxSharedMemoryPerBlockOptin, device_id);
    max_bytes = std::max(max_bytes, max_bytes_opt_in);

    // config kernel attribute
    GpuFuncSetAttribute(
        (void*)kernel::p2013Histogram<T, uint32_t>,
        (GpuFuncAttribute)GpuFuncAttributeMaxDynamicSharedMemorySize,
        max_bytes);
  };

  auto optimize_launch = [&]() {
    items_per_thread = 1;
    r_per_block = (max_bytes / sizeof(int)) / (outlen + 1);
    grid_dim = num_SMs;
    // fits to size
    block_dim =
        ((((inlen / (grid_dim * items_per_thread)) + 1) / 64) + 1) * 64;
    while (block_dim > 1024) {
      if (r_per_block <= 1) { block_dim = 1024; }
      else {
        r_per_block /= 2;
        grid_dim *= 2;
        block_dim =
            ((((inlen / (grid_dim * items_per_thread)) + 1) / 64) + 1) * 64;
      }
    }
    shmem_use = ((outlen + 1) * r_per_block) * sizeof(int);
  };

  query_maxbytes();
  optimize_launch();

  CREATE_GPUEVENT_PAIR;
  START_GPUEVENT_RECORDING(stream);

  kernel::p2013Histogram<<<grid_dim, block_dim, shmem_use, stream>>>  //
      (in, out_hist, inlen, outlen, r_per_block);

  STOP_GPUEVENT_RECORDING(stream);

  GpuStreamSync(stream);
  TIME_ELAPSED_GPUEVENT(milliseconds);
  DESTROY_GPUEVENT_PAIR;

  return CUSZ_SUCCESS;
}

}  // namespace cuda_hip_compat
}  // namespace psz

#endif /* D69BE972_2A8C_472E_930F_FFAB041F3F2B */
