/**
 * @file test_l3_lorenzosp.cc
 * @author Jiannan Tian
 * @brief
 * @version 0.4
 * @date 2023-04-05
 *
 * (C) 2023 by Indiana University, Argonne National Laboratory
 *
 */

#include <thrust/execution_policy.h>
#include <thrust/sort.h>

#include <cstdint>
#include <stdexcept>
#include <typeinfo>

#include "kernel/l23.hh"
#include "kernel/l23r.hh"
#include "kernel/spv.hh"
#include "mem/compact.hh"
#include "mem/memseg_cxx.hh"
#include "stat/compare_cpu.hh"
#include "stat/compare_thrust.hh"
#include "utils/print_arr.hh"
#include "utils/viewer.hh"

std::string type_literal;

template <typename T, typename EQ, bool LENIENT = true>
bool testcase(
    size_t const x, size_t const y, size_t const z, double const eb,
    int const radius = 512)
{
  // When the input type is FP<X>, the internal precision should be the same.
  using FP = T;
  auto len = x * y * z;

  auto oridata = new pszmem_cxx<T>(x, y, z, "oridata");
  auto de_data = new pszmem_cxx<T>(x, y, z, "de_data");
  auto outlier = new pszmem_cxx<T>(x, y, z, "outlier, normal");
  auto ectrl_focus = new pszmem_cxx<EQ>(x, y, z, "ectrl_focus");
  auto ectrl_ref = new pszmem_cxx<EQ>(x, y, z, "ectrl_ref");
  auto spval = new pszmem_cxx<T>(x, y, z, "spval");
  auto spidx = new pszmem_cxx<uint32_t>(x, y, z, "spidx");

  oridata->control({Malloc, MallocHost});
  de_data->control({Malloc, MallocHost});
  outlier->control({Malloc, MallocHost});
  ectrl_focus->control({Malloc, MallocHost});
  ectrl_ref->control({Malloc, MallocHost});
  spval->control({Malloc, MallocHost});
  spidx->control({Malloc, MallocHost});

  // setup dummy outliers
  auto ratio_outlier = 0.00001;
  auto num_of_exaggerated = (int)(ratio_outlier * len);
  auto step = (int)(len / (num_of_exaggerated + 1));

  cout << "num_of_exaggerated: " << num_of_exaggerated << endl;
  cout << "step of inserting outlier: " << step << endl;

  CompactGpuDram<T> compact_outlier;
  compact_outlier.reserve_space(len).control({Malloc, MallocHost});

  psz::testutils::cuda_hip_compat::rand_array<T>(oridata->dptr(), len);

  oridata->control({D2H});
  for (auto i = 0; i < num_of_exaggerated; i++) {
    oridata->hptr(i * step) *= 4;
  }
  oridata->control({H2D});

  GpuStreamT stream;
  GpuStreamCreate(&stream);

  float time;
  auto len3 = dim3(x, y, z);

  psz_comp_l23r<T, EQ, false>(  //
      oridata->dptr(), len3, eb, radius, ectrl_focus->dptr(), &compact_outlier,
      &time, stream);
  GpuStreamSync(stream);

  psz_comp_l23<T, EQ>(  //
      oridata->dptr(), len3, eb, radius, ectrl_ref->dptr(), outlier->dptr(),
      &time, stream);
  GpuStreamSync(stream);

  ectrl_focus->control({ASYNC_D2H}, stream);
  ectrl_ref->control({ASYNC_D2H}, stream);
  GpuStreamSync(stream);

  auto two_ectrl_eq = true;
  for (auto i = 0; i < len; i++) {
    auto e1 = ectrl_focus->hptr(i);
    auto e2 = ectrl_ref->hptr(i);
    if (e1 != e2) {
      printf("i: %d\t not equal\te1: %u\te2: %u\n", i, e1, e2);
      two_ectrl_eq = false;
    }
  }
  printf("    two kinds of ectrls equal?: %s\n", two_ectrl_eq ? "yes" : "no");

  compact_outlier.make_host_accessible(stream);

  ////////////////////////////////////////////////////////////////////////////////
  int splen;
  {
    thrust::sort_by_key(
        thrust::host,  // execution
        compact_outlier.h_idx,
        compact_outlier.h_idx + compact_outlier.h_num,  // key
        compact_outlier.h_val);                         // value

    float __t;
    psz::spv_gather<PROPER_GPU_BACKEND, T, uint32_t>(
        outlier->dptr(), len, spval->dptr(), spidx->dptr(), &splen, &__t,
        stream);
    spidx->control({D2H});
    spval->control({D2H});

    auto two_outlier_eq = true;

    for (auto i = 0; i < splen; i++) {
      auto normal_idx = spidx->hptr(i);
      auto normal_val = spval->hptr(i);
      auto compact_idx = compact_outlier.h_idx[i];
      auto compact_val = compact_outlier.h_val[i];

      if (normal_idx != compact_idx or normal_val != compact_val) {
        // printf(
        //     "i: %d\t"
        //     "normal-(idx,val)=(%u,%4.1f)\t"
        //     "compact-(idx,val)=(%u,%4.1f)"
        //     "\n",
        //     i, normal_idx, normal_val, compact_idx, compact_val);
        two_outlier_eq = false;
      }
    }
    printf("#normal_outlier: %d\n", splen);
    printf("#compact_outlier: %d\n", compact_outlier.num_outliers());

    printf(
        "    two kinds of outliers equal?: %s\n",
        two_outlier_eq ? "yes" : "no");
  }
  // //\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\


  psz::spv_scatter<PROPER_GPU_BACKEND, T, u4>(
      compact_outlier.val(), compact_outlier.idx(),
      compact_outlier.num_outliers(), de_data->dptr(), &time, stream);

  psz_decomp_l23<T, EQ, FP>(
      ectrl_focus->dptr(), len3, de_data->dptr(), eb, radius,
      de_data->dptr(),  //
      &time, stream);
  GpuStreamSync(stream);

  de_data->control({D2H});

  size_t first_non_eb = 0;
  // bool   error_bounded = psz::error_bounded<THRUST, T>(de_data, oridata,
  // len, eb, &first_non_eb);
  bool error_bounded = psz::error_bounded<CPU, T>(
      de_data->hptr(), oridata->hptr(), len, eb, &first_non_eb);

  // psz::eval_dataquality_gpu(oridata->dptr(), de_data->dptr(), len);

  GpuStreamDestroy(stream);

  delete oridata;
  delete de_data;
  delete ectrl_focus;
  delete ectrl_ref;
  delete outlier;
  delete spidx;
  delete spval;

  compact_outlier.control({Free, FreeHost});

  printf(
      "(%zu,%zu,%zu)\t(T=%s,EQ=%s)\terror bounded?\t", x, y, z,
      typeid(T).name(), typeid(EQ).name());
  if (not LENIENT) {
    if (not error_bounded) throw std::runtime_error("NO");
  }
  else {
    cout << (error_bounded ? "yes" : "NO") << endl;
  }

  return error_bounded;
}
