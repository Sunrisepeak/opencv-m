// opencv — library-root module (mcpp convention: src/<package>.cppm).
// Everything lives in opencv.cv and the per-module interfaces; this root
// simply forwards so `import opencv;` also works.
export module opencv;

export import opencv.cv;
