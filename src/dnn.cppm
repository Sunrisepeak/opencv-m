// opencv.dnn — C++23 module over OpenCV 5's deep-learning module (API
// unchanged). OPTIONAL: this file is compiled only when the package `dnn`
// feature is enabled, which forwards `compat.opencv/dnn` so the underlying
// library is built with its dnn module (+ vendored protobuf + mlas). It is
// deliberately NOT part of the `opencv.cv` umbrella — consumers opt in with
// `import opencv.dnn;` (Net, blobFromImage, readNet, NMSBoxes, …).
module;
#include <opencv2/dnn.hpp>
// <opencv2/dnn.hpp> pulls in only the core dnn surface (Net, blobFromImage,
// readNet, Model classes, Backend/Target). The 181 Layer types + ACTIV_/
// AUTO_PAD_ enums live in this separate public header — include it so they are
// visible to the `export using` list below.
#include <opencv2/dnn/all_layers.hpp>

export module opencv.dnn;

#include "gen_exports/dnn.inc"
