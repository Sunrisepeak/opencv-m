// core_ops.inc — hand-maintained replacement surface for upstream entities
// that CANNOT cross a module boundary because OpenCV declares them
// `static inline` (internal linkage): the namespace-scope operator templates
// for the everyday value types, and saturate_cast which they depend on.
//
// These are self-contained reimplementations (no calls into TU-local
// upstream helpers — importer-side template instantiation must not expose
// them). Semantics follow upstream types.hpp; float→integer conversions
// round to nearest-even (lrint), matching cvRound on every mainstream
// target. Matx/Vec matrix algebra is NOT replaced yet — known v0.0.1 gap,
// tracked in README Notes.
//
// Mixed TUs (textual <opencv2/…> include + import): the header's internal
// operators and these module operators are both visible there — calls on
// value types become ambiguous. In mixed TUs prefer member/field forms.

export namespace cv {
// inline namespace: distinct entities from the upstream TU-local ones (no
// in-module redefinition), yet reachable as cv::… via qualified lookup and
// ADL on the importer side, where the upstream internals are invisible.
inline namespace opencv_m_repl {

// ── saturate_cast (upstream: static inline, core/saturate.hpp) ──────────
template<typename _Tp> constexpr _Tp saturate_cast(unsigned char v)      { return _Tp(v); }
template<typename _Tp> constexpr _Tp saturate_cast(signed char v)        { return _Tp(v); }
template<typename _Tp> constexpr _Tp saturate_cast(unsigned short v)     { return _Tp(v); }
template<typename _Tp> constexpr _Tp saturate_cast(short v)              { return _Tp(v); }
template<typename _Tp> constexpr _Tp saturate_cast(unsigned v)           { return _Tp(v); }
template<typename _Tp> constexpr _Tp saturate_cast(int v)                { return _Tp(v); }
template<typename _Tp> constexpr _Tp saturate_cast(float v)              { return _Tp(v); }
template<typename _Tp> constexpr _Tp saturate_cast(double v)             { return _Tp(v); }
template<typename _Tp> constexpr _Tp saturate_cast(long long v)          { return _Tp(v); }
template<typename _Tp> constexpr _Tp saturate_cast(unsigned long long v) { return _Tp(v); }

template<> inline unsigned char saturate_cast<unsigned char>(int v)
{ return (unsigned char)((unsigned)v <= 255 ? v : v > 0 ? 255 : 0); }
template<> inline unsigned char saturate_cast<unsigned char>(float v)
{ return saturate_cast<unsigned char>((int)__builtin_lrintf(v)); }
template<> inline unsigned char saturate_cast<unsigned char>(double v)
{ return saturate_cast<unsigned char>((int)__builtin_lrint(v)); }
template<> inline signed char saturate_cast<signed char>(int v)
{ return (signed char)((unsigned)(v + 128) <= 255 ? v : v > 0 ? 127 : -128); }
template<> inline signed char saturate_cast<signed char>(float v)
{ return saturate_cast<signed char>((int)__builtin_lrintf(v)); }
template<> inline signed char saturate_cast<signed char>(double v)
{ return saturate_cast<signed char>((int)__builtin_lrint(v)); }
template<> inline unsigned short saturate_cast<unsigned short>(int v)
{ return (unsigned short)((unsigned)v <= 65535 ? v : v > 0 ? 65535 : 0); }
template<> inline unsigned short saturate_cast<unsigned short>(float v)
{ return saturate_cast<unsigned short>((int)__builtin_lrintf(v)); }
template<> inline unsigned short saturate_cast<unsigned short>(double v)
{ return saturate_cast<unsigned short>((int)__builtin_lrint(v)); }
template<> inline short saturate_cast<short>(int v)
{ return (short)((unsigned)(v + 32768) <= 65535 ? v : v > 0 ? 32767 : -32768); }
template<> inline short saturate_cast<short>(float v)
{ return saturate_cast<short>((int)__builtin_lrintf(v)); }
template<> inline short saturate_cast<short>(double v)
{ return saturate_cast<short>((int)__builtin_lrint(v)); }
template<> inline int saturate_cast<int>(float v)  { return (int)__builtin_lrintf(v); }
template<> inline int saturate_cast<int>(double v) { return (int)__builtin_lrint(v); }

// ── Point_ ──────────────────────────────────────────────────────────────
template<typename _Tp> inline bool operator==(const Point_<_Tp>& a, const Point_<_Tp>& b)
{ return a.x == b.x && a.y == b.y; }
template<typename _Tp> inline bool operator!=(const Point_<_Tp>& a, const Point_<_Tp>& b)
{ return a.x != b.x || a.y != b.y; }
template<typename _Tp> inline Point_<_Tp> operator+(const Point_<_Tp>& a, const Point_<_Tp>& b)
{ return Point_<_Tp>(saturate_cast<_Tp>(a.x + b.x), saturate_cast<_Tp>(a.y + b.y)); }
template<typename _Tp> inline Point_<_Tp> operator-(const Point_<_Tp>& a, const Point_<_Tp>& b)
{ return Point_<_Tp>(saturate_cast<_Tp>(a.x - b.x), saturate_cast<_Tp>(a.y - b.y)); }
template<typename _Tp> inline Point_<_Tp> operator-(const Point_<_Tp>& a)
{ return Point_<_Tp>(saturate_cast<_Tp>(-a.x), saturate_cast<_Tp>(-a.y)); }
template<typename _Tp> inline Point_<_Tp> operator*(const Point_<_Tp>& a, int b)
{ return Point_<_Tp>(saturate_cast<_Tp>(a.x * b), saturate_cast<_Tp>(a.y * b)); }
template<typename _Tp> inline Point_<_Tp> operator*(int a, const Point_<_Tp>& b)
{ return Point_<_Tp>(saturate_cast<_Tp>(b.x * a), saturate_cast<_Tp>(b.y * a)); }
template<typename _Tp> inline Point_<_Tp> operator*(const Point_<_Tp>& a, float b)
{ return Point_<_Tp>(saturate_cast<_Tp>(a.x * b), saturate_cast<_Tp>(a.y * b)); }
template<typename _Tp> inline Point_<_Tp> operator*(float a, const Point_<_Tp>& b)
{ return Point_<_Tp>(saturate_cast<_Tp>(b.x * a), saturate_cast<_Tp>(b.y * a)); }
template<typename _Tp> inline Point_<_Tp> operator*(const Point_<_Tp>& a, double b)
{ return Point_<_Tp>(saturate_cast<_Tp>(a.x * b), saturate_cast<_Tp>(a.y * b)); }
template<typename _Tp> inline Point_<_Tp> operator*(double a, const Point_<_Tp>& b)
{ return Point_<_Tp>(saturate_cast<_Tp>(b.x * a), saturate_cast<_Tp>(b.y * a)); }
template<typename _Tp> inline Point_<_Tp> operator/(const Point_<_Tp>& a, int b)
{ return Point_<_Tp>(saturate_cast<_Tp>(a.x / b), saturate_cast<_Tp>(a.y / b)); }
template<typename _Tp> inline Point_<_Tp> operator/(const Point_<_Tp>& a, float b)
{ return Point_<_Tp>(saturate_cast<_Tp>(a.x / b), saturate_cast<_Tp>(a.y / b)); }
template<typename _Tp> inline Point_<_Tp> operator/(const Point_<_Tp>& a, double b)
{ return Point_<_Tp>(saturate_cast<_Tp>(a.x / b), saturate_cast<_Tp>(a.y / b)); }
template<typename _Tp> inline Point_<_Tp>& operator+=(Point_<_Tp>& a, const Point_<_Tp>& b)
{ a.x = saturate_cast<_Tp>(a.x + b.x); a.y = saturate_cast<_Tp>(a.y + b.y); return a; }
template<typename _Tp> inline Point_<_Tp>& operator-=(Point_<_Tp>& a, const Point_<_Tp>& b)
{ a.x = saturate_cast<_Tp>(a.x - b.x); a.y = saturate_cast<_Tp>(a.y - b.y); return a; }

// ── Point3_ ─────────────────────────────────────────────────────────────
template<typename _Tp> inline bool operator==(const Point3_<_Tp>& a, const Point3_<_Tp>& b)
{ return a.x == b.x && a.y == b.y && a.z == b.z; }
template<typename _Tp> inline bool operator!=(const Point3_<_Tp>& a, const Point3_<_Tp>& b)
{ return a.x != b.x || a.y != b.y || a.z != b.z; }
template<typename _Tp> inline Point3_<_Tp> operator+(const Point3_<_Tp>& a, const Point3_<_Tp>& b)
{ return Point3_<_Tp>(saturate_cast<_Tp>(a.x + b.x), saturate_cast<_Tp>(a.y + b.y), saturate_cast<_Tp>(a.z + b.z)); }
template<typename _Tp> inline Point3_<_Tp> operator-(const Point3_<_Tp>& a, const Point3_<_Tp>& b)
{ return Point3_<_Tp>(saturate_cast<_Tp>(a.x - b.x), saturate_cast<_Tp>(a.y - b.y), saturate_cast<_Tp>(a.z - b.z)); }
template<typename _Tp> inline Point3_<_Tp> operator-(const Point3_<_Tp>& a)
{ return Point3_<_Tp>(saturate_cast<_Tp>(-a.x), saturate_cast<_Tp>(-a.y), saturate_cast<_Tp>(-a.z)); }
template<typename _Tp> inline Point3_<_Tp> operator*(const Point3_<_Tp>& a, double b)
{ return Point3_<_Tp>(saturate_cast<_Tp>(a.x * b), saturate_cast<_Tp>(a.y * b), saturate_cast<_Tp>(a.z * b)); }
template<typename _Tp> inline Point3_<_Tp> operator*(double a, const Point3_<_Tp>& b)
{ return Point3_<_Tp>(saturate_cast<_Tp>(b.x * a), saturate_cast<_Tp>(b.y * a), saturate_cast<_Tp>(b.z * a)); }

// ── Size_ ───────────────────────────────────────────────────────────────
template<typename _Tp> inline bool operator==(const Size_<_Tp>& a, const Size_<_Tp>& b)
{ return a.width == b.width && a.height == b.height; }
template<typename _Tp> inline bool operator!=(const Size_<_Tp>& a, const Size_<_Tp>& b)
{ return a.width != b.width || a.height != b.height; }
template<typename _Tp> inline Size_<_Tp> operator+(const Size_<_Tp>& a, const Size_<_Tp>& b)
{ return Size_<_Tp>(a.width + b.width, a.height + b.height); }
template<typename _Tp> inline Size_<_Tp> operator-(const Size_<_Tp>& a, const Size_<_Tp>& b)
{ return Size_<_Tp>(a.width - b.width, a.height - b.height); }
template<typename _Tp> inline Size_<_Tp> operator*(const Size_<_Tp>& a, _Tp b)
{ return Size_<_Tp>(a.width * b, a.height * b); }
template<typename _Tp> inline Size_<_Tp> operator/(const Size_<_Tp>& a, _Tp b)
{ return Size_<_Tp>(a.width / b, a.height / b); }

// ── Rect_ ───────────────────────────────────────────────────────────────
template<typename _Tp> inline bool operator==(const Rect_<_Tp>& a, const Rect_<_Tp>& b)
{ return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height; }
template<typename _Tp> inline bool operator!=(const Rect_<_Tp>& a, const Rect_<_Tp>& b)
{ return a.x != b.x || a.y != b.y || a.width != b.width || a.height != b.height; }
template<typename _Tp> inline Rect_<_Tp> operator+(const Rect_<_Tp>& a, const Point_<_Tp>& b)
{ return Rect_<_Tp>(a.x + b.x, a.y + b.y, a.width, a.height); }
template<typename _Tp> inline Rect_<_Tp> operator-(const Rect_<_Tp>& a, const Point_<_Tp>& b)
{ return Rect_<_Tp>(a.x - b.x, a.y - b.y, a.width, a.height); }
template<typename _Tp> inline Rect_<_Tp> operator+(const Rect_<_Tp>& a, const Size_<_Tp>& b)
{ return Rect_<_Tp>(a.x, a.y, a.width + b.width, a.height + b.height); }
template<typename _Tp> inline Rect_<_Tp> operator&(const Rect_<_Tp>& a, const Rect_<_Tp>& b)
{
    _Tp x1 = a.x > b.x ? a.x : b.x;
    _Tp y1 = a.y > b.y ? a.y : b.y;
    _Tp x2 = (a.x + a.width < b.x + b.width ? a.x + a.width : b.x + b.width);
    _Tp y2 = (a.y + a.height < b.y + b.height ? a.y + a.height : b.y + b.height);
    if (x2 <= x1 || y2 <= y1) return Rect_<_Tp>();
    return Rect_<_Tp>(x1, y1, x2 - x1, y2 - y1);
}
template<typename _Tp> inline Rect_<_Tp> operator|(const Rect_<_Tp>& a, const Rect_<_Tp>& b)
{
    if (a.width <= 0 || a.height <= 0) return b;
    if (b.width <= 0 || b.height <= 0) return a;
    _Tp x1 = a.x < b.x ? a.x : b.x;
    _Tp y1 = a.y < b.y ? a.y : b.y;
    _Tp x2 = (a.x + a.width > b.x + b.width ? a.x + a.width : b.x + b.width);
    _Tp y2 = (a.y + a.height > b.y + b.height ? a.y + a.height : b.y + b.height);
    return Rect_<_Tp>(x1, y1, x2 - x1, y2 - y1);
}

// ── Range ───────────────────────────────────────────────────────────────
inline bool operator==(const Range& a, const Range& b) { return a.start == b.start && a.end == b.end; }
inline bool operator!=(const Range& a, const Range& b) { return a.start != b.start || a.end != b.end; }
inline bool operator!(const Range& r) { return r.start == r.end; }
inline Range operator&(const Range& a, const Range& b)
{
    Range r(a.start > b.start ? a.start : b.start, a.end < b.end ? a.end : b.end);
    if (r.end < r.start) r.end = r.start;
    return r;
}
inline Range operator+(const Range& r, int delta) { return Range(r.start + delta, r.end + delta); }
inline Range operator+(int delta, const Range& r) { return Range(r.start + delta, r.end + delta); }
inline Range operator-(const Range& r, int delta) { return Range(r.start - delta, r.end - delta); }

// ── Scalar_ ─────────────────────────────────────────────────────────────
template<typename _Tp> inline bool operator==(const Scalar_<_Tp>& a, const Scalar_<_Tp>& b)
{ return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]; }
template<typename _Tp> inline bool operator!=(const Scalar_<_Tp>& a, const Scalar_<_Tp>& b)
{ return a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3]; }
template<typename _Tp> inline Scalar_<_Tp> operator+(const Scalar_<_Tp>& a, const Scalar_<_Tp>& b)
{ return Scalar_<_Tp>(a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]); }
template<typename _Tp> inline Scalar_<_Tp> operator-(const Scalar_<_Tp>& a, const Scalar_<_Tp>& b)
{ return Scalar_<_Tp>(saturate_cast<_Tp>(a[0] - b[0]), saturate_cast<_Tp>(a[1] - b[1]),
                      saturate_cast<_Tp>(a[2] - b[2]), saturate_cast<_Tp>(a[3] - b[3])); }
template<typename _Tp> inline Scalar_<_Tp> operator*(const Scalar_<_Tp>& a, _Tp alpha)
{ return Scalar_<_Tp>(a[0] * alpha, a[1] * alpha, a[2] * alpha, a[3] * alpha); }
template<typename _Tp> inline Scalar_<_Tp> operator*(_Tp alpha, const Scalar_<_Tp>& a)
{ return Scalar_<_Tp>(a[0] * alpha, a[1] * alpha, a[2] * alpha, a[3] * alpha); }
template<typename _Tp> inline Scalar_<_Tp> operator-(const Scalar_<_Tp>& a)
{ return Scalar_<_Tp>(saturate_cast<_Tp>(-a[0]), saturate_cast<_Tp>(-a[1]),
                      saturate_cast<_Tp>(-a[2]), saturate_cast<_Tp>(-a[3])); }

// ── Complex ─────────────────────────────────────────────────────────────
template<typename _Tp> inline bool operator==(const Complex<_Tp>& a, const Complex<_Tp>& b)
{ return a.re == b.re && a.im == b.im; }
template<typename _Tp> inline bool operator!=(const Complex<_Tp>& a, const Complex<_Tp>& b)
{ return a.re != b.re || a.im != b.im; }
template<typename _Tp> inline Complex<_Tp> operator+(const Complex<_Tp>& a, const Complex<_Tp>& b)
{ return Complex<_Tp>(a.re + b.re, a.im + b.im); }
template<typename _Tp> inline Complex<_Tp> operator-(const Complex<_Tp>& a, const Complex<_Tp>& b)
{ return Complex<_Tp>(a.re - b.re, a.im - b.im); }
template<typename _Tp> inline Complex<_Tp> operator*(const Complex<_Tp>& a, const Complex<_Tp>& b)
{ return Complex<_Tp>(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re); }

} // inline namespace opencv_m_repl
} // export namespace cv

// ── Mat / MatExpr operator delegation ───────────────────────────────────
// Upstream declares these CV_EXPORTS (external) — but the plain
// `using cv::operator+;` route would also drag the TU-local template
// overload sets into importers and collide with the replacements above.
// Instead: capture each upstream overload by exact function-pointer type
// (unique even though calls would be ambiguous), then re-expose it as a
// delegating operator inside the exported inline namespace.
namespace opencv_m_up {
inline constexpr auto up_ne_sh_sh = static_cast<bool (*)(const cv::MatShape&, const cv::MatShape&)>(&cv::operator !=);
inline constexpr auto up_eq_sh_sh = static_cast<bool (*)(const cv::MatShape&, const cv::MatShape&)>(&cv::operator ==);
inline constexpr auto up_ne_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator !=);
inline constexpr auto up_and_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator &);
inline constexpr auto up_mul_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator *);
inline constexpr auto up_add_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator +);
inline constexpr auto up_sub_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator -);
inline constexpr auto up_div_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator /);
inline constexpr auto up_lt_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator <);
inline constexpr auto up_le_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator <=);
inline constexpr auto up_eq_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator ==);
inline constexpr auto up_gt_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator >);
inline constexpr auto up_ge_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator >=);
inline constexpr auto up_xor_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator ^);
inline constexpr auto up_or_m_m = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Mat&)>(&cv::operator |);
inline constexpr auto up_and_m_s = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Scalar&)>(&cv::operator &);
inline constexpr auto up_add_m_s = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Scalar&)>(&cv::operator +);
inline constexpr auto up_sub_m_s = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Scalar&)>(&cv::operator -);
inline constexpr auto up_xor_m_s = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Scalar&)>(&cv::operator ^);
inline constexpr auto up_or_m_s = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::Scalar&)>(&cv::operator |);
inline constexpr auto up_ne_m_d = static_cast<cv::MatExpr (*)(const cv::Mat&, double)>(&cv::operator !=);
inline constexpr auto up_mul_m_d = static_cast<cv::MatExpr (*)(const cv::Mat&, double)>(&cv::operator *);
inline constexpr auto up_div_m_d = static_cast<cv::MatExpr (*)(const cv::Mat&, double)>(&cv::operator /);
inline constexpr auto up_lt_m_d = static_cast<cv::MatExpr (*)(const cv::Mat&, double)>(&cv::operator <);
inline constexpr auto up_le_m_d = static_cast<cv::MatExpr (*)(const cv::Mat&, double)>(&cv::operator <=);
inline constexpr auto up_eq_m_d = static_cast<cv::MatExpr (*)(const cv::Mat&, double)>(&cv::operator ==);
inline constexpr auto up_gt_m_d = static_cast<cv::MatExpr (*)(const cv::Mat&, double)>(&cv::operator >);
inline constexpr auto up_ge_m_d = static_cast<cv::MatExpr (*)(const cv::Mat&, double)>(&cv::operator >=);
inline constexpr auto up_sub_e = static_cast<cv::MatExpr (*)(const cv::MatExpr&)>(&cv::operator -);
inline constexpr auto up_mul_e_e = static_cast<cv::MatExpr (*)(const cv::MatExpr&, const cv::MatExpr&)>(&cv::operator *);
inline constexpr auto up_add_e_e = static_cast<cv::MatExpr (*)(const cv::MatExpr&, const cv::MatExpr&)>(&cv::operator +);
inline constexpr auto up_sub_e_e = static_cast<cv::MatExpr (*)(const cv::MatExpr&, const cv::MatExpr&)>(&cv::operator -);
inline constexpr auto up_div_e_e = static_cast<cv::MatExpr (*)(const cv::MatExpr&, const cv::MatExpr&)>(&cv::operator /);
inline constexpr auto up_mul_e_m = static_cast<cv::MatExpr (*)(const cv::MatExpr&, const cv::Mat&)>(&cv::operator *);
inline constexpr auto up_add_e_m = static_cast<cv::MatExpr (*)(const cv::MatExpr&, const cv::Mat&)>(&cv::operator +);
inline constexpr auto up_sub_e_m = static_cast<cv::MatExpr (*)(const cv::MatExpr&, const cv::Mat&)>(&cv::operator -);
inline constexpr auto up_div_e_m = static_cast<cv::MatExpr (*)(const cv::MatExpr&, const cv::Mat&)>(&cv::operator /);
inline constexpr auto up_add_e_s = static_cast<cv::MatExpr (*)(const cv::MatExpr&, const cv::Scalar&)>(&cv::operator +);
inline constexpr auto up_sub_e_s = static_cast<cv::MatExpr (*)(const cv::MatExpr&, const cv::Scalar&)>(&cv::operator -);
inline constexpr auto up_mul_e_d = static_cast<cv::MatExpr (*)(const cv::MatExpr&, double)>(&cv::operator *);
inline constexpr auto up_div_e_d = static_cast<cv::MatExpr (*)(const cv::MatExpr&, double)>(&cv::operator /);
inline constexpr auto up_sub_m = static_cast<cv::MatExpr (*)(const cv::Mat&)>(&cv::operator -);
inline constexpr auto up_not_m = static_cast<cv::MatExpr (*)(const cv::Mat&)>(&cv::operator ~);
inline constexpr auto up_mul_m_e = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::MatExpr&)>(&cv::operator *);
inline constexpr auto up_add_m_e = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::MatExpr&)>(&cv::operator +);
inline constexpr auto up_sub_m_e = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::MatExpr&)>(&cv::operator -);
inline constexpr auto up_div_m_e = static_cast<cv::MatExpr (*)(const cv::Mat&, const cv::MatExpr&)>(&cv::operator /);
inline constexpr auto up_and_s_m = static_cast<cv::MatExpr (*)(const cv::Scalar&, const cv::Mat&)>(&cv::operator &);
inline constexpr auto up_add_s_m = static_cast<cv::MatExpr (*)(const cv::Scalar&, const cv::Mat&)>(&cv::operator +);
inline constexpr auto up_sub_s_m = static_cast<cv::MatExpr (*)(const cv::Scalar&, const cv::Mat&)>(&cv::operator -);
inline constexpr auto up_xor_s_m = static_cast<cv::MatExpr (*)(const cv::Scalar&, const cv::Mat&)>(&cv::operator ^);
inline constexpr auto up_or_s_m = static_cast<cv::MatExpr (*)(const cv::Scalar&, const cv::Mat&)>(&cv::operator |);
inline constexpr auto up_add_s_e = static_cast<cv::MatExpr (*)(const cv::Scalar&, const cv::MatExpr&)>(&cv::operator +);
inline constexpr auto up_sub_s_e = static_cast<cv::MatExpr (*)(const cv::Scalar&, const cv::MatExpr&)>(&cv::operator -);
inline constexpr auto up_ne_d_m = static_cast<cv::MatExpr (*)(double, const cv::Mat&)>(&cv::operator !=);
inline constexpr auto up_mul_d_m = static_cast<cv::MatExpr (*)(double, const cv::Mat&)>(&cv::operator *);
inline constexpr auto up_div_d_m = static_cast<cv::MatExpr (*)(double, const cv::Mat&)>(&cv::operator /);
inline constexpr auto up_lt_d_m = static_cast<cv::MatExpr (*)(double, const cv::Mat&)>(&cv::operator <);
inline constexpr auto up_le_d_m = static_cast<cv::MatExpr (*)(double, const cv::Mat&)>(&cv::operator <=);
inline constexpr auto up_eq_d_m = static_cast<cv::MatExpr (*)(double, const cv::Mat&)>(&cv::operator ==);
inline constexpr auto up_gt_d_m = static_cast<cv::MatExpr (*)(double, const cv::Mat&)>(&cv::operator >);
inline constexpr auto up_ge_d_m = static_cast<cv::MatExpr (*)(double, const cv::Mat&)>(&cv::operator >=);
inline constexpr auto up_mul_d_e = static_cast<cv::MatExpr (*)(double, const cv::MatExpr&)>(&cv::operator *);
inline constexpr auto up_div_d_e = static_cast<cv::MatExpr (*)(double, const cv::MatExpr&)>(&cv::operator /);
} // namespace opencv_m_up

export namespace cv {
inline namespace opencv_m_repl {
inline bool operator != (const cv::MatShape& a0, const cv::MatShape& a1) { return ::opencv_m_up::up_ne_sh_sh(a0, a1); }
inline bool operator == (const cv::MatShape& a0, const cv::MatShape& a1) { return ::opencv_m_up::up_eq_sh_sh(a0, a1); }
inline cv::MatExpr operator != (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_ne_m_m(a0, a1); }
inline cv::MatExpr operator & (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_and_m_m(a0, a1); }
inline cv::MatExpr operator * (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_mul_m_m(a0, a1); }
inline cv::MatExpr operator + (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_add_m_m(a0, a1); }
inline cv::MatExpr operator - (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_sub_m_m(a0, a1); }
inline cv::MatExpr operator / (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_div_m_m(a0, a1); }
inline cv::MatExpr operator < (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_lt_m_m(a0, a1); }
inline cv::MatExpr operator <= (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_le_m_m(a0, a1); }
inline cv::MatExpr operator == (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_eq_m_m(a0, a1); }
inline cv::MatExpr operator > (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_gt_m_m(a0, a1); }
inline cv::MatExpr operator >= (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_ge_m_m(a0, a1); }
inline cv::MatExpr operator ^ (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_xor_m_m(a0, a1); }
inline cv::MatExpr operator | (const cv::Mat& a0, const cv::Mat& a1) { return ::opencv_m_up::up_or_m_m(a0, a1); }
inline cv::MatExpr operator & (const cv::Mat& a0, const cv::Scalar& a1) { return ::opencv_m_up::up_and_m_s(a0, a1); }
inline cv::MatExpr operator + (const cv::Mat& a0, const cv::Scalar& a1) { return ::opencv_m_up::up_add_m_s(a0, a1); }
inline cv::MatExpr operator - (const cv::Mat& a0, const cv::Scalar& a1) { return ::opencv_m_up::up_sub_m_s(a0, a1); }
inline cv::MatExpr operator ^ (const cv::Mat& a0, const cv::Scalar& a1) { return ::opencv_m_up::up_xor_m_s(a0, a1); }
inline cv::MatExpr operator | (const cv::Mat& a0, const cv::Scalar& a1) { return ::opencv_m_up::up_or_m_s(a0, a1); }
inline cv::MatExpr operator != (const cv::Mat& a0, double a1) { return ::opencv_m_up::up_ne_m_d(a0, a1); }
inline cv::MatExpr operator * (const cv::Mat& a0, double a1) { return ::opencv_m_up::up_mul_m_d(a0, a1); }
inline cv::MatExpr operator / (const cv::Mat& a0, double a1) { return ::opencv_m_up::up_div_m_d(a0, a1); }
inline cv::MatExpr operator < (const cv::Mat& a0, double a1) { return ::opencv_m_up::up_lt_m_d(a0, a1); }
inline cv::MatExpr operator <= (const cv::Mat& a0, double a1) { return ::opencv_m_up::up_le_m_d(a0, a1); }
inline cv::MatExpr operator == (const cv::Mat& a0, double a1) { return ::opencv_m_up::up_eq_m_d(a0, a1); }
inline cv::MatExpr operator > (const cv::Mat& a0, double a1) { return ::opencv_m_up::up_gt_m_d(a0, a1); }
inline cv::MatExpr operator >= (const cv::Mat& a0, double a1) { return ::opencv_m_up::up_ge_m_d(a0, a1); }
inline cv::MatExpr operator - (const cv::MatExpr& a0) { return ::opencv_m_up::up_sub_e(a0); }
inline cv::MatExpr operator * (const cv::MatExpr& a0, const cv::MatExpr& a1) { return ::opencv_m_up::up_mul_e_e(a0, a1); }
inline cv::MatExpr operator + (const cv::MatExpr& a0, const cv::MatExpr& a1) { return ::opencv_m_up::up_add_e_e(a0, a1); }
inline cv::MatExpr operator - (const cv::MatExpr& a0, const cv::MatExpr& a1) { return ::opencv_m_up::up_sub_e_e(a0, a1); }
inline cv::MatExpr operator / (const cv::MatExpr& a0, const cv::MatExpr& a1) { return ::opencv_m_up::up_div_e_e(a0, a1); }
inline cv::MatExpr operator * (const cv::MatExpr& a0, const cv::Mat& a1) { return ::opencv_m_up::up_mul_e_m(a0, a1); }
inline cv::MatExpr operator + (const cv::MatExpr& a0, const cv::Mat& a1) { return ::opencv_m_up::up_add_e_m(a0, a1); }
inline cv::MatExpr operator - (const cv::MatExpr& a0, const cv::Mat& a1) { return ::opencv_m_up::up_sub_e_m(a0, a1); }
inline cv::MatExpr operator / (const cv::MatExpr& a0, const cv::Mat& a1) { return ::opencv_m_up::up_div_e_m(a0, a1); }
inline cv::MatExpr operator + (const cv::MatExpr& a0, const cv::Scalar& a1) { return ::opencv_m_up::up_add_e_s(a0, a1); }
inline cv::MatExpr operator - (const cv::MatExpr& a0, const cv::Scalar& a1) { return ::opencv_m_up::up_sub_e_s(a0, a1); }
inline cv::MatExpr operator * (const cv::MatExpr& a0, double a1) { return ::opencv_m_up::up_mul_e_d(a0, a1); }
inline cv::MatExpr operator / (const cv::MatExpr& a0, double a1) { return ::opencv_m_up::up_div_e_d(a0, a1); }
inline cv::MatExpr operator - (const cv::Mat& a0) { return ::opencv_m_up::up_sub_m(a0); }
inline cv::MatExpr operator ~ (const cv::Mat& a0) { return ::opencv_m_up::up_not_m(a0); }
inline cv::MatExpr operator * (const cv::Mat& a0, const cv::MatExpr& a1) { return ::opencv_m_up::up_mul_m_e(a0, a1); }
inline cv::MatExpr operator + (const cv::Mat& a0, const cv::MatExpr& a1) { return ::opencv_m_up::up_add_m_e(a0, a1); }
inline cv::MatExpr operator - (const cv::Mat& a0, const cv::MatExpr& a1) { return ::opencv_m_up::up_sub_m_e(a0, a1); }
inline cv::MatExpr operator / (const cv::Mat& a0, const cv::MatExpr& a1) { return ::opencv_m_up::up_div_m_e(a0, a1); }
inline cv::MatExpr operator & (const cv::Scalar& a0, const cv::Mat& a1) { return ::opencv_m_up::up_and_s_m(a0, a1); }
inline cv::MatExpr operator + (const cv::Scalar& a0, const cv::Mat& a1) { return ::opencv_m_up::up_add_s_m(a0, a1); }
inline cv::MatExpr operator - (const cv::Scalar& a0, const cv::Mat& a1) { return ::opencv_m_up::up_sub_s_m(a0, a1); }
inline cv::MatExpr operator ^ (const cv::Scalar& a0, const cv::Mat& a1) { return ::opencv_m_up::up_xor_s_m(a0, a1); }
inline cv::MatExpr operator | (const cv::Scalar& a0, const cv::Mat& a1) { return ::opencv_m_up::up_or_s_m(a0, a1); }
inline cv::MatExpr operator + (const cv::Scalar& a0, const cv::MatExpr& a1) { return ::opencv_m_up::up_add_s_e(a0, a1); }
inline cv::MatExpr operator - (const cv::Scalar& a0, const cv::MatExpr& a1) { return ::opencv_m_up::up_sub_s_e(a0, a1); }
inline cv::MatExpr operator != (double a0, const cv::Mat& a1) { return ::opencv_m_up::up_ne_d_m(a0, a1); }
inline cv::MatExpr operator * (double a0, const cv::Mat& a1) { return ::opencv_m_up::up_mul_d_m(a0, a1); }
inline cv::MatExpr operator / (double a0, const cv::Mat& a1) { return ::opencv_m_up::up_div_d_m(a0, a1); }
inline cv::MatExpr operator < (double a0, const cv::Mat& a1) { return ::opencv_m_up::up_lt_d_m(a0, a1); }
inline cv::MatExpr operator <= (double a0, const cv::Mat& a1) { return ::opencv_m_up::up_le_d_m(a0, a1); }
inline cv::MatExpr operator == (double a0, const cv::Mat& a1) { return ::opencv_m_up::up_eq_d_m(a0, a1); }
inline cv::MatExpr operator > (double a0, const cv::Mat& a1) { return ::opencv_m_up::up_gt_d_m(a0, a1); }
inline cv::MatExpr operator >= (double a0, const cv::Mat& a1) { return ::opencv_m_up::up_ge_d_m(a0, a1); }
inline cv::MatExpr operator * (double a0, const cv::MatExpr& a1) { return ::opencv_m_up::up_mul_d_e(a0, a1); }
inline cv::MatExpr operator / (double a0, const cv::MatExpr& a1) { return ::opencv_m_up::up_div_d_e(a0, a1); }
} // inline namespace opencv_m_repl
} // export namespace cv
