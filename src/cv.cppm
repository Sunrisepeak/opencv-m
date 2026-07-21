// opencv.cv — umbrella module: the `import opencv.cv;` analog of
// `#include <opencv2/opencv.hpp>` / Python's `import cv2` for the modules
// enabled in this build profile.
export module opencv.cv;

export import opencv.core;
export import opencv.imgproc;
export import opencv.imgcodecs;
export import opencv.videoio;
export import opencv.highgui;
export import opencv.flann;
export import opencv.geometry;
