//
// dispatch.cpp -- which backends were built, which run on this CPU, and which one is used.
//
// Compiled with the baseline flags only: the feature checks must never themselves use the
// instructions they check for.  The backends' kernels live in their own files (see simd.h).
//

#include "kernels.h"
#include "simd.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#if defined(ALF_SIMD_SVE) && defined(__linux__)
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

#if defined(ALF_SIMD_X86)
    // __builtin_cpu_supports also checks that the OS saves the wider registers (XGETBV).
    auto has_sse2() noexcept -> bool { return __builtin_cpu_supports("sse2"); }
    auto has_ssse3() noexcept -> bool { return __builtin_cpu_supports("ssse3"); }
    auto has_avx2() noexcept -> bool { return __builtin_cpu_supports("avx2"); }
#endif
#if defined(ALF_SIMD_AVX512)
    auto has_avx512() noexcept -> bool { return __builtin_cpu_supports("avx512f") and __builtin_cpu_supports("avx512bw"); }
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

    // Best first.  "preferred" is what automatic selection uses; any backend that runs here can
    // still be forced with ALF_SIMD.
#if defined(ALF_SIMD_AVX512)
    Backend const avx512{ "avx512", has_avx512, no, find_any_avx512, teddy_avx512, line_start_avx512, true };
#endif
#if defined(ALF_SIMD_X86)
    Backend const avx2{ "avx2", has_avx2, has_avx2, find_any_avx2, teddy_avx2, line_start_avx2, true };
    Backend const ssse3{ "ssse3", has_ssse3, has_ssse3, find_any_sse2, teddy_ssse3, line_start_sse2, true };
    Backend const sse2{ "sse2", has_sse2, has_sse2, find_any_sse2, teddy_scalar, line_start_sse2, false };
#endif
#if defined(ALF_SIMD_SVE)
    Backend const sve{ "sve", has_sve, sve_wider, find_any_sve, teddy_sve, line_start_neon, true };
#endif
#if defined(ALF_SIMD_NEON)
    Backend const neon{ "neon", yes, yes, find_any_neon, teddy_neon, line_start_neon, true };
#endif
    Backend const scalar{ "scalar", yes, yes, find_any_scalar, teddy_scalar, line_start_scalar, false };

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
