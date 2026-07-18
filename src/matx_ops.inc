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
template<typename _Tp, typename _T2, int m, int n> requires opencv_m_pick<_Tp> CV_DEPRECATED_EXTERNAL inline
MatxCommaInitializer<_Tp, m, n> operator << (const Matx<_Tp, m, n>& mtx, _T2 val)
{
    MatxCommaInitializer<_Tp, m, n> ci((Matx<_Tp, m, n>*)&mtx);
    ci.dst->val[ci.idx++] = cv::opencv_m_repl::saturate_cast<_Tp>(val);
    return ci;
}

// ── Matx reductions (upstream: static inline, matx.hpp/matx.inl.hpp) ────
// determinant: closed forms of the internal::Matx_DetOp specializations for
// m ≤ 3; the general branch matches the primary template — in-place LU via
// the CV_EXPORTS cv::LU proxy (instantiable for float/double only, as
// upstream). Small-size arithmetic runs in _Tp and converts to double at
// return, exactly like the upstream functors.
template<typename _Tp, int m> requires opencv_m_pick<_Tp> inline double determinant(const Matx<_Tp, m, m>& a)
{
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
template<typename _Tp1, typename _Tp2, int m, int n> requires opencv_m_pick<_Tp1> inline
Matx<_Tp1, m, n>& operator += (Matx<_Tp1, m, n>& a, const Matx<_Tp2, m, n>& b)
{
    for (int i = 0; i < m*n; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp1>(a.val[i] + b.val[i]);
    return a;
}
template<typename _Tp1, typename _Tp2, int m, int n> requires opencv_m_pick<_Tp1> inline
Matx<_Tp1, m, n>& operator -= (Matx<_Tp1, m, n>& a, const Matx<_Tp2, m, n>& b)
{
    for (int i = 0; i < m*n; i++)
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
template<typename _Tp, int m, int n, int l> requires opencv_m_pick<_Tp> inline
Matx<_Tp, m, n> operator * (const Matx<_Tp, m, l>& a, const Matx<_Tp, l, n>& b)
{
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
template<typename _Tp, typename _T2, int cn> requires opencv_m_pick<_Tp> CV_DEPRECATED_EXTERNAL inline
VecCommaInitializer<_Tp, cn> operator << (const Vec<_Tp, cn>& vec, _T2 val)
{
    VecCommaInitializer<_Tp, cn> ci((Vec<_Tp, cn>*)&vec);
    ci.dst->val[ci.idx++] = cv::opencv_m_repl::saturate_cast<_Tp>(val);
    return ci;
}

// ── Vec algebra (upstream: static inline, matx.hpp/matx.inl.hpp) ────────
// Note: ==/!= and Matx-typed scalar overloads apply to Vec through
// derived-to-base deduction against the Matx templates above, as upstream.
template<typename _Tp1, typename _Tp2, int cn> requires opencv_m_pick<_Tp1> inline
Vec<_Tp1, cn>& operator += (Vec<_Tp1, cn>& a, const Vec<_Tp2, cn>& b)
{
    for (int i = 0; i < cn; i++)
        a.val[i] = cv::opencv_m_repl::saturate_cast<_Tp1>(a.val[i] + b.val[i]);
    return a;
}
template<typename _Tp1, typename _Tp2, int cn> requires opencv_m_pick<_Tp1> inline
Vec<_Tp1, cn>& operator -= (Vec<_Tp1, cn>& a, const Vec<_Tp2, cn>& b)
{
    for (int i = 0; i < cn; i++)
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
