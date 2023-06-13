/**
 * @file compressor.hh
 * @author Jiannan Tian
 * @brief
 * @version 0.3
 * @date 2022-04-23
 *
 * (C) 2022 by Washington State University, Argonne National Laboratory
 *
 */

#ifndef CUSZ_COMPRESSOR_HH
#define CUSZ_COMPRESSOR_HH

#include <cuda_runtime.h>
#include <cstdint>
#include <memory>
#include <tuple>
#include <type_traits>
#include <vector>

#include "compaction.hh"
#include "context.h"
#include "header.h"
#include "hf/hf.hh"
#include "layout.h"
#include "type_traits.hh"

namespace cusz {

// extra helper
struct CompressorHelper {
    static int autotune_coarse_parvle(cusz_context* ctx);
};

template <class Combination>
class Compressor {
   public:
    /* removed, leaving vacancy in pipeline
     * using FallbackCodec = typename Combination::FallbackCodec;
     * using Spcodec = typename Combination::Spcodec;
     */

    using Codec = typename Combination::Codec;
    using BYTE  = uint8_t;

    using T      = typename Combination::DATA;
    using FP     = typename Combination::FP;
    using E      = typename Combination::ERRCTRL;
    using H      = typename Codec::Encoded;
    using M      = uint32_t;
    using Header = cusz_header;

    using TimeRecord   = std::vector<std::tuple<const char*, double>>;
    using timerecord_t = TimeRecord*;

   private:
    // profiling
    TimeRecord timerecord;

    // header
    Header header;

    // external codec that has complex internals
    Codec* codec;

    float time_pred, time_hist, time_sp;

    // sizes
    dim3   len3;
    size_t len;
    int    splen;

    // configs
    float outlier_density{0.2};

    // buffers
    pszmem_cxx<BYTE>*     compressed;
    pszmem_cxx<E>*        errctrl;
    pszmem_cxx<T>*        outlier;
    pszmem_cxx<uint32_t>* freq;
    pszmem_cxx<T>*        spval;
    pszmem_cxx<uint32_t>* spidx;

   public:
    Compressor() = default;
    ~Compressor();

    // public methods
    Compressor* init(cusz_context* config, bool dbg_print = false);
    Compressor* init(cusz_header* config, bool dbg_print = false);
    Compressor* compress(cusz_context*, T*, BYTE*&, size_t&, cudaStream_t = nullptr, bool = false);
    Compressor* decompress(cusz_header*, BYTE*, T*, cudaStream_t = nullptr, bool = true);
    Compressor* clear_buffer();
    Compressor* dump_intermediate(std::vector<pszmem_dump>, char const*);
    Compressor* destroy();

    // getter
    Compressor* export_header(cusz_header&);
    Compressor* export_header(cusz_header*);
    Compressor* export_timerecord(TimeRecord*);

   private:
    // helper
    template <class CONFIG>
    Compressor* init_detail(CONFIG*, bool);
    Compressor* collect_comp_time();
    Compressor* collect_decomp_time();
    Compressor* merge_subfiles(BYTE*, size_t, T*, M*, size_t, cudaStream_t);
};

}  // namespace cusz

#endif
