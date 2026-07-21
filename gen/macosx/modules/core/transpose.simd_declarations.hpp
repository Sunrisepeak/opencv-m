#define CV_CPU_SIMD_FILENAME "modules/core/src/transpose.simd.hpp"
#define CV_CPU_DISPATCH_MODE NEON
#include "opencv2/core/private/cv_cpu_include_simd_declarations.hpp"

#define CV_CPU_DISPATCH_MODES_ALL NEON, BASELINE

#undef CV_CPU_SIMD_FILENAME
