// The optional macros side header must coexist with the import in one TU.
// In such a mixed TU the RAW macro spellings are the ones to use (an active
// CV_8UC3 macro would mangle the cv::CV_8UC3 spelling — that constexpr
// surface is for module-only TUs, see api_surface_test.cpp).
#include <gtest/gtest.h>

// Canonical mixing order: textual includes BEFORE the import.
#include <opencv-m/macros.hpp>

import opencv.cv;

TEST(MacrosMix, AssertAndTypeMacros) {
    cv::Mat m(2, 2, CV_8UC1, cv::Scalar(7));
    CV_Assert(!m.empty());
    EXPECT_EQ(m.type(), CV_8UC1);
    EXPECT_EQ(CV_MAKETYPE(CV_8U, 3), CV_8UC3);
}

TEST(MacrosMix, VersionMacros) {
    EXPECT_EQ(CV_VERSION_MAJOR, 5);
    EXPECT_EQ(cv::getVersionMajor(), CV_VERSION_MAJOR);
}
