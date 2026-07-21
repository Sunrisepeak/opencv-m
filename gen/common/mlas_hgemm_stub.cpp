// compat.opencv curated stub: the vendored mlas subset declares and
// calls MlasHGemmSupported but does not vendor its definition (it
// lives in onnxruntime's full mlas). False = fp16 HGemm unsupported,
// which is the truth for this subset; callers fall back.
#include "mlas.h"
bool MLASCALL MlasHGemmSupported(CBLAS_TRANSPOSE, CBLAS_TRANSPOSE) { return false; }
