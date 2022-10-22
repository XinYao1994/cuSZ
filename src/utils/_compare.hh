/**
 * @file _compare.hh
 * @author Jiannan Tian
 * @brief
 * @version 0.3
 * @date 2022-10-08
 *
 * (C) 2022 by Indiana University, Argonne National Laboratory
 *
 */

#ifndef C0E747B4_066F_4B04_A3D2_00E1A3B7D682
#define C0E747B4_066F_4B04_A3D2_00E1A3B7D682

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include "cusz/type.h"

namespace gpusz {
namespace detail {

template <typename T>
bool cppstd_identical(T* d1, T* d2, size_t const len)
{
    return std::equal(d1, d1 + len, d2);
}

template <typename T>
bool cppstd_error_bounded(T* a, T* b, size_t const len, double const eb, size_t* first_faulty_idx = nullptr)
{
    for (size_t i = 0; i < len; i++) {
        if (fabs(a[i] - b[i]) > eb) {
            if (first_faulty_idx) *first_faulty_idx = i;
            return false;
        }
    }
    return true;
}

template <typename T>
void cppstd_assess_quality(cusz_stats* s, T* xdata, T* odata, size_t const len)
{
    double max_odata = odata[0], min_odata = odata[0];
    double max_xdata = xdata[0], min_xdata = xdata[0];
    double max_abserr = max_abserr = fabs(xdata[0] - odata[0]);

    double sum_0 = 0, sum_x = 0;
    for (size_t i = 0; i < len; i++) sum_0 += odata[i], sum_x += xdata[i];

    double mean_odata = sum_0 / len, mean_xdata = sum_x / len;
    double sum_var_odata = 0, sum_var_xdata = 0, sum_err2 = 0, sum_corr = 0, rel_abserr = 0;

    double max_pwrrel_abserr = 0;
    size_t max_abserr_index  = 0;
    for (size_t i = 0; i < len; i++) {
        max_odata = max_odata < odata[i] ? odata[i] : max_odata;
        min_odata = min_odata > odata[i] ? odata[i] : min_odata;

        max_xdata = max_xdata < odata[i] ? odata[i] : max_xdata;
        min_xdata = min_xdata > xdata[i] ? xdata[i] : min_xdata;

        float abserr = fabs(xdata[i] - odata[i]);
        if (odata[i] != 0) {
            rel_abserr        = abserr / fabs(odata[i]);
            max_pwrrel_abserr = max_pwrrel_abserr < rel_abserr ? rel_abserr : max_pwrrel_abserr;
        }
        max_abserr_index = max_abserr < abserr ? i : max_abserr_index;
        max_abserr       = max_abserr < abserr ? abserr : max_abserr;
        sum_corr += (odata[i] - mean_odata) * (xdata[i] - mean_xdata);
        sum_var_odata += (odata[i] - mean_odata) * (odata[i] - mean_odata);
        sum_var_xdata += (xdata[i] - mean_xdata) * (xdata[i] - mean_xdata);
        sum_err2 += abserr * abserr;
    }
    double std_odata = sqrt(sum_var_odata / len);
    double std_xdata = sqrt(sum_var_xdata / len);
    double ee        = sum_corr / len;

    s->len = len;

    s->odata.max = max_odata;
    s->odata.min = min_odata;
    s->odata.rng = max_odata - min_odata;
    s->odata.std = std_odata;

    s->xdata.max = max_xdata;
    s->xdata.min = min_xdata;
    s->xdata.rng = max_xdata - min_xdata;
    s->xdata.std = std_xdata;

    s->max_err.idx    = max_abserr_index;
    s->max_err.abs    = max_abserr;
    s->max_err.rel    = max_abserr / s->odata.rng;
    s->max_err.pwrrel = max_pwrrel_abserr;

    s->reduced.coeff = ee / std_odata / std_xdata;
    s->reduced.MSE   = sum_err2 / len;
    s->reduced.NRMSE = sqrt(s->reduced.MSE) / s->odata.rng;
    s->reduced.PSNR  = 20 * log10(s->odata.rng) - 10 * log10(s->reduced.MSE);
}

}  // namespace detail
}  // namespace gpusz

#endif /* C0E747B4_066F_4B04_A3D2_00E1A3B7D682 */
