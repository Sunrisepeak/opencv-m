// Classic first OpenCV program, header-free: load (or synthesize) an image,
// resize, grayscale, write a PNG next to it.
#include <cstdio>
import std;
import opencv.cv;

int main(int argc, char** argv) {
    cv::Mat img;
    if (argc > 1) {
        img = cv::imread(argv[1], cv::IMREAD_COLOR);
        if (img.empty()) {
            std::println(stderr, "cannot read {}", argv[1]);
            return 1;
        }
    } else {
        img = cv::Mat(240, 320, cv::CV_8UC3, cv::Scalar(40, 80, 160));
        cv::putText(img, "opencv-m", {40, 130}, cv::FONT_HERSHEY_SIMPLEX, 1.5,
                    {255, 255, 255}, 2);
    }
    cv::Mat small, gray;
    cv::resize(img, small, {}, 0.5, 0.5, cv::INTER_AREA);
    cv::cvtColor(small, gray, cv::COLOR_BGR2GRAY);
    const char* out = "gray_pipeline_out.png";
    if (!cv::imwrite(out, gray)) {
        std::println(stderr, "imwrite failed");
        return 2;
    }
    std::println("{} written ({}x{})", out, gray.cols, gray.rows);
}
