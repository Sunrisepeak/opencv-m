// core_fns.inc — hand-maintained replacement surface for the NAMED free
// functions of core that OpenCV declares `static inline` (internal linkage)
// and therefore cannot cross a module boundary via `using cv::NAME;` (clang
// rejects `export`ing an internal-linkage using-declaration outright; gcc is
// lenient but the resulting entities are still not the deterministic,
// module-linkage surface we want). Companion to core_ops.inc / matx_ops.inc
// (operator layer); textually included after them in the same module purview.
//
// Two mechanisms, mirroring core_ops.inc exactly:
//
//   1. SELF-CONTAINED REIMPL — for the `static inline` overloads whose body
//      is a short, unambiguous transcription of the upstream header (align*,
//      *Up, getElemSize, cubeRoot(double), makePtr, to{Lower,Upper}Case, the
//      integer/Complex/Mat_ abs, the Mat_/Matx max/min, the Point norms, the
//      randu<_Tp>()/format/print family). Bodies call only public/CV_EXPORTS
//      entities (never TU-local GMF helpers), so importer-side instantiation
//      is closed. Signatures mirror upstream 1:1 and carry the trivially-true
//      `opencv_m_pick` constraint → in a mixed TU ours is MORE constrained
//      than the header's identical template and wins deterministically; the
//      non-template helpers use a dummy `_D=void` param for the same reason
//      (upstream's plain static-inline non-template wins in mixed TUs, ours
//      serves pure-import TUs). See core_ops.inc header for the full spec.
//
//   2. FN-POINTER FORWARD — for the CV_EXPORTS (external) overloads that share
//      a name with an internal one (so the whole `using` set is poisoned):
//      capture each by exact function-pointer type in `opencv_m_fns_up`, then
//      re-expose as a CONSTRAINED TEMPLATE delegating to the captured pointer
//      (upstream's plain function wins in mixed TUs; pure-import uses ours).
//
// Matx determinant/trace/norm and Matx/Vec algebra already live in
// matx_ops.inc; only the Mat/InputArray/Point/scalar surfaces are added here.
//
// PRUNED (see tools/curated/core.prune.txt): cv::details::char_tolower /
// char_toupper and cv::ipp::{setUseIPP_NE,useIPP_NE} (internal helpers, not
// user API); the read()/write() FileStorage *convenience wrappers* over
// geometry types (Matx/Vec/Point/Rect/…) — those are `static inline`
// templates over TU-local cv::internal::{WriteStructContext,Vec*Proxy}
// helpers that cannot be reached self-containedly; the CV_EXPORTS read()/
// write() *primitives* (scalars/String/Mat/SparseMat/KeyPoint/DMatch) ARE
// forwarded below.

// (cstdarg/cstdio are included in core.cppm's global module fragment so the
// std::va_list / FILE machinery used below is attached to the global module,
// not to opencv.core.)

namespace opencv_m_fns_up {
// ── abs (CV_EXPORTS Mat/MatExpr + external-inline softfloat) ────────────
inline constexpr auto abs_m   = static_cast<cv::MatExpr   (*)(const cv::Mat&)>(&cv::abs);
inline constexpr auto abs_e   = static_cast<cv::MatExpr   (*)(const cv::MatExpr&)>(&cv::abs);
inline constexpr auto abs_sf  = static_cast<cv::softfloat (*)(cv::softfloat)>(&cv::abs);
inline constexpr auto abs_sd  = static_cast<cv::softdouble(*)(cv::softdouble)>(&cv::abs);
// ── max / min (CV_EXPORTS Mat/MatExpr/InputArray/UMat + softfloat) ──────
inline constexpr auto max_mm  = static_cast<cv::MatExpr(*)(const cv::Mat&, const cv::Mat&)>(&cv::max);
inline constexpr auto max_md  = static_cast<cv::MatExpr(*)(const cv::Mat&, double)>(&cv::max);
inline constexpr auto max_dm  = static_cast<cv::MatExpr(*)(double, const cv::Mat&)>(&cv::max);
inline constexpr auto max_ia  = static_cast<void(*)(cv::InputArray, cv::InputArray, cv::OutputArray)>(&cv::max);
inline constexpr auto max_sf  = static_cast<cv::softfloat (*)(const cv::softfloat&,  const cv::softfloat&)>(&cv::max);
inline constexpr auto max_sd  = static_cast<cv::softdouble(*)(const cv::softdouble&, const cv::softdouble&)>(&cv::max);
inline constexpr auto min_mm  = static_cast<cv::MatExpr(*)(const cv::Mat&, const cv::Mat&)>(&cv::min);
inline constexpr auto min_md  = static_cast<cv::MatExpr(*)(const cv::Mat&, double)>(&cv::min);
inline constexpr auto min_dm  = static_cast<cv::MatExpr(*)(double, const cv::Mat&)>(&cv::min);
inline constexpr auto min_ia  = static_cast<void(*)(cv::InputArray, cv::InputArray, cv::OutputArray)>(&cv::min);
inline constexpr auto min_sf  = static_cast<cv::softfloat (*)(const cv::softfloat&,  const cv::softfloat&)>(&cv::min);
inline constexpr auto min_sd  = static_cast<cv::softdouble(*)(const cv::softdouble&, const cv::softdouble&)>(&cv::min);
// ── norm / determinant / trace (CV_EXPORTS InputArray + SparseMat) ──────
inline constexpr auto norm_a1 = static_cast<double(*)(cv::InputArray, int, cv::InputArray)>(&cv::norm);
inline constexpr auto norm_a2 = static_cast<double(*)(cv::InputArray, cv::InputArray, int, cv::InputArray)>(&cv::norm);
inline constexpr auto norm_sm = static_cast<double(*)(const cv::SparseMat&, int)>(&cv::norm);
inline constexpr auto det_a   = static_cast<double(*)(cv::InputArray)>(&cv::determinant);
inline constexpr auto trace_a = static_cast<cv::Scalar(*)(cv::InputArray)>(&cv::trace);
// ── swap / parallel_for_ / cubeRoot / randu (CV_EXPORTS) ────────────────
inline constexpr auto swap_mm = static_cast<void(*)(cv::Mat&, cv::Mat&)>(&cv::swap);
inline constexpr auto swap_uu = static_cast<void(*)(cv::UMat&, cv::UMat&)>(&cv::swap);
inline constexpr auto pf_body = static_cast<void(*)(const cv::Range&, const cv::ParallelLoopBody&, double)>(&cv::parallel_for_);
inline constexpr auto cbrt_f  = static_cast<float(*)(float)>(&cv::cubeRoot);
inline constexpr auto randu_a = static_cast<void(*)(cv::InputOutputArray, cv::InputArray, cv::InputArray)>(&cv::randu);
// ── write / read primitives (CV_EXPORTS, persistence.hpp) ───────────────
inline constexpr auto wr_i    = static_cast<void(*)(cv::FileStorage&, const cv::String&, int)>(&cv::write);
inline constexpr auto wr_b    = static_cast<void(*)(cv::FileStorage&, const cv::String&, bool)>(&cv::write);
inline constexpr auto wr_i64  = static_cast<void(*)(cv::FileStorage&, const cv::String&, int64_t)>(&cv::write);
inline constexpr auto wr_f    = static_cast<void(*)(cv::FileStorage&, const cv::String&, float)>(&cv::write);
inline constexpr auto wr_d    = static_cast<void(*)(cv::FileStorage&, const cv::String&, double)>(&cv::write);
inline constexpr auto wr_s    = static_cast<void(*)(cv::FileStorage&, const cv::String&, const cv::String&)>(&cv::write);
inline constexpr auto wr_m    = static_cast<void(*)(cv::FileStorage&, const cv::String&, const cv::Mat&)>(&cv::write);
inline constexpr auto wr_sm   = static_cast<void(*)(cv::FileStorage&, const cv::String&, const cv::SparseMat&)>(&cv::write);
inline constexpr auto wr_vkp  = static_cast<void(*)(cv::FileStorage&, const cv::String&, const std::vector<cv::KeyPoint>&)>(&cv::write);
inline constexpr auto wr_vdm  = static_cast<void(*)(cv::FileStorage&, const cv::String&, const std::vector<cv::DMatch>&)>(&cv::write);
inline constexpr auto rd_i    = static_cast<void(*)(const cv::FileNode&, int&, int)>(&cv::read);
inline constexpr auto rd_i64  = static_cast<void(*)(const cv::FileNode&, int64_t&, int64_t)>(&cv::read);
inline constexpr auto rd_f    = static_cast<void(*)(const cv::FileNode&, float&, float)>(&cv::read);
inline constexpr auto rd_d    = static_cast<void(*)(const cv::FileNode&, double&, double)>(&cv::read);
inline constexpr auto rd_s    = static_cast<void(*)(const cv::FileNode&, std::string&, const std::string&)>(&cv::read);
inline constexpr auto rd_m    = static_cast<void(*)(const cv::FileNode&, cv::Mat&, const cv::Mat&)>(&cv::read);
inline constexpr auto rd_sm   = static_cast<void(*)(const cv::FileNode&, cv::SparseMat&, const cv::SparseMat&)>(&cv::read);
inline constexpr auto rd_vkp  = static_cast<void(*)(const cv::FileNode&, std::vector<cv::KeyPoint>&)>(&cv::read);
inline constexpr auto rd_vdm  = static_cast<void(*)(const cv::FileNode&, std::vector<cv::DMatch>&)>(&cv::read);
inline constexpr auto rd_kp   = static_cast<void(*)(const cv::FileNode&, cv::KeyPoint&, const cv::KeyPoint&)>(&cv::read);
inline constexpr auto rd_dm   = static_cast<void(*)(const cv::FileNode&, cv::DMatch&, const cv::DMatch&)>(&cv::read);
} // namespace opencv_m_fns_up

namespace opencv_m_fns_detail {
// non-overloaded print core (upstream: print(Ptr<Formatted>, FILE*))
inline int print_fmtd(const cv::Ptr<cv::Formatted>& fmtd, FILE* stream)
{
    int written = 0;
    fmtd->reset();
    for (const char* str = fmtd->next(); str; str = fmtd->next())
        written += std::fputs(str, stream);
    return written;
}
// self-contained ParallelLoopBody adapter for the std::function overload
// (mirrors upstream ParallelLoopBodyLambdaWrapper; depends only on the
// CV_EXPORTS cv::ParallelLoopBody base — no TU-local GMF helper)
class LambdaBody : public cv::ParallelLoopBody {
    std::function<void(const cv::Range&)> m_functor;
public:
    explicit LambdaBody(std::function<void(const cv::Range&)> functor) : m_functor(functor) {}
    void operator()(const cv::Range& range) const override { m_functor(range); }
};
} // namespace opencv_m_fns_detail

export namespace cv {
inline namespace opencv_m_repl {

// scalar min/max (upstream cvstd.hpp does `using std::min/std::max`). We
// cannot re-export std::min/std::max here: their 2-arg and 3-arg(Compare)
// templates out-rank our Mat forwards in overload resolution (exact match vs
// conversion-to-InputArray) and hijack cv::max(Mat,Mat)/cv::max(a,b,dst).
// Instead expose arithmetic-only scalar forms — same result for the common
// case, and is_arithmetic_v cleanly excludes Mat/MatExpr/Matx/softfloat so
// those resolve to the dedicated overloads below.
template<class _Tp> requires std::is_arithmetic_v<_Tp>
inline const _Tp& min(const _Tp& a, const _Tp& b) { return a < b ? a : b; }
template<class _Tp> requires std::is_arithmetic_v<_Tp>
inline const _Tp& max(const _Tp& a, const _Tp& b) { return a > b ? a : b; }

// ── trivial self-contained reimpls (upstream: static inline) ────────────
// alignPtr / alignSize / divUp / roundUp / getElemSize — utility.hpp
template<typename _Tp> requires opencv_m_pick<_Tp>
inline _Tp* alignPtr(_Tp* ptr, int n = (int)sizeof(_Tp))
{ return (_Tp*)(((size_t)ptr + n-1) & -n); }
template<class _D = void> requires opencv_m_pick<_D>
inline size_t alignSize(size_t sz, int n) { return (sz + n-1) & -n; }
template<class _D = void> requires opencv_m_pick<_D>
inline int divUp(int a, unsigned int b) { return (a + b - 1) / b; }
template<class _D = void> requires opencv_m_pick<_D>
inline size_t divUp(size_t a, unsigned int b) { return (a + b - 1) / b; }
template<class _D = void> requires opencv_m_pick<_D>
inline int roundUp(int a, unsigned int b) { return a + b - 1 - (a + b - 1) % b; }
template<class _D = void> requires opencv_m_pick<_D>
inline size_t roundUp(size_t a, unsigned int b) { return a + b - 1 - (a + b - 1) % b; }
template<class _D = void> requires opencv_m_pick<_D>
inline size_t getElemSize(int type) { return (size_t)CV_ELEM_SIZE(type); }

// makePtr — cvstd_wrapper.hpp (static_assert on has_custom_delete dropped:
// that trait is a TU-local GMF helper; the shared_ptr construction is the
// operative body and the assert never fires for ordinary user types)
template<typename _Tp, typename ... A1> requires opencv_m_pick<_Tp>
inline Ptr<_Tp> makePtr(const A1&... a1) { return (Ptr<_Tp>)std::make_shared<_Tp>(a1...); }

// to{Lower,Upper}Case — cvstd.hpp (char_tolower/char_toupper inlined as a
// lambda so the body does not reference the pruned cv::details helpers)
template<class _D = void> requires opencv_m_pick<_D>
inline std::string toLowerCase(const std::string& str)
{
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](char ch){ return (char)std::tolower((int)ch); });
    return result;
}
template<class _D = void> requires opencv_m_pick<_D>
inline std::string toUpperCase(const std::string& str)
{
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](char ch){ return (char)std::toupper((int)ch); });
    return result;
}

// cubeRoot — base.hpp: float is CV_EXPORTS (forward), double is static inline
template<class _F> requires __is_same(_F, float)
inline float cubeRoot(_F val) { return ::opencv_m_fns_up::cbrt_f(val); }
template<class _D = void> requires opencv_m_pick<_D>
inline double cubeRoot(double val) { return std::cbrt(val); }

// ── abs (cvstd.hpp integers + types.hpp Complex + mat.inl.hpp Mat_ ; and
//    the CV_EXPORTS Mat/MatExpr + softfloat forwards) ─────────────────────
template<class _D = void> requires opencv_m_pick<_D> inline uchar    abs(uchar a)    { return a; }
template<class _D = void> requires opencv_m_pick<_D> inline ushort   abs(ushort a)   { return a; }
template<class _D = void> requires opencv_m_pick<_D> inline unsigned abs(unsigned a) { return a; }
template<class _D = void> requires opencv_m_pick<_D> inline uint64   abs(uint64 a)   { return a; }
template<typename _Tp> requires opencv_m_pick<_Tp>
inline double abs(const Complex<_Tp>& a) { return std::sqrt((double)a.re*a.re + (double)a.im*a.im); }
template<typename _Tp> requires opencv_m_pick<_Tp>
inline MatExpr abs(const Mat_<_Tp>& m) { return ::opencv_m_fns_up::abs_m((const Mat&)m); }
template<class _M> requires __is_same(_M, cv::Mat)
inline MatExpr abs(const _M& m) { return ::opencv_m_fns_up::abs_m(m); }
template<class _E> requires __is_same(_E, cv::MatExpr)
inline MatExpr abs(const _E& e) { return ::opencv_m_fns_up::abs_e(e); }
template<class _S> requires __is_same(_S, cv::softfloat)
inline softfloat abs(_S a) { return ::opencv_m_fns_up::abs_sf(a); }
template<class _S> requires __is_same(_S, cv::softdouble)
inline softdouble abs(_S a) { return ::opencv_m_fns_up::abs_sd(a); }

// ── max (CV_EXPORTS forwards + mat.hpp/mat.inl.hpp static-inline reimpls) ─
// Same-type two-arg forms use ONE template param `_A` (same shape as the
// `using std::max` template) so that, being MORE CONSTRAINED, they win the
// partial-ordering tie against std::max instead of losing to it.
template<class _A> requires __is_same(_A, cv::Mat)
inline MatExpr max(const _A& a, const _A& b) { return ::opencv_m_fns_up::max_mm(a, b); }
template<class _A, class _B> requires (__is_same(_A, cv::Mat) && std::is_arithmetic_v<_B>)
inline MatExpr max(const _A& a, _B s) { return ::opencv_m_fns_up::max_md(a, s); }
template<class _A, class _B> requires (std::is_arithmetic_v<_A> && __is_same(_B, cv::Mat))
inline MatExpr max(_A s, const _B& a) { return ::opencv_m_fns_up::max_dm(s, a); }
// element-wise 3-arg: concrete InputArray/OutputArray params cover Mat/UMat/
// etc. by the usual implicit conversions (no per-type forward needed).
template<class _D = void> requires opencv_m_pick<_D>
inline void max(InputArray a, InputArray b, OutputArray c) { ::opencv_m_fns_up::max_ia(a, b, c); }
template<class _S> requires __is_same(_S, cv::softfloat)
inline softfloat max(const _S& a, const _S& b) { return ::opencv_m_fns_up::max_sf(a, b); }
template<class _S> requires __is_same(_S, cv::softdouble)
inline softdouble max(const _S& a, const _S& b) { return ::opencv_m_fns_up::max_sd(a, b); }
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp>
inline MatExpr max(const Mat& a, const Matx<_Tp, m, n>& b) { return ::opencv_m_fns_up::max_mm(a, Mat(b)); }
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp>
inline MatExpr max(const Matx<_Tp, m, n>& a, const Mat& b) { return ::opencv_m_fns_up::max_mm(Mat(a), b); }
template<typename _Tp> requires opencv_m_pick<_Tp>
inline MatExpr max(const Mat_<_Tp>& a, const Mat_<_Tp>& b) { return ::opencv_m_fns_up::max_mm((const Mat&)a, (const Mat&)b); }
template<typename _Tp> requires opencv_m_pick<_Tp>
inline MatExpr max(const Mat_<_Tp>& a, double s) { return ::opencv_m_fns_up::max_md((const Mat&)a, s); }
template<typename _Tp> requires opencv_m_pick<_Tp>
inline MatExpr max(double s, const Mat_<_Tp>& a) { return ::opencv_m_fns_up::max_md((const Mat&)a, s); }

// ── min (mirror of max) ─────────────────────────────────────────────────
template<class _A> requires __is_same(_A, cv::Mat)
inline MatExpr min(const _A& a, const _A& b) { return ::opencv_m_fns_up::min_mm(a, b); }
template<class _A, class _B> requires (__is_same(_A, cv::Mat) && std::is_arithmetic_v<_B>)
inline MatExpr min(const _A& a, _B s) { return ::opencv_m_fns_up::min_md(a, s); }
template<class _A, class _B> requires (std::is_arithmetic_v<_A> && __is_same(_B, cv::Mat))
inline MatExpr min(_A s, const _B& a) { return ::opencv_m_fns_up::min_dm(s, a); }
template<class _D = void> requires opencv_m_pick<_D>
inline void min(InputArray a, InputArray b, OutputArray c) { ::opencv_m_fns_up::min_ia(a, b, c); }
template<class _S> requires __is_same(_S, cv::softfloat)
inline softfloat min(const _S& a, const _S& b) { return ::opencv_m_fns_up::min_sf(a, b); }
template<class _S> requires __is_same(_S, cv::softdouble)
inline softdouble min(const _S& a, const _S& b) { return ::opencv_m_fns_up::min_sd(a, b); }
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp>
inline MatExpr min(const Mat& a, const Matx<_Tp, m, n>& b) { return ::opencv_m_fns_up::min_mm(a, Mat(b)); }
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp>
inline MatExpr min(const Matx<_Tp, m, n>& a, const Mat& b) { return ::opencv_m_fns_up::min_mm(Mat(a), b); }
template<typename _Tp> requires opencv_m_pick<_Tp>
inline MatExpr min(const Mat_<_Tp>& a, const Mat_<_Tp>& b) { return ::opencv_m_fns_up::min_mm((const Mat&)a, (const Mat&)b); }
template<typename _Tp> requires opencv_m_pick<_Tp>
inline MatExpr min(const Mat_<_Tp>& a, double s) { return ::opencv_m_fns_up::min_md((const Mat&)a, s); }
template<typename _Tp> requires opencv_m_pick<_Tp>
inline MatExpr min(double s, const Mat_<_Tp>& a) { return ::opencv_m_fns_up::min_md((const Mat&)a, s); }

// ── norm (CV_EXPORTS InputArray/SparseMat forwards + Point reimpls; Matx
//    norms are in matx_ops.inc) ──────────────────────────────────────────
template<class _D = void> requires opencv_m_pick<_D>
inline double norm(InputArray src1, int normType = NORM_L2, InputArray mask = noArray())
{ return ::opencv_m_fns_up::norm_a1(src1, normType, mask); }
template<class _D = void> requires opencv_m_pick<_D>
inline double norm(InputArray src1, InputArray src2, int normType = NORM_L2, InputArray mask = noArray())
{ return ::opencv_m_fns_up::norm_a2(src1, src2, normType, mask); }
template<class _SM> requires __is_same(_SM, cv::SparseMat)
inline double norm(const _SM& src, int normType) { return ::opencv_m_fns_up::norm_sm(src, normType); }
template<typename _Tp> requires opencv_m_pick<_Tp>
inline double norm(const Point_<_Tp>& pt) { return std::sqrt((double)pt.x*pt.x + (double)pt.y*pt.y); }
template<typename _Tp> requires opencv_m_pick<_Tp>
inline double norm(const Point3_<_Tp>& pt) { return std::sqrt((double)pt.x*pt.x + (double)pt.y*pt.y + (double)pt.z*pt.z); }

// ── determinant / trace (CV_EXPORTS InputArray; Matx in matx_ops.inc) ────
template<class _D = void> requires opencv_m_pick<_D>
inline double determinant(InputArray mtx) { return ::opencv_m_fns_up::det_a(mtx); }
template<class _D = void> requires opencv_m_pick<_D>
inline Scalar trace(InputArray mtx) { return ::opencv_m_fns_up::trace_a(mtx); }

// ── swap (CV_EXPORTS Mat/UMat forwards + MatExpr reimpl) ─────────────────
template<class _M> requires __is_same(_M, cv::Mat)
inline void swap(_M& a, _M& b) { ::opencv_m_fns_up::swap_mm(a, b); }
template<class _M> requires __is_same(_M, cv::UMat)
inline void swap(_M& a, _M& b) { ::opencv_m_fns_up::swap_uu(a, b); }
template<class _E> requires __is_same(_E, cv::MatExpr)
inline void swap(_E& a, _E& b) { a.swap(b); }

// ── parallel_for_ (CV_EXPORTS body forward + std::function reimpl) ───────
template<class _B> requires __is_same(_B, cv::ParallelLoopBody)
inline void parallel_for_(const Range& range, const _B& body, double nstripes = -1.)
{ ::opencv_m_fns_up::pf_body(range, body, nstripes); }
template<class _D = void> requires opencv_m_pick<_D>
inline void parallel_for_(const Range& range, std::function<void(const Range&)> functor, double nstripes = -1.)
{ ::opencv_m_fns_up::pf_body(range, ::opencv_m_fns_detail::LambdaBody(functor), nstripes); }

// ── format (operations.hpp static inline + core.hpp CV_EXPORTS printf) ───
template<class _FT> requires __is_same(_FT, cv::Formatter::FormatType)
inline Ptr<Formatted> format(InputArray mtx, _FT fmt) { return Formatter::get(fmt)->format(mtx.getMat()); }
template<class _D = void> requires opencv_m_pick<_D>
inline String format(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = std::vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);
    std::string buf((n > 0 ? n : 0), '\0');
    if (n > 0) std::vsnprintf(&buf[0], (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

// ── print (operations.hpp static inline family) ─────────────────────────
template<class _P> requires __is_same(_P, cv::Ptr<cv::Formatted>)
inline int print(const _P& fmtd, FILE* stream = stdout) { return ::opencv_m_fns_detail::print_fmtd(fmtd, stream); }
template<class _M> requires __is_same(_M, cv::Mat)
inline int print(const _M& mtx, FILE* stream = stdout) { return ::opencv_m_fns_detail::print_fmtd(Formatter::get()->format(mtx), stream); }
template<class _M> requires __is_same(_M, cv::UMat)
inline int print(const _M& mtx, FILE* stream = stdout) { return ::opencv_m_fns_detail::print_fmtd(Formatter::get()->format(mtx.getMat(ACCESS_READ)), stream); }
template<typename _Tp> requires opencv_m_pick<_Tp>
inline int print(const std::vector<Point_<_Tp> >& vec, FILE* stream = stdout) { return ::opencv_m_fns_detail::print_fmtd(Formatter::get()->format(Mat(vec)), stream); }
template<typename _Tp> requires opencv_m_pick<_Tp>
inline int print(const std::vector<Point3_<_Tp> >& vec, FILE* stream = stdout) { return ::opencv_m_fns_detail::print_fmtd(Formatter::get()->format(Mat(vec)), stream); }
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp>
inline int print(const Matx<_Tp, m, n>& matx, FILE* stream = stdout) { return ::opencv_m_fns_detail::print_fmtd(Formatter::get()->format(Mat(matx)), stream); }

// ── randu (operations.hpp randu<_Tp>() + core.hpp CV_EXPORTS array form) ─
template<typename _Tp> requires opencv_m_pick<_Tp>
inline _Tp randu() { return (_Tp)theRNG(); }
template<class _D = void> requires opencv_m_pick<_D>
inline void randu(InputOutputArray dst, InputArray low, InputArray high) { ::opencv_m_fns_up::randu_a(dst, low, high); }

// ── write / read CV_EXPORTS primitives (persistence.hpp) ────────────────
template<class _V> requires __is_same(_V, int)      inline void write(FileStorage& fs, const String& n, _V v) { ::opencv_m_fns_up::wr_i(fs, n, v); }
template<class _V> requires __is_same(_V, bool)     inline void write(FileStorage& fs, const String& n, _V v) { ::opencv_m_fns_up::wr_b(fs, n, v); }
template<class _V> requires __is_same(_V, int64_t)  inline void write(FileStorage& fs, const String& n, _V v) { ::opencv_m_fns_up::wr_i64(fs, n, v); }
template<class _V> requires __is_same(_V, float)    inline void write(FileStorage& fs, const String& n, _V v) { ::opencv_m_fns_up::wr_f(fs, n, v); }
template<class _V> requires __is_same(_V, double)   inline void write(FileStorage& fs, const String& n, _V v) { ::opencv_m_fns_up::wr_d(fs, n, v); }
template<class _V> requires __is_same(_V, cv::String)     inline void write(FileStorage& fs, const String& n, const _V& v) { ::opencv_m_fns_up::wr_s(fs, n, v); }
template<class _V> requires __is_same(_V, cv::Mat)        inline void write(FileStorage& fs, const String& n, const _V& v) { ::opencv_m_fns_up::wr_m(fs, n, v); }
template<class _V> requires __is_same(_V, cv::SparseMat)  inline void write(FileStorage& fs, const String& n, const _V& v) { ::opencv_m_fns_up::wr_sm(fs, n, v); }
template<class _V> requires __is_same(_V, std::vector<cv::KeyPoint>) inline void write(FileStorage& fs, const String& n, const _V& v) { ::opencv_m_fns_up::wr_vkp(fs, n, v); }
template<class _V> requires __is_same(_V, std::vector<cv::DMatch>)   inline void write(FileStorage& fs, const String& n, const _V& v) { ::opencv_m_fns_up::wr_vdm(fs, n, v); }

template<class _V> requires __is_same(_V, int)      inline void read(const FileNode& node, _V& v, _V dv) { ::opencv_m_fns_up::rd_i(node, v, dv); }
template<class _V> requires __is_same(_V, int64_t)  inline void read(const FileNode& node, _V& v, _V dv) { ::opencv_m_fns_up::rd_i64(node, v, dv); }
template<class _V> requires __is_same(_V, float)    inline void read(const FileNode& node, _V& v, _V dv) { ::opencv_m_fns_up::rd_f(node, v, dv); }
template<class _V> requires __is_same(_V, double)   inline void read(const FileNode& node, _V& v, _V dv) { ::opencv_m_fns_up::rd_d(node, v, dv); }
template<class _V> requires __is_same(_V, std::string)   inline void read(const FileNode& node, _V& v, const _V& dv) { ::opencv_m_fns_up::rd_s(node, v, dv); }
template<class _V> requires __is_same(_V, cv::Mat)       inline void read(const FileNode& node, _V& v, const _V& dv = cv::Mat()) { ::opencv_m_fns_up::rd_m(node, v, dv); }
template<class _V> requires __is_same(_V, cv::SparseMat) inline void read(const FileNode& node, _V& v, const _V& dv = cv::SparseMat()) { ::opencv_m_fns_up::rd_sm(node, v, dv); }
template<class _V> requires __is_same(_V, cv::KeyPoint)  inline void read(const FileNode& node, _V& v, const _V& dv) { ::opencv_m_fns_up::rd_kp(node, v, dv); }
template<class _V> requires __is_same(_V, cv::DMatch)    inline void read(const FileNode& node, _V& v, const _V& dv) { ::opencv_m_fns_up::rd_dm(node, v, dv); }
template<class _V> requires __is_same(_V, std::vector<cv::KeyPoint>) inline void read(const FileNode& node, _V& v) { ::opencv_m_fns_up::rd_vkp(node, v); }
template<class _V> requires __is_same(_V, std::vector<cv::DMatch>)   inline void read(const FileNode& node, _V& v) { ::opencv_m_fns_up::rd_vdm(node, v); }

} // inline namespace opencv_m_repl
} // export namespace cv
