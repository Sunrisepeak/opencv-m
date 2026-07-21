// opencv.core — C++23 module over OpenCV 5 core. The C++ API is unchanged:
// every exported name is the upstream entity itself (`export using`), only
// the header inclusion goes away. Function-like macros (CV_Assert, CV_Error,
// …) cannot cross a module boundary — include <opencv-m/macros.hpp> BEFORE
// importing when you need them.
module;
#include <cstdarg>   // va_list/va_start/va_copy/va_end — cv::format printf reimpl (core_fns.inc)
#include <cstdio>    // vsnprintf/fputs/FILE/stdout — cv::format / cv::print reimpls (core_fns.inc)
#include <opencv2/core.hpp>
#include <opencv2/core/async.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/core/quaternion.hpp>
#include <opencv2/core/softfloat.hpp>
#include <opencv2/core/version.hpp>

export module opencv.core;

#include "gen_exports/core.inc"
#include "core_ops.inc"
#include "matx_ops.inc"
#include "core_fns.inc"

// ── constant macros re-homed as cv:: constexpr (original spellings) ──────
// CV_8U / CV_8UC3 / CV_PI … are object-like macros upstream; a module cannot
// export a macro, so the module exports `cv::CV_8U`-style constexpr values
// carrying the exact upstream spelling after the namespace. The raw global
// macros remain available via the <opencv-m/macros.hpp> side header.
namespace opencv_m_detail {
inline constexpr int    k_CV_8U  = CV_8U,  k_CV_8S  = CV_8S,  k_CV_16U = CV_16U,
                        k_CV_16S = CV_16S, k_CV_32S = CV_32S, k_CV_32F = CV_32F,
                        k_CV_64F = CV_64F, k_CV_16F = CV_16F;
inline constexpr int    k_CV_CN_MAX = CV_CN_MAX, k_CV_MAT_CN_MASK = CV_MAT_CN_MASK,
                        k_CV_DEPTH_MAX = CV_DEPTH_MAX;
inline constexpr double k_CV_PI = CV_PI, k_CV_2PI = CV_2PI, k_CV_LOG2 = CV_LOG2;
constexpr int k_maketype(int depth, int cn) { return CV_MAKETYPE(depth, cn); }
#define OPENCV_M_C(base) \
    inline constexpr int k_##base##C1 = base##C1, k_##base##C2 = base##C2, \
                         k_##base##C3 = base##C3, k_##base##C4 = base##C4;
OPENCV_M_C(CV_8U) OPENCV_M_C(CV_8S) OPENCV_M_C(CV_16U) OPENCV_M_C(CV_16S)
OPENCV_M_C(CV_32S) OPENCV_M_C(CV_32F) OPENCV_M_C(CV_64F) OPENCV_M_C(CV_16F)
#undef OPENCV_M_C
}

#undef CV_8U
#undef CV_8S
#undef CV_16U
#undef CV_16S
#undef CV_32S
#undef CV_32F
#undef CV_64F
#undef CV_16F
#undef CV_PI
#undef CV_2PI
#undef CV_LOG2
#undef CV_CN_MAX
#undef CV_MAT_CN_MASK
#undef CV_DEPTH_MAX
#undef CV_MAKETYPE
#undef CV_MAKE_TYPE
#undef CV_8UC1
#undef CV_8UC2
#undef CV_8UC3
#undef CV_8UC4
#undef CV_8SC1
#undef CV_8SC2
#undef CV_8SC3
#undef CV_8SC4
#undef CV_16UC1
#undef CV_16UC2
#undef CV_16UC3
#undef CV_16UC4
#undef CV_16SC1
#undef CV_16SC2
#undef CV_16SC3
#undef CV_16SC4
#undef CV_32SC1
#undef CV_32SC2
#undef CV_32SC3
#undef CV_32SC4
#undef CV_32FC1
#undef CV_32FC2
#undef CV_32FC3
#undef CV_32FC4
#undef CV_64FC1
#undef CV_64FC2
#undef CV_64FC3
#undef CV_64FC4
#undef CV_16FC1
#undef CV_16FC2
#undef CV_16FC3
#undef CV_16FC4

export namespace cv {
inline constexpr int    CV_8U  = opencv_m_detail::k_CV_8U,
                        CV_8S  = opencv_m_detail::k_CV_8S,
                        CV_16U = opencv_m_detail::k_CV_16U,
                        CV_16S = opencv_m_detail::k_CV_16S,
                        CV_32S = opencv_m_detail::k_CV_32S,
                        CV_32F = opencv_m_detail::k_CV_32F,
                        CV_64F = opencv_m_detail::k_CV_64F,
                        CV_16F = opencv_m_detail::k_CV_16F;
inline constexpr int    CV_CN_MAX = opencv_m_detail::k_CV_CN_MAX,
                        CV_MAT_CN_MASK = opencv_m_detail::k_CV_MAT_CN_MASK,
                        CV_DEPTH_MAX = opencv_m_detail::k_CV_DEPTH_MAX;
inline constexpr double CV_PI = opencv_m_detail::k_CV_PI,
                        CV_2PI = opencv_m_detail::k_CV_2PI,
                        CV_LOG2 = opencv_m_detail::k_CV_LOG2;
constexpr int CV_MAKETYPE(int depth, int cn) { return opencv_m_detail::k_maketype(depth, cn); }
#define OPENCV_M_E(base) \
    inline constexpr int base##C1 = opencv_m_detail::k_##base##C1, \
                         base##C2 = opencv_m_detail::k_##base##C2, \
                         base##C3 = opencv_m_detail::k_##base##C3, \
                         base##C4 = opencv_m_detail::k_##base##C4;
OPENCV_M_E(CV_8U) OPENCV_M_E(CV_8S) OPENCV_M_E(CV_16U) OPENCV_M_E(CV_16S)
OPENCV_M_E(CV_32S) OPENCV_M_E(CV_32F) OPENCV_M_E(CV_64F) OPENCV_M_E(CV_16F)
#undef OPENCV_M_E
}

// ── global-scope helpers (outside namespace cv upstream) ────────────────
export using ::CpuFeatures;
export using ::CPU_MMX;
export using ::CPU_SSE;
export using ::CPU_SSE2;
export using ::CPU_SSE3;
export using ::CPU_SSSE3;
export using ::CPU_SSE4_1;
export using ::CPU_SSE4_2;
export using ::CPU_POPCNT;
export using ::CPU_FP16;
export using ::CPU_AVX;
export using ::CPU_AVX2;
export using ::CPU_FMA3;
export using ::CPU_AVX512_SKX;
export using ::CPU_NEON;
export using ::CPU_MAX_FEATURE;
