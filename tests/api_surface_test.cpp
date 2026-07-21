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
    EXPECT_TRUE(gray.size() == cv::Size(16, 16));  // see printer note below
    EXPECT_EQ(gray.channels(), 1);
}

TEST(ApiSurface, ReplacementValueOps) {
    // upstream declares these operators `static inline` (TU-local) — the
    // module ships self-contained replacements (src/core_ops.inc)
    // NOTE: cv-typed comparisons use EXPECT_TRUE(a == b), not EXPECT_EQ —
    // gtest's value-printer probes operator<< via ADL over the imported
    // module surface and that ADL walk segfaults linux clang 20/22 (frontend
    // bug, minimal repro archived; gcc is fine either way).
    EXPECT_TRUE(cv::Size(2, 3) != cv::Size(3, 2));
    EXPECT_TRUE(cv::Size(2, 3) == cv::Size(2, 3));
    EXPECT_TRUE(cv::Point(1, 2) + cv::Point(3, 4) == cv::Point(4, 6));
    EXPECT_TRUE(cv::Point2f(1.f, 2.f) * 2.f == cv::Point2f(2.f, 4.f));
    EXPECT_TRUE((cv::Rect(0, 0, 4, 4) & cv::Rect(2, 2, 4, 4)) == cv::Rect(2, 2, 2, 2));
    EXPECT_TRUE((cv::Rect(0, 0, 2, 2) | cv::Rect(2, 2, 2, 2)) == cv::Rect(0, 0, 4, 4));
    EXPECT_TRUE(cv::Range(1, 5) == cv::Range(1, 5));
    EXPECT_TRUE((cv::Range(1, 5) & cv::Range(3, 9)) == cv::Range(3, 5));
    EXPECT_EQ(cv::saturate_cast<unsigned char>(300), 255);
    EXPECT_EQ(cv::saturate_cast<unsigned char>(-5), 0);
    EXPECT_EQ(cv::saturate_cast<short>(1e9f), 32767);
}

TEST(ApiSurface, VideoioRegistry) {
    EXPECT_FALSE(cv::videoio_registry::getBackends().empty());
}

TEST(MatxOps, Algebra) {
    // R1 replacement surface: Matx/Vec namespace-scope operators.
    cv::Matx33f a(1, 2, 3, 4, 5, 6, 7, 8, 9);
    cv::Matx33f b = a + a;
    EXPECT_FLOAT_EQ(b(1, 1), 10.0f);
    cv::Matx33f c = b - a;
    EXPECT_FLOAT_EQ(c(2, 0), 7.0f);
    EXPECT_TRUE(c == a);
    EXPECT_FALSE(c != a);

    cv::Matx33f s = a * 2.0f;
    EXPECT_FLOAT_EQ(s(0, 2), 6.0f);
    cv::Matx33f neg = -a;
    EXPECT_FLOAT_EQ(neg(0, 0), -1.0f);

    // real matrix multiply
    cv::Matx22f m1(1, 2, 3, 4), m2(5, 6, 7, 8);
    cv::Matx22f mm = m1 * m2;
    EXPECT_FLOAT_EQ(mm(0, 0), 19.0f);
    EXPECT_FLOAT_EQ(mm(1, 1), 50.0f);

    // Matx * Vec -> Vec
    cv::Vec2f v(1, 1);
    cv::Vec2f mv = m1 * v;
    EXPECT_FLOAT_EQ(mv[0], 3.0f);
    EXPECT_FLOAT_EQ(mv[1], 7.0f);

    EXPECT_DOUBLE_EQ(cv::determinant(cv::Matx22d(3, 0, 0, 2)), 6.0);
    EXPECT_DOUBLE_EQ(cv::trace(m1), 5.0);
    EXPECT_NEAR(cv::norm(cv::Matx21f(3, 4)), 5.0, 1e-6);
}

TEST(MatxOps, VecAlgebra) {
    cv::Vec3f u(1, 2, 3), w(4, 5, 6);
    cv::Vec3f sum = u + w;
    EXPECT_FLOAT_EQ(sum[2], 9.0f);
    cv::Vec3f dif = w - u;
    EXPECT_FLOAT_EQ(dif[0], 3.0f);
    cv::Vec3f sc = u * 2.0f;
    EXPECT_FLOAT_EQ(sc[1], 4.0f);
    cv::Vec3f dv = w / 2.0f;
    EXPECT_FLOAT_EQ(dv[0], 2.0f);
    cv::Vec3f nv = -u;
    EXPECT_FLOAT_EQ(nv[2], -3.0f);
    u += w;
    EXPECT_FLOAT_EQ(u[0], 5.0f);

    // Vec4 quaternion-style product
    cv::Vec4d q1(1, 0, 0, 0), q2(0, 1, 0, 0);
    cv::Vec4d q = q1 * q2;
    EXPECT_DOUBLE_EQ(q[1], 1.0);

    // interop with Mat
    cv::Matx33f eye = cv::Matx33f::eye();
    cv::Mat em(eye);
    EXPECT_FLOAT_EQ(em.at<float>(1, 1), 1.0f);
}
