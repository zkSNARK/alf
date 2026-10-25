//
// kernels.h -- the per-instruction-set kernels, for dispatch.cpp to assemble into Backends.
// Declarations only (see simd.h on why nothing here may be inline).  Each group exists only
// when CMake compiled that backend's file (ALF_SIMD_* definitions).
//

#pragma once

#include "simd.h"

namespace alf::simd
{
#if defined(ALF_SIMD_X86)
  auto find_any_sse2(ByteSet const&, char const*, char const*, char const*) noexcept -> char const*;
  auto line_start_sse2(char const*, char const*) noexcept -> char const*;
  auto teddy_ssse3(TeddyTables const&, char const*, char const*, char const*) noexcept -> char const*;
  auto find_any_avx2(ByteSet const&, char const*, char const*, char const*) noexcept -> char const*;
  auto teddy_avx2(TeddyTables const&, char const*, char const*, char const*) noexcept -> char const*;
  auto line_start_avx2(char const*, char const*) noexcept -> char const*;
#endif
#if defined(ALF_SIMD_AVX512)
  auto find_any_avx512(ByteSet const&, char const*, char const*, char const*) noexcept -> char const*;
  auto teddy_avx512(TeddyTables const&, char const*, char const*, char const*) noexcept -> char const*;
  auto line_start_avx512(char const*, char const*) noexcept -> char const*;
#endif
#if defined(ALF_SIMD_NEON)
  auto find_any_neon(ByteSet const&, char const*, char const*, char const*) noexcept -> char const*;
  auto teddy_neon(TeddyTables const&, char const*, char const*, char const*) noexcept -> char const*;
  auto line_start_neon(char const*, char const*) noexcept -> char const*;
#endif
#if defined(ALF_SIMD_SVE)
  auto find_any_sve(ByteSet const&, char const*, char const*, char const*) noexcept -> char const*;
  auto teddy_sve(TeddyTables const&, char const*, char const*, char const*) noexcept -> char const*;
  auto sve_vector_bytes() noexcept -> unsigned;  // call only on a CPU with SVE
#endif
}
