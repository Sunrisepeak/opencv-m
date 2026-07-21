// matx_ops.inc — hand-maintained replacement surface for the Matx/Vec
// algebra (R1): the namespace-scope `static inline` operator templates and
// reduction helpers of core/matx.hpp + core/matx.inl.hpp, which have
// internal linkage upstream and cannot cross a module boundary. Textually
// included after core_ops.inc in the same module purview; upstream's
// unqualified saturate_cast calls become fully qualified
// cv::opencv_m_repl::saturate_cast (gcc16 counts GMF-internal upstream
// declarations as overload candidates during importer-side instantiation —
// unqualified calls would be ambiguous).
//
// These are self-contained reimplementations (no calls into TU-local
// upstream helpers): the Matx_AddOp/SubOp/ScaleOp/MatMulOp ctor-tag paths
// and base.hpp's static-inline normL1/normL2Sqr/normInf are replaced by
// explicit element loops over the public .val arrays, transcribed 1:1 from
// matx.inl.hpp — accumulator types and saturate_cast placement included.
// cv::LU and cv::cv_abs are called (CV_EXPORTS resp. plain-inline external
// linkage upstream — reachable, not redefined).
//
// Deliberately NOT replaced here:
//  * normalize(Vec)      — plain `inline` upstream (external linkage), not
//    static; already re-exported by gen_exports/core.inc `using cv::normalize;`.
//  * Matx members (inv/solve/t/mul/div/ddot/…), Vec members (cross/conj/…)
//    — member functions, cross the boundary with the class.
//  * operations.hpp augmenting ops (Mat += Matx, Mat_ ^= Matx, …) — static
//    inline but Mat-based (cv::add/gemm/…); Mat/MatExpr surface lives in
//    core_ops.inc.
//  * operations.hpp randu<_Tp>(), format(), print() family — static inline
//    but not Matx/Vec algebra (Mat/Formatter based), out of R1 scope.
//
// Mixed TUs (textual <opencv2/…> include + import): the header's internal
// operators and these module operators are both visible there — calls on
// value types become ambiguous. In mixed TUs prefer member/field forms.

export namespace cv {
// inline namespace: distinct entities from the upstream TU-local ones (no
// in-module redefinition), yet reachable as cv::… via qualified lookup and
// ADL on the importer side, where the upstream internals are invisible.
inline namespace opencv_m_repl {

// ── Matx comma-initializer (upstream: CV_DEPRECATED_EXTERNAL static inline) ─
// NOTE all two-typename / extra-NTTP upstream shapes in this file are
// reformulated over whole deduced operand types: serialized into a module
// BMI, templates whose parameters are not all pinned by the first argument
// (e.g. template<typename _Tp, typename _T2, int m, int n>, or the 4-int
// Matx multiply below) poison clang 20/22's lookup for that NAME — any use
// of the operator on ANY cv type in an importer segfaults the frontend
// (AddArgumentDependentLookupCandidates / InstantiateFunctionDefinition;
// gcc16 fine; minimal repros archived 2026-07-21). The whole-type form has
// identical call semantics; in mixed TUs upstream's exact-pattern
// static-inline templates are more specialized and win (they are TU-local
// and defined there), pure-import TUs only ever see these.
template<typename _MA, typename _T2>
    requires opencv_m_pick<typename _MA::value_type>
          && __is_same(_MA, typename _MA::mat_type)
CV_DEPRECATED_EXTERNAL inline
MatxCommaInitializer<typename _MA::value_type, (int)_MA::rows, (int)_MA::cols>
operator << (const _MA& mtx, _T2 val)
{
    using _Tp = typename _MA::value_type;
    MatxCommaInitializer<_Tp, (int)_MA::rows, (int)_MA::cols> ci((_MA*)&mtx);
    ci.dst->val[ci.idx++] = cv::opencv_m_repl::saturate_cast<_Tp>(val);
    return ci;
}

// ── Matx reductions (upstream: static inline, matx.hpp/matx.inl.hpp) ────
// determinant: closed forms of the internal::Matx_DetOp specializations for
// m ≤ 3; the general branch matches the primary template — in-place LU via
// the CV_EXPORTS cv::LU proxy (instantiable for float/double only, as
// upstream). Small-size arithmetic runs in _Tp and converts to double at
// return, exactly like the upstream functors.
// determinant: not the upstream-shaped `template<typename _Tp, int m>
// (const Matx<_Tp, m, m>&)` form — the repeated NTTP inside one parameter
// type is another spelling of the clang 20/22 BMI ADL-poisoning bug that hit
// operator* below (any lookup of the NAME determinant in an importer
// segfaults). Deduce the Matx whole, encode squareness in the constraint.
template<typename _MA>
    requires opencv_m_pick<typename _MA::value_type>
          && __is_same(_MA, typename _MA::mat_type)
          && ((int)_MA::rows == (int)_MA::cols)
inline double determinant(const _MA& a)
{
    using _Tp = typename _MA::value_type;
    constexpr int m = (int)_MA::rows;
    if constexpr (m == 1)
        return a.val[0];
    else if constexpr (m == 2)
        return a.val[0]*a.val[3] - a.val[1]*a.val[2];
    else if constexpr (m == 3)
        return a.val[0]*(a.val[4]*a.val[8] - a.val[7]*a.val[5]) -
               a.val[1]*(a.val[3]*a.val[8] - a.val[6]*a.val[5]) +
               a.val[2]*(a.val[3]*a.val[7] - a.val[6]*a.val[4]);
    else
    {
        Matx<_Tp, m, m> temp = a;
        double p = cv::LU(temp.val, m*sizeof(_Tp), m, (_Tp*)0, 0, 0);
        if (p == 0)
            return p;
        for (int i = 0; i < m; i++)
            p *= temp.val[i*m + i];
        return p;
    }
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline double trace(const Matx<_Tp, m, n>& a)
{
    _Tp s = 0;    // upstream accumulates in _Tp, converts to double at return
    for (int i = 0; i < (m < n ? m : n); i++)
        s += a.val[i*n + i];
    return s;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline double norm(const Matx<_Tp, m, n>& M)
{
    double s = 0; // upstream: normL2Sqr<_Tp, double> — double accumulator
    for (int i = 0; i < m*n; i++)
    {
        double v = (double)M.val[i];
        s += v*v;
    }
    return __builtin_sqrt(s);
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline double norm(const Matx<_Tp, m, n>& M, int normType)
{
    // upstream: normInf/normL1/normL2Sqr<_Tp, DataType<_Tp>::work_type>
    typedef typename DataType<_Tp>::work_type work_type;
    work_type s = 0;
    switch (normType) {
    case NORM_INF:
        for (int i = 0; i < m*n; i++)
        {
            work_type v = (work_type)cv::cv_abs(M.val[i]);
            if (v > s) s = v;
        }
        return (double)s;
    case NORM_L1:
        for (int i = 0; i < m*n; i++)
            s += cv::cv_abs(M.val[i]);
        return (double)s;
    case NORM_L2SQR:
        for (int i = 0; i < m*n; i++)
        {
            work_type v = (work_type)M.val[i];
            s += v*v;
        }
        return (double)s;
    default:
    case NORM_L2:
        for (int i = 0; i < m*n; i++)
        {
            work_type v = (work_type)M.val[i];
            s += v*v;
        }
        return __builtin_sqrt((double)s);
    }
}

// ── Matx algebra (upstream: static inline, matx.hpp/matx.inl.hpp) ───────
template<typename _MA, typename _MB>
    requires opencv_m_pick<typename _MA::value_type>
          && __is_same(_MA, typename _MA::mat_type)
          && __is_same(_MB, Matx<typename _MB::value_type, (int)_MA::rows, (int)_MA::cols>)
inline _MA& operator += (_MA& a, const _MB& b)
{
    using _Tp1 = typename _MA::value_type;
    for (int i = 0; i < (int)_MA::channels; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp1>(a.val[i] + b.val[i]);
    return a;
}
template<typename _MA, typename _MB>
    requires opencv_m_pick<typename _MA::value_type>
          && __is_same(_MA, typename _MA::mat_type)
          && __is_same(_MB, Matx<typename _MB::value_type, (int)_MA::rows, (int)_MA::cols>)
inline _MA& operator -= (_MA& a, const _MB& b)
{
    using _Tp1 = typename _MA::value_type;
    for (int i = 0; i < (int)_MA::channels; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp1>(a.val[i] - b.val[i]);
    return a;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n> operator + (const Matx<_Tp, m, n>& a, const Matx<_Tp, m, n>& b)
{
    Matx<_Tp, m, n> M;
    for (int i = 0; i < m*n; i++)
        M.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] + b.val[i]);
    return M;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n> operator - (const Matx<_Tp, m, n>& a, const Matx<_Tp, m, n>& b)
{
    Matx<_Tp, m, n> M;
    for (int i = 0; i < m*n; i++)
        M.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] - b.val[i]);
    return M;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n>& operator *= (Matx<_Tp, m, n>& a, int alpha)
{
    for (int i = 0; i < m*n; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return a;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n>& operator *= (Matx<_Tp, m, n>& a, float alpha)
{
    for (int i = 0; i < m*n; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return a;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n>& operator *= (Matx<_Tp, m, n>& a, double alpha)
{
    for (int i = 0; i < m*n; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return a;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n> operator * (const Matx<_Tp, m, n>& a, int alpha)
{
    Matx<_Tp, m, n> M;
    for (int i = 0; i < m*n; i++)
        M.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return M;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n> operator * (const Matx<_Tp, m, n>& a, float alpha)
{
    Matx<_Tp, m, n> M;
    for (int i = 0; i < m*n; i++)
        M.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return M;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n> operator * (const Matx<_Tp, m, n>& a, double alpha)
{
    Matx<_Tp, m, n> M;
    for (int i = 0; i < m*n; i++)
        M.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return M;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n> operator * (int alpha, const Matx<_Tp, m, n>& a)
{
    Matx<_Tp, m, n> M;
    for (int i = 0; i < m*n; i++)
        M.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return M;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n> operator * (float alpha, const Matx<_Tp, m, n>& a)
{
    Matx<_Tp, m, n> M;
    for (int i = 0; i < m*n; i++)
        M.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return M;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n> operator * (double alpha, const Matx<_Tp, m, n>& a)
{
    Matx<_Tp, m, n> M;
    for (int i = 0; i < m*n; i++)
        M.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return M;
}
// Matx /= : upstream assigns the plain quotient — conversion, NO saturate.
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n>& operator /= (Matx<_Tp, m, n>& a, float alpha)
{
    for (int i = 0; i < m*n; i++)
        a.val[i] = _Tp(a.val[i] / alpha);
    return a;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n>& operator /= (Matx<_Tp, m, n>& a, double alpha)
{
    for (int i = 0; i < m*n; i++)
        a.val[i] = _Tp(a.val[i] / alpha);
    return a;
}
// Matx / : upstream scales by the reciprocal through Matx_ScaleOp — saturates.
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n> operator / (const Matx<_Tp, m, n>& a, float alpha)
{
    float ialpha = 1.f/alpha;
    Matx<_Tp, m, n> M;
    for (int i = 0; i < m*n; i++)
        M.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * ialpha);
    return M;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n> operator / (const Matx<_Tp, m, n>& a, double alpha)
{
    double ialpha = 1./alpha;
    Matx<_Tp, m, n> M;
    for (int i = 0; i < m*n; i++)
        M.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * ialpha);
    return M;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n> operator - (const Matx<_Tp, m, n>& a)
{
    Matx<_Tp, m, n> M; // upstream negates via Matx_ScaleOp with int -1
    for (int i = 0; i < m*n; i++)
        M.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * -1);
    return M;
}
// Matx x Matx multiplication. NOT the upstream-shaped 4-parameter form
// (template<typename _Tp, int m, int n, int l> over Matx<_Tp,m,l> x
// Matx<_Tp,l,n>): serialized into a module BMI, that exact shape poisons
// clang 20/22's ADL for the NAME operator* — every `x * y` on ANY cv type
// in an importer segfaults the compiler frontend inside
// AddArgumentDependentLookupCandidates (gcc16 is fine; minimal repro
// archived 2026-07-21). Deduce the two Matx operands whole instead:
// mat_type self-check keeps it Matx-only (a Vec's mat_type is Matx<T,cn,1>,
// never Vec itself), the value_type/cols==rows checks encode the same
// dimension contract. In mixed TUs upstream's exact-pattern static-inline
// template is more specialized and wins (it is TU-local and defined there);
// pure-import TUs only ever see this one.
template<typename _MA, typename _MB>
    requires opencv_m_pick<typename _MA::value_type>
          && __is_same(_MA, typename _MA::mat_type)
          && __is_same(_MB, typename _MB::mat_type)
          && __is_same(typename _MA::value_type, typename _MB::value_type)
          && ((int)_MA::cols == (int)_MB::rows)
inline Matx<typename _MA::value_type, (int)_MA::rows, (int)_MB::cols>
operator * (const _MA& a, const _MB& b)
{
    using _Tp = typename _MA::value_type;
    constexpr int m = (int)_MA::rows, n = (int)_MB::cols, l = (int)_MA::cols;
    Matx<_Tp, m, n> M; // upstream Matx_MatMulOp: _Tp accumulator, no saturate
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++)
        {
            _Tp s = 0;
            for (int k = 0; k < l; k++)
                s += a.val[i*l + k] * b.val[k*n + j];
            M.val[i*n + j] = s;
        }
    return M;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
Vec<_Tp, m> operator * (const Matx<_Tp, m, n>& a, const Vec<_Tp, n>& b)
{
    Vec<_Tp, m> v; // upstream: Matx<_Tp,m,1> MatMulOp, reinterpreted as Vec
    for (int i = 0; i < m; i++)
    {
        _Tp s = 0;
        for (int k = 0; k < n; k++)
            s += a.val[i*n + k] * b.val[k];
        v.val[i] = s;
    }
    return v;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
bool operator == (const Matx<_Tp, m, n>& a, const Matx<_Tp, m, n>& b)
{
    for (int i = 0; i < m*n; i++)
        if (a.val[i] != b.val[i]) return false;
    return true;
}
template<typename _Tp, int m, int n> requires opencv_m_pick<_Tp> inline
bool operator != (const Matx<_Tp, m, n>& a, const Matx<_Tp, m, n>& b)
{
    for (int i = 0; i < m*n; i++)
        if (a.val[i] != b.val[i]) return true;
    return false;
}

// ── Vec comma-initializer (upstream: CV_DEPRECATED_EXTERNAL static inline) ─
template<typename _VA, typename _T2>
    requires opencv_m_pick<typename _VA::value_type>
          && __is_same(_VA, Vec<typename _VA::value_type, (int)_VA::channels>)
CV_DEPRECATED_EXTERNAL inline
VecCommaInitializer<typename _VA::value_type, (int)_VA::channels>
operator << (const _VA& vec, _T2 val)
{
    using _Tp = typename _VA::value_type;
    VecCommaInitializer<_Tp, (int)_VA::channels> ci((_VA*)&vec);
    ci.dst->val[ci.idx++] = cv::opencv_m_repl::saturate_cast<_Tp>(val);
    return ci;
}

// ── Vec algebra (upstream: static inline, matx.hpp/matx.inl.hpp) ────────
// Note: ==/!= and Matx-typed scalar overloads apply to Vec through
// derived-to-base deduction against the Matx templates above, as upstream.
template<typename _VA, typename _VB>
    requires opencv_m_pick<typename _VA::value_type>
          && __is_same(_VA, Vec<typename _VA::value_type, (int)_VA::channels>)
          && __is_same(_VB, Vec<typename _VB::value_type, (int)_VA::channels>)
inline _VA& operator += (_VA& a, const _VB& b)
{
    using _Tp1 = typename _VA::value_type;
    for (int i = 0; i < (int)_VA::channels; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp1>(a.val[i] + b.val[i]);
    return a;
}
template<typename _VA, typename _VB>
    requires opencv_m_pick<typename _VA::value_type>
          && __is_same(_VA, Vec<typename _VA::value_type, (int)_VA::channels>)
          && __is_same(_VB, Vec<typename _VB::value_type, (int)_VA::channels>)
inline _VA& operator -= (_VA& a, const _VB& b)
{
    using _Tp1 = typename _VA::value_type;
    for (int i = 0; i < (int)_VA::channels; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp1>(a.val[i] - b.val[i]);
    return a;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn> operator + (const Vec<_Tp, cn>& a, const Vec<_Tp, cn>& b)
{
    Vec<_Tp, cn> v;
    for (int i = 0; i < cn; i++)
        v.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] + b.val[i]);
    return v;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn> operator - (const Vec<_Tp, cn>& a, const Vec<_Tp, cn>& b)
{
    Vec<_Tp, cn> v;
    for (int i = 0; i < cn; i++)
        v.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] - b.val[i]);
    return v;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn>& operator *= (Vec<_Tp, cn>& a, int alpha)
{
    for (int i = 0; i < cn; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return a;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn>& operator *= (Vec<_Tp, cn>& a, float alpha)
{
    for (int i = 0; i < cn; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return a;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn>& operator *= (Vec<_Tp, cn>& a, double alpha)
{
    for (int i = 0; i < cn; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return a;
}
// Vec /= and / : upstream multiplies by the reciprocal and saturates
// (double reciprocal for int and double, float reciprocal for float).
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn>& operator /= (Vec<_Tp, cn>& a, int alpha)
{
    double ialpha = 1./alpha;
    for (int i = 0; i < cn; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * ialpha);
    return a;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn>& operator /= (Vec<_Tp, cn>& a, float alpha)
{
    float ialpha = 1.f/alpha;
    for (int i = 0; i < cn; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * ialpha);
    return a;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn>& operator /= (Vec<_Tp, cn>& a, double alpha)
{
    double ialpha = 1./alpha;
    for (int i = 0; i < cn; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * ialpha);
    return a;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn> operator * (const Vec<_Tp, cn>& a, int alpha)
{
    Vec<_Tp, cn> v;
    for (int i = 0; i < cn; i++)
        v.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return v;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn> operator * (int alpha, const Vec<_Tp, cn>& a)
{
    Vec<_Tp, cn> v;
    for (int i = 0; i < cn; i++)
        v.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return v;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn> operator * (const Vec<_Tp, cn>& a, float alpha)
{
    Vec<_Tp, cn> v;
    for (int i = 0; i < cn; i++)
        v.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return v;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn> operator * (float alpha, const Vec<_Tp, cn>& a)
{
    Vec<_Tp, cn> v;
    for (int i = 0; i < cn; i++)
        v.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return v;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn> operator * (const Vec<_Tp, cn>& a, double alpha)
{
    Vec<_Tp, cn> v;
    for (int i = 0; i < cn; i++)
        v.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return v;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn> operator * (double alpha, const Vec<_Tp, cn>& a)
{
    Vec<_Tp, cn> v;
    for (int i = 0; i < cn; i++)
        v.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * alpha);
    return v;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn> operator / (const Vec<_Tp, cn>& a, int alpha)
{
    double ialpha = 1./alpha;
    Vec<_Tp, cn> v;
    for (int i = 0; i < cn; i++)
        v.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * ialpha);
    return v;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn> operator / (const Vec<_Tp, cn>& a, float alpha)
{
    float ialpha = 1.f/alpha;
    Vec<_Tp, cn> v;
    for (int i = 0; i < cn; i++)
        v.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * ialpha);
    return v;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn> operator / (const Vec<_Tp, cn>& a, double alpha)
{
    double ialpha = 1./alpha;
    Vec<_Tp, cn> v;
    for (int i = 0; i < cn; i++)
        v.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(a.val[i] * ialpha);
    return v;
}
template<typename _Tp, int cn> requires opencv_m_pick<_Tp> inline
Vec<_Tp, cn> operator - (const Vec<_Tp, cn>& a)
{
    Vec<_Tp, cn> t; // upstream negates directly — unlike the Matx * -1 form
    for (int i = 0; i < cn; i++)
        t.val[i] = cv::opencv_m_repl::saturate_cast<_Tp>(-a.val[i]);
    return t;
}

// ── Vec4 quaternion product (upstream: plain `inline`, matx.inl.hpp — still
// invisible to importers as an unexported GMF entity, so replaced too) ───
template<typename _Tp> requires opencv_m_pick<_Tp> inline
Vec<_Tp, 4> operator * (const Vec<_Tp, 4>& v1, const Vec<_Tp, 4>& v2)
{
    return Vec<_Tp, 4>(
        cv::opencv_m_repl::saturate_cast<_Tp>(v1.val[0]*v2.val[0] - v1.val[1]*v2.val[1] - v1.val[2]*v2.val[2] - v1.val[3]*v2.val[3]),
        cv::opencv_m_repl::saturate_cast<_Tp>(v1.val[0]*v2.val[1] + v1.val[1]*v2.val[0] + v1.val[2]*v2.val[3] - v1.val[3]*v2.val[2]),
        cv::opencv_m_repl::saturate_cast<_Tp>(v1.val[0]*v2.val[2] - v1.val[1]*v2.val[3] + v1.val[2]*v2.val[0] + v1.val[3]*v2.val[1]),
        cv::opencv_m_repl::saturate_cast<_Tp>(v1.val[0]*v2.val[3] + v1.val[1]*v2.val[2] - v1.val[2]*v2.val[1] + v1.val[3]*v2.val[0]));
}
template<typename _Tp> requires opencv_m_pick<_Tp> inline
Vec<_Tp, 4>& operator *= (Vec<_Tp, 4>& v1, const Vec<_Tp, 4>& v2)
{
    Vec<_Tp, 4> t(
        cv::opencv_m_repl::saturate_cast<_Tp>(v1.val[0]*v2.val[0] - v1.val[1]*v2.val[1] - v1.val[2]*v2.val[2] - v1.val[3]*v2.val[3]),
        cv::opencv_m_repl::saturate_cast<_Tp>(v1.val[0]*v2.val[1] + v1.val[1]*v2.val[0] + v1.val[2]*v2.val[3] - v1.val[3]*v2.val[2]),
        cv::opencv_m_repl::saturate_cast<_Tp>(v1.val[0]*v2.val[2] - v1.val[1]*v2.val[3] + v1.val[2]*v2.val[0] + v1.val[3]*v2.val[1]),
        cv::opencv_m_repl::saturate_cast<_Tp>(v1.val[0]*v2.val[3] + v1.val[1]*v2.val[2] - v1.val[2]*v2.val[1] + v1.val[3]*v2.val[0]));
    v1 = t;
    return v1;
}

} // inline namespace opencv_m_repl
} // export namespace cv
