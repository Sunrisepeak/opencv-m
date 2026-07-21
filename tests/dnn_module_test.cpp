// dnn feature test — the opencv.dnn MODULE interface (not textual headers).
// Only built with `--features dnn` (which adds the vendored dnn sources and
// compiles src/dnn.cppm); without the feature this TU has no tests, so the
// default `mcpp test` stays a clean pass. Deliberately not named dnn.cpp
// (would collide with the dep's modules/dnn/src/dnn.cpp — #240 family).
#include <gtest/gtest.h>
#if defined(MCPP_FEATURE_DNN)
import opencv.cv;
import opencv.dnn;

// blobFromImage through the module layer must yield a correctly shaped/valued
// NCHW blob (exercises the dnn core + the whole protobuf/mlas link closure),
// and an empty Net must construct.
TEST(DnnModule, BlobFromImageAndNet) {
    cv::Mat img(32, 32, cv::CV_8UC3, cv::Scalar(10, 20, 30));
    cv::Mat blob = cv::dnn::blobFromImage(img, 1.0 / 255.0, cv::Size(16, 16),
                                          cv::Scalar(), true, false);
    ASSERT_EQ(blob.dims, 4);
    EXPECT_EQ(blob.size[0], 1);
    EXPECT_EQ(blob.size[1], 3);
    EXPECT_EQ(blob.size[2], 16);
    EXPECT_EQ(blob.size[3], 16);
    EXPECT_NEAR(blob.ptr<float>(0)[0], 30.0f / 255.0f, 1e-3f);  // R after swap

    cv::dnn::Net net;
    EXPECT_TRUE(net.empty());
}
#endif
