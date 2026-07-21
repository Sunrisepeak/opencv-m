// unifont feature test — through the module layer (import opencv.cv). The
// "uni" builtin FontFace only exists when the library is built with
// HAVE_UNIFONT (build.mcpp embeds the vendored font), so its usability IS
// the feature probe: CJK rendering must produce real ink. Only built with
// `--features unifont` (no new module surface); without it this TU has no tests.
#include <gtest/gtest.h>
#if defined(MCPP_FEATURE_UNIFONT)
import opencv.cv;

TEST(UnifontModule, CjkPutTextInks) {
    cv::FontFace uni("uni");
    cv::Mat img(64, 256, cv::CV_8UC1, cv::Scalar(0));
    cv::putText(img, "中文字体", cv::Point(8, 44), cv::Scalar(255), uni, 28);
    EXPECT_GT(cv::countNonZero(img), 100);  // glyphs actually rendered
}
#endif
