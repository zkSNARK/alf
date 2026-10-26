//
// dispatch.cpp -- which backends were built, which run on this CPU, and which one is used.
//
// Compiled with the baseline flags only: the feature checks must never themselves use the
// instructions they check for.  The backends' kernels live in their own files (see simd.h).
//

#include "kernels.h"
#include "simd.h"

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#if (defined(ALF_SIMD_SVE) || defined(ALF_SIMD_RVV) || defined(ALF_SIMD_VSX)) && defined(__linux__)
#include <sys/auxv.h>
#endif

namespace alf::simd
{
  namespace
  {
    auto yes() noexcept -> bool { return true; }
#if defined(ALF_SIMD_AVX512)
    auto no() noexcept -> bool { return false; }
#endif

#if defined(ALF_SIMD_X86) && defined(_MSC_VER) && !defined(__clang__)
    // MSVC: read CPUID directly.  AVX/AVX-512 also need the OS to save the wider registers on a
    // context switch, which XGETBV reports (bits 1-2: SSE/AVX state; 5-7: AVX-512 state).
    struct X86Features
    {
      bool sse2, ssse3, avx2, avx512;
    };
    auto x86() noexcept -> X86Features const&
    {
      static X86Features const f = [] {
        int r[4];
        __cpuid(r, 0);
        int const max_leaf = r[0];
        __cpuid(r, 1);
        bool const sse2 = (r[3] & (1 << 26)) != 0;
        bool const ssse3 = (r[2] & (1 << 9)) != 0;
        bool const osxsave = (r[2] & (1 << 27)) != 0;
        bool const avx = (r[2] & (1 << 28)) != 0;
        unsigned long long const xcr0 = osxsave ? _xgetbv(0) : 0;
        bool avx2 = false, avx512f = false, avx512bw = false;
        if (max_leaf >= 7) {
          __cpuidex(r, 7, 0);
          avx2 = (r[1] & (1 << 5)) != 0;
          avx512f = (r[1] & (1 << 16)) != 0;
          avx512bw = (r[1] & (1 << 30)) != 0;
        }
        bool const ymm_saved = (xcr0 & 0x06) == 0x06, zmm_saved = (xcr0 & 0xE6) == 0xE6;
        return X86Features{ sse2, ssse3, avx and avx2 and ymm_saved, avx512f and avx512bw and zmm_saved };
      }();
      return f;
    }
    auto has_sse2() noexcept -> bool { return x86().sse2; }
    auto has_ssse3() noexcept -> bool { return x86().ssse3; }
    auto has_avx2() noexcept -> bool { return x86().avx2; }
#if defined(ALF_SIMD_AVX512)
    auto has_avx512() noexcept -> bool { return x86().avx512; }
#endif
#elif defined(ALF_SIMD_X86)
    // __builtin_cpu_supports also checks that the OS saves the wider registers (XGETBV).
    auto has_sse2() noexcept -> bool { return __builtin_cpu_supports("sse2"); }
    auto has_ssse3() noexcept -> bool { return __builtin_cpu_supports("ssse3"); }
    auto has_avx2() noexcept -> bool { return __builtin_cpu_supports("avx2"); }
#if defined(ALF_SIMD_AVX512)
    auto has_avx512() noexcept -> bool { return __builtin_cpu_supports("avx512f") and __builtin_cpu_supports("avx512bw"); }
#endif
#endif
#if defined(ALF_SIMD_SVE)
    auto has_sve() noexcept -> bool
    {
#if defined(__linux__) && defined(AT_HWCAP)
      return (getauxval(AT_HWCAP) & (1ul << 22)) != 0;  // HWCAP_SVE
#else
      return false;  // macOS: no Apple CPU has SVE so far
#endif
    }
    // Prefer SVE over NEON only where its vectors are wider than NEON's 16 bytes.
    auto sve_wider() noexcept -> bool { return has_sve() and sve_vector_bytes() > 16; }
#endif

#if defined(ALF_SIMD_RVV)
    auto has_rvv() noexcept -> bool
    {
#if defined(__linux__) && defined(AT_HWCAP)
      return (getauxval(AT_HWCAP) & (1ul << ('V' - 'A'))) != 0;  // COMPAT_HWCAP_ISA_V
#else
      return false;
#endif
    }
#endif
#if defined(ALF_SIMD_VSX)
    auto has_vsx() noexcept -> bool
    {
#if defined(__linux__) && defined(AT_HWCAP)
      return (getauxval(AT_HWCAP) & 0x00000080ul) != 0;  // PPC_FEATURE_HAS_VSX
#else
      return true;  // little-endian POWER requires POWER8, which has VSX
#endif
    }
#endif

    // Best first.  "preferred" is what automatic selection uses; any backend that runs here can
    // still be forced with ALF_SIMD.
#if defined(ALF_SIMD_AVX512)
    Backend const avx512{ "avx512", has_avx512, no, find_any_avx512, teddy_avx512, line_start_avx512, true, find_pair_avx2 };
#endif
#if defined(ALF_SIMD_X86)
    Backend const avx2{ "avx2", has_avx2, has_avx2, find_any_avx2, teddy_avx2, line_start_avx2, true, find_pair_avx2 };
    Backend const ssse3{ "ssse3", has_ssse3, has_ssse3, find_any_sse2, teddy_ssse3, line_start_sse2, true, find_pair_sse2 };
    Backend const sse2{ "sse2", has_sse2, has_sse2, find_any_sse2, teddy_scalar, line_start_sse2, false, find_pair_sse2 };
#endif
#if defined(ALF_SIMD_WASM)
    Backend const wasm{ "wasm", yes, yes, find_any_wasm, teddy_wasm, line_start_wasm, true, find_pair_wasm };
#endif
#if defined(ALF_SIMD_RVV)
    Backend const rvv{ "rvv", has_rvv, has_rvv, find_any_rvv, teddy_rvv, line_start_rvv, true, find_pair_rvv };
#endif
#if defined(ALF_SIMD_VSX)
    Backend const vsx{ "vsx", has_vsx, has_vsx, find_any_vsx, teddy_vsx, line_start_vsx, true, find_pair_vsx };
#endif
#if defined(ALF_SIMD_SVE)
    Backend const sve{ "sve", has_sve, sve_wider, find_any_sve, teddy_sve, line_start_neon, true, find_pair_neon };
#endif
#if defined(ALF_SIMD_NEON)
    Backend const neon{ "neon", yes, yes, find_any_neon, teddy_neon, line_start_neon, true, find_pair_neon };
#endif
    Backend const scalar{ "scalar", yes, yes, find_any_scalar, teddy_scalar, line_start_scalar, false, find_pair_scalar };

    Backend const* const all[] = {
#if defined(ALF_SIMD_AVX512)
      &avx512,
#endif
#if defined(ALF_SIMD_X86)
      &avx2, &ssse3, &sse2,
#endif
#if defined(ALF_SIMD_SVE)
      &sve,
#endif
#if defined(ALF_SIMD_RVV)
      &rvv,
#endif
#if defined(ALF_SIMD_WASM)
      &wasm,
#endif
#if defined(ALF_SIMD_VSX)
      &vsx,
#endif
#if defined(ALF_SIMD_NEON)
      &neon,
#endif
      &scalar,
    };

    auto choose() -> Backend const&
    {
      if (char const* want = std::getenv("ALF_SIMD"); want and *want) {
        if (Backend const* b = by_name(want); b and b->runs_here()) {
          return *b;
        }
        std::fprintf(stderr, "alf: ALF_SIMD=%s is not available on this machine; choosing automatically\n", want);
      }
      for (Backend const* b : all) {
        if (b->runs_here() and b->preferred()) {
          return *b;
        }
      }
      return scalar;
    }

    std::atomic<Backend const*> forced{ nullptr };
  }

  auto active() -> Backend const&
  {
    if (Backend const* f = forced.load(std::memory_order_acquire)) {
      return *f;
    }
    static Backend const& chosen = choose();
    return chosen;
  }

  auto compiled() -> std::span<Backend const* const>
  {
    return all;
  }

  auto by_name(std::string_view name) -> Backend const*
  {
    for (Backend const* b : all) {
      if (name == b->name) {
        return b;
      }
    }
    return nullptr;
  }

  void use(Backend const& b)
  {
    forced.store(&b, std::memory_order_release);
  }
}
