// The heavy mixed TU: textual value-type/Mat headers plus the import in one
// TU. Since v0.0.3 the replacement operators are deliberately less
// specialized than upstream's, so overload resolution picks upstream's
// header versions here — deterministically, with NO ambiguity. These are
// exactly the calls the v0.0.2 docs said to avoid in mixed TUs.
//
// NOTE the include level: <opencv2/core.hpp> itself cannot be mixed with
// the import on gcc16 — its redeclarations with DEFAULT ARGUMENTS
// (cv::SVD::compute) trip "conflicting default argument" when merged with
// the GMF's copy (compiler-side global-module merge limitation, unrelated
// to operators). types.hpp/mat.hpp carry the whole operator surface and
// merge cleanly.
#include <gtest/gtest.h>

// Canonical mixing order: textual includes BEFORE the import.
#include <opencv2/core/types.hpp>
#include <opencv2/core/mat.hpp>

import opencv.cv;

TEST(FullMix, ValueTypeOperators) {
    cv::Size s1(128, 128), s2(128, 128);
    EXPECT_TRUE(s1 == s2);
    EXPECT_FALSE(s1 != s2);

    cv::Point p1(1, 2), p2(3, 4);
    cv::Point ps = p1 + p2;
    EXPECT_EQ(ps.x, 4);
    EXPECT_EQ(ps.y, 6);
    cv::Point pm = p1 * 3;
    EXPECT_EQ(pm.x, 3);
    cv::Point pn = -p1;
    EXPECT_EQ(pn.y, -2);

    cv::Rect r1(0, 0, 10, 10), r2(5, 5, 10, 10);
    cv::Rect ri = r1 & r2;
    EXPECT_TRUE(ri == cv::Rect(5, 5, 5, 5));
    cv::Rect ru = r1 | r2;
    EXPECT_TRUE(ru == cv::Rect(0, 0, 15, 15));

    cv::Range g1(2, 8), g2(4, 12);
    EXPECT_TRUE((g1 & g2) == cv::Range(4, 8));

    cv::Scalar c1(1, 2, 3, 4), c2(1, 2, 3, 4);
    EXPECT_TRUE(c1 == c2);
}

TEST(FullMix, SaturateCast) {
    EXPECT_EQ(cv::saturate_cast<unsigned char>(300), 255);
    EXPECT_EQ(cv::saturate_cast<unsigned char>(-5), 0);
    EXPECT_EQ(cv::saturate_cast<short>(1e9), 32767);
    EXPECT_EQ(cv::saturate_cast<unsigned char>(254.6f), 255);
}

TEST(FullMix, MatOperators) {
    cv::Mat a = cv::Mat::ones(3, 3, CV_32F), b = cv::Mat::ones(3, 3, CV_32F);
    cv::Mat sum = a + b;
    EXPECT_FLOAT_EQ(sum.at<float>(1, 1), 2.0f);
    cv::Mat scaled = a * 4.0;
    EXPECT_FLOAT_EQ(scaled.at<float>(0, 0), 4.0f);
    cv::Mat cmp = a != b;
    EXPECT_EQ(cv::countNonZero(cmp), 0);
}

TEST(FullMix, MatxOperators) {
    // constrained replacements win here too (subsumption tiebreak) — and
    // being module entities they always link, unlike the header's
    // static-inline instantiations whose home gcc16 picks arbitrarily.
    cv::Matx22f m1(1, 2, 3, 4), m2(5, 6, 7, 8);
    cv::Matx22f mm = m1 * m2;
    EXPECT_FLOAT_EQ(mm(0, 0), 19.0f);
    cv::Vec2f mv = m1 * cv::Vec2f(1, 1);
    EXPECT_FLOAT_EQ(mv[1], 7.0f);
    cv::Vec3f u(1, 2, 3);
    cv::Vec3f s = (u + u) * 2.0f;
    EXPECT_FLOAT_EQ(s[2], 12.0f);
    EXPECT_DOUBLE_EQ(cv::determinant(cv::Matx22d(3, 0, 0, 2)), 6.0);
}
