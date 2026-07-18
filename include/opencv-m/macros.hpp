// opencv-m/macros.hpp — optional side header for OpenCV's macro surface.
//
// Modules cannot export macros. Everything with linkage is re-exported by the
// opencv.* modules; this textual header covers the rest: CV_Assert / CV_Error
// / CV_DbgAssert, the CV_8UC(n)-style *function-like* type macros, version
// macros, and the global-namespace inline helpers (cvRound, cvFloor, …).
//
// IMPORTANT: include this header BEFORE any `import opencv.*;` in the same
// translation unit (include-after-import is ill-formed for headers that also
// declare entities attached to the global module — same rule as ffmpeg-m's
// macros.h).
#pragma once

#include <opencv2/core/base.hpp>
#include <opencv2/core/check.hpp>
#include <opencv2/core/cvdef.h>
#include <opencv2/core/version.hpp>
