// API-surface smoke tests through the module layer ONLY (no textual OpenCV
// headers in this TU): the imported names must expose the OpenCV C++ API
// unchanged, and the constant macros must be reachable as cv:: constexpr.
#include <gtest/gtest.h>

#include <vector>

import opencv.cv;

TEST(ApiSurface, VersionMatchesPinnedRelease) {
    EXPECT_EQ(cv::getVersionString(), "5.0.0");
    EXPECT_EQ(cv::getVersionMajor(), 5);
}

TEST(ApiSurface, CoreTypesAndConstants) {
    static_assert(cv::CV_MAKETYPE(cv::CV_8U, 3) == cv::CV_8UC3);
    cv::Mat m(4, 4, cv::CV_8UC3, cv::Scalar(1, 2, 3));
    EXPECT_EQ(m.rows, 4);
    EXPECT_EQ(m.type(), cv::CV_8UC3);
    cv::Point2f p(1.f, 2.f);
    cv::Rect r(0, 0, 4, 4);
    EXPECT_TRUE(r.contains(cv::Point(1, 1)));
    EXPECT_FLOAT_EQ(p.x, 1.f);
}

TEST(ApiSurface, ImgprocEnumsAndOps) {
    cv::Mat src(8, 8, cv::CV_8UC3, cv::Scalar(10, 20, 30)), dst, gray;
    cv::resize(src, dst, {16, 16}, 0, 0, cv::INTER_CUBIC);
    cv::cvtColor(dst, gray, cv::COLOR_BGR2GRAY);
    EXPECT_EQ(gray.size(), cv::Size(16, 16));
    EXPECT_EQ(gray.channels(), 1);
}

TEST(ApiSurface, VideoioRegistry) {
    EXPECT_FALSE(cv::videoio_registry::getBackends().empty());
}
