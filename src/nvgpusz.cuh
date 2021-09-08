#ifndef CUSZ_NVGPUSZ_CUH
#define CUSZ_NVGPUSZ_CUH

/**
 * @file nvgpusz.cuh
 * @author Jiannan Tian
 * @brief Workflow of cuSZ (header).
 * @version 0.3
 * @date 2021-07-12
 * (created) 2020-02-12 (rev.1) 2020-09-20 (rev.2) 2021-07-12; (rev.3) 2021-09-06
 *
 * @copyright (C) 2020 by Washington State University, The University of Alabama, Argonne National Laboratory
 * See LICENSE in top-level directory
 *
 */

#include <cxxabi.h>
#include <iostream>

#include "argparse.hh"
#include "datapack.hh"
#include "pack.hh"
#include "type_trait.hh"
#include "utils.hh"
#include "wrapper/interp_spline.h"

using namespace std;

template <bool If_FP, int DataByte, int QuantByte, int HuffByte>
void cusz_compress(
    argpack*,
    struct PartialData<typename DataTrait<If_FP, DataByte>::Data>*,
    dim3,
    cusz_header* mp,
    unsigned int = 1);

template <bool If_FP, int DataByte, int QuantByte, int HuffByte>
void cusz_decompress(argpack*, cusz_header* mp);

template <typename Data, typename Quant, typename Huff, typename FP>
class Compressor {
   private:
    static const auto TYPE_BITCOUNT = sizeof(Huff) * 8;

    unsigned int tune_deflate_chunksize(size_t len);

    void report_compression_time(size_t len, float lossy, float outlier, float hist, float book, float lossless);

    void export_codebook(Huff* d_book, const string& basename, size_t dict_size);

   public:
    Spline3<Data*, Quant*, float>* spline3;

    void register_spline3(Spline3<Data*, Quant*, float>* _spline3) { spline3 = _spline3; }
    struct {
        unsigned int data, quant, anchor;
        int          nnz_outlier;  // TODO modify the type correspondingly
        unsigned int dict_size;
    } length;

    int ndim;

    struct {
        int    radius;
        double eb;
        FP     ebx2, ebx2_r, eb_r;
    } config;

    struct {
        float lossy, outlier, hist, book, lossless;
    } time;

    struct {
        struct {
            size_t num_bits, num_uints, revbook_nbyte;
        } meta;

        struct {
            size_t *h_counts, *d_counts;
            Huff *  h_bitstream, *d_bitstream, *d_encspace;
        } array;
    } huffman;

    // huffman arrays

    // context
    argpack* ctx;

    unsigned int get_revbook_nbyte() { return sizeof(Huff) * (2 * TYPE_BITCOUNT) + sizeof(Quant) * length.dict_size; }

    Compressor(argpack* _ap, unsigned int _data_len, double _eb);

    void lorenzo_dryrun(struct PartialData<Data>* in_data);

    Compressor& predict_quantize(
        struct PartialData<Data>*  data,
        dim3                       xyz,
        struct PartialData<Data>*  anchor,
        struct PartialData<Quant>* quant);

    Compressor& gather_outlier(struct PartialData<Data>* in_data);

    Compressor& get_freq_and_codebook(
        struct PartialData<Quant>*        quant,
        struct PartialData<unsigned int>* freq,
        struct PartialData<Huff>*         book,
        struct PartialData<uint8_t>*      revbook);

    Compressor& analyze_compressibility(struct PartialData<unsigned int>* freq, struct PartialData<Huff>* book);

    Compressor& internal_eval_try_export_book(struct PartialData<Huff>* book);

    Compressor& internal_eval_try_export_quant(struct PartialData<Quant>* quant);

    // TODO make it return *this
    void try_skip_huffman(struct PartialData<Quant>* quant);

    Compressor& try_report_time();

    Compressor& export_revbook(struct PartialData<uint8_t>* revbook);

    Compressor& huffman_encode(struct PartialData<Quant>* quant, struct PartialData<Huff>* book);

    Compressor& pack_metadata(cusz_header* mp);
};

template <typename Data, typename Quant, typename Huff, typename FP>
class Decompressor {
   private:
    void report_decompression_time(size_t len, float lossy, float outlier, float lossless);

    void unpack_metadata(cusz_header* mp, argpack* ap);

    Spline3<Data*, Quant*, FP>* spline3;

   public:
    void register_spline3(Spline3<Data*, Quant*, float>* _spline3) { spline3 = _spline3; }

    size_t archive_bytes;
    struct {
        float lossy, outlier, lossless;
    } time;
    struct {
        unsigned int data, quant, anchor;
        int          nnz_outlier;  // TODO modify the type correspondingly
        unsigned int dict_size;
    } length;

    struct {
        int    radius;
        double eb;
        FP     ebx2, ebx2_r, eb_r;
    } config;

    size_t m, mxm;

    struct {
        struct {
            size_t num_bits, num_uints, revbook_nbyte;
        } meta;
    } huffman;

    argpack* ctx;

    Decompressor(cusz_header* _mp, argpack* _ap);

    Decompressor& huffman_decode(struct PartialData<Quant>* quant);

    Decompressor& scatter_outlier(Data* outlier);

    Decompressor& reversed_predict_quantize(Data* xdata, dim3 xyz, Data* anchor, Quant* quant);

    Decompressor& calculate_archive_nbyte();

    Decompressor& try_report_time();

    Decompressor& try_compare(Data* xdata);

    Decompressor& try_write2disk(Data* host_xdata);
};

template <bool If_FP, int DataByte, int QuantByte, int HuffByte>
void cusz_compress(
    argpack*                                                       ctx,
    struct PartialData<typename DataTrait<If_FP, DataByte>::Data>* in_data,
    dim3                                                           xyz,
    cusz_header*                                                   header,
    unsigned int                                                   optional_w);

template <bool If_FP, int DataByte, int QuantByte, int HuffByte>
void cusz_decompress(argpack* ctx, cusz_header* header);

#endif
