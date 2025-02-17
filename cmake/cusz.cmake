add_compile_definitions(PSZ_USE_CUDA)

find_package(CUDAToolkit REQUIRED)

include(GNUInstallDirs)
include(CTest)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/cusz_version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/cusz_version.h)

add_library(pszcompile_settings INTERFACE)

target_compile_definitions(
  pszcompile_settings
  INTERFACE $<$<COMPILE_LANG_AND_ID:CUDA,Clang>:__STRICT_ANSI__>)
target_compile_options(
  pszcompile_settings
  INTERFACE $<$<COMPILE_LANG_AND_ID:CUDA,NVIDIA>:--extended-lambda
            --expt-relaxed-constexpr -Wno-deprecated-declarations>)
target_compile_features(pszcompile_settings INTERFACE cxx_std_14 cuda_std_14)
target_include_directories(
  pszcompile_settings
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cusz>)

option(PSZ_RESEARCH_HUFFBK_CUDA
       "build research artifacts: create Huffman codebook on GPU" OFF)
option(PSZ_REACTIVATE_THRUSTGPU
       "build previously thrust implemented functions" OFF)

# seprate later
add_library(psztime src/utils/timer_cpu.cc src/utils/timer_gpu.cc)
target_link_libraries(psztime PUBLIC pszcompile_settings CUDA::cudart)

add_library(pszstat_ser src/stat/compare_cpu.cc)
target_link_libraries(pszstat_ser PUBLIC pszcompile_settings)

if(PSZ_REACTIVATE_THRUSTGPU)
  add_compile_definitions(REACTIVATE_THRUSTGPU)
  add_library(
    pszstat_cu
    src/stat/extrema.cu
    src/stat/cmpg1_4.cu
    src/stat/cmpg1_5.cu
    src/stat/cmpg2.cu
    src/stat/cmpg3.cu
    src/stat/cmpg4_1.cu
    src/stat/cmpg4_2.cu
    src/stat/cmpg5_1.cu
    src/stat/cmpg5_2.cu)
else()
  add_library(
    pszstat_cu src/stat/extrema.cu src/stat/cmpg2.cu src/stat/cmpg4_1.cu
               src/stat/cmpg4_2.cu src/stat/cmpg5_1.cu src/stat/cmpg5_2.cu)
endif()
target_link_libraries(pszstat_cu PUBLIC pszcompile_settings)

# FUNC={core,api}, BACKEND={serial,cuda,...}
add_library(pszkernel_ser src/kernel/l23_ser.cc src/kernel/hist_ser.cc
                          src/kernel/histsp_ser.cc)
target_link_libraries(pszkernel_ser PUBLIC pszcompile_settings)

add_library(
  pszkernel_cu
  src/kernel/dryrun.cu
  src/kernel/lproto.cu
  src/kernel/l23.cu
  src/kernel/spline3.cu
  src/kernel/hist.cu
  src/kernel/histsp.cu
  src/kernel/l23r.cu)
target_link_libraries(pszkernel_cu PUBLIC pszcompile_settings)

add_library(pszmem src/mem/memseg.cc src/mem/memseg_cu.cc)
target_link_libraries(pszmem PUBLIC pszcompile_settings CUDA::cudart)

add_library(pszutils_ser src/utils/vis_stat.cc src/context.cc)
target_link_libraries(pszutils_ser PUBLIC pszcompile_settings)

add_library(pszspv_cu src/kernel/spv.cu)
target_link_libraries(pszspv_cu PUBLIC pszcompile_settings)

if(PSZ_RESEARCH_HUFFBK_CUDA)
  # define C/Cxx macro: https://stackoverflow.com/a/9017635
  add_compile_definitions(ENABLE_HUFFBK_GPU)
  add_library(pszhfbook_cu src/hf/hf_book1.cu src/hf/hf_book2.cu
                           src/hf/hf_sort.cu)
  target_link_libraries(pszhfbook_cu PUBLIC pszcompile_settings
                                            CUDA::cuda_driver)
  set_target_properties(pszhfbook_cu PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
  string(FIND "${CUDA_cuda_driver_LIBRARY}" "stub" CUDA_DRIVER_IS_STUB)
  if(NOT ${CUDA_DRIVER_IS_STUB} EQUAL -1)
    message(WARNING "the cuda driver is a stub!! adding --allow-shlib-undefined to fix downstream linking issues")
    target_link_options(pszhfbook_cu PUBLIC $<HOST_LINK:LINKER:--allow-shlib-undefined>)
  endif()
endif(PSZ_RESEARCH_HUFFBK_CUDA)
# cmake option on-and-off-cache: https://stackoverflow.com/a/48984477
# unset(PSZ_RESEARCH_HUFFBK_CUDA CACHE)

add_library(
  pszhfbook_ser src/hf/hf_bk_impl1.cc src/hf/hf_bk_impl2.cc
                src/hf/hf_bk_internal.cc src/hf/hf_bk.cc src/hf/hf_canon.cc)
target_link_libraries(pszhfbook_ser PUBLIC pszcompile_settings)

add_library(pszhf_cu src/hf/hf_obj.cu src/hf/hf_codec.cu)
if(PSZ_RESEARCH_HUFFBK_CUDA)
  target_link_libraries(pszhf_cu PUBLIC pszcompile_settings pszstat_cu
                                        pszhfbook_cu pszhfbook_ser)
else()
  target_link_libraries(pszhf_cu PUBLIC pszcompile_settings pszstat_cu
                                        pszhfbook_ser CUDA::cuda_driver)
endif(PSZ_RESEARCH_HUFFBK_CUDA)
# unset(PSZ_RESEARCH_HUFFBK_CUDA CACHE)

# [TODO] maybe a standalone libpszdbg
add_library(psz_comp src/compressor.cc src/log/sanitize.cc)
target_link_libraries(psz_comp PUBLIC pszcompile_settings pszkernel_cu
                                      pszstat_cu pszhf_cu CUDA::cudart)

add_library(cusz src/cusz_lib.cc)
target_link_libraries(cusz PUBLIC psz_comp pszhf_cu pszspv_cu pszstat_ser
                                  pszutils_ser pszmem)

add_executable(cusz-bin src/cli_cusz.cc)
target_link_libraries(cusz-bin PRIVATE cusz)
set_target_properties(cusz-bin PROPERTIES OUTPUT_NAME cusz)

# enable examples and testing
if(PSZ_BUILD_EXAMPLES)
  add_subdirectory(example)
endif()

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

# installation
install(TARGETS pszcompile_settings EXPORT CUSZTargets)

install(TARGETS pszkernel_ser EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS pszkernel_cu EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS pszstat_ser EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS pszstat_cu EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS pszmem EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS pszutils_ser EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS psztime EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS pszspv_cu EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS pszhfbook_ser EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS pszhf_cu EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS psz_comp EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS cusz EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS cusz-bin EXPORT CUSZTargets)
if(PSZ_RESEARCH_HUFFBK_CUDA)
  install(TARGETS pszhfbook_cu EXPORT CUSZTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif(PSZ_RESEARCH_HUFFBK_CUDA)

install(
  EXPORT CUSZTargets
  NAMESPACE CUSZ::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CUSZ/)
include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/CUSZConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/CUSZConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CUSZ)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/CUSZConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY AnyNewerVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/CUSZConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/CUSZConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CUSZ)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cusz)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/cusz_version.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cusz/)
