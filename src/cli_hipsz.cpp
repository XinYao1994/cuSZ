/**
 * @file cli_prog_hip.cpp
 * @author Jiannan Tian
 * @brief CLI program of hipSZ.
 * @version 0.1
 * @date 2020-09-20
 * (created) 2019-12-30 (rev) 2022-02-20
 *
 * @copyright (C) 2020 by Washington State University, The University of Alabama, Argonne National Laboratory
 * See LICENSE in top-level directory
 *
 */

#include "pipeline/cli.inl"

int main(int argc, char** argv)
{
    // auto ctx = new cusz_context(argc, argv);
    auto ctx = new cusz_context;
    pszctx_create_from_argv(ctx, argc, argv);

    if (ctx->verbose) {
        Diagnostics::GetMachineProperties();
        GpuDiagnostics::GetDeviceProperty();
    }

    cusz::CLI<float> cusz_cli;
    cusz_cli.dispatch(ctx);
}
