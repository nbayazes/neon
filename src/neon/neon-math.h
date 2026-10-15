#pragma once
#include <algorithm>
#include <DirectXMath.h>
#include <DirectXTK12/SimpleMath.h>
#include <numbers>
#include "neon-types.h"
#include "Random.h"

namespace neon {
using DirectX::SimpleMath::Vector2;
using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Vector4;
using DirectX::SimpleMath::Matrix;
using DirectX::SimpleMath::Plane;
using DirectX::SimpleMath::Color;
using DirectX::SimpleMath::Ray;
using DirectX::SimpleMath::Quaternion;
using uint3 = DirectX::XMUINT3;

//Some handy constants for interacting with fixed precision values
constexpr auto F1_0 = 0x10000;

constexpr float DegToRad = (float)std::numbers::pi / 180.0f;
constexpr float RadToDeg = 180.0f / (float)std::numbers::pi;

struct uint2 : DirectX::XMUINT2 {
    uint2() noexcept : DirectX::XMUINT2(0, 0) {}
    constexpr explicit uint2(unsigned int ix) noexcept : DirectX::XMUINT2(ix, ix) {}
    constexpr uint2(unsigned int ix, unsigned int iy) noexcept : DirectX::XMUINT2(ix, iy) {}
    explicit uint2(_In_reads_(2) const unsigned int* pArray) noexcept : DirectX::XMUINT2(pArray) {}
    //uint2(DirectX::FXMVECTOR V) noexcept { DirectX::XMStoreUInt2(this, V); }
    uint2(const DirectX::XMUINT2& v) noexcept {
        this->x = v.x;
        this->y = v.y;
    }

    //explicit uint2(const DirectX::XMVECTORU32& F) noexcept { this->x = F.u[0]; this->y = F.u[1]; }

    ~uint2() {}

    uint2(const uint2&) = default;
    uint2& operator=(const uint2&) = default;

    uint2(uint2&&) = default;
    uint2& operator=(uint2&&) = default;

    bool operator ==(const uint2& v) const noexcept {
        return v.x == x && v.y == y;
    }

    bool operator !=(const uint2& v) const noexcept {
        return v.x != x || v.y != y;
    }
};

struct Matrix3x3 : DirectX::XMFLOAT3X3 {
    Matrix3x3() noexcept
        : XMFLOAT3X3(1.f, 0, 0,
                     0, 1.f, 0,
                     0, 0, 1.f) {}

    // right, up, forward vectors
    explicit Matrix3x3(const Vector3& r0, const Vector3& r1, const Vector3& r2) noexcept
        : XMFLOAT3X3(r0.x, r0.y, r0.z,
                     r1.x, r1.y, r1.z,
                     r2.x, r2.y, r2.z) {}

    explicit Matrix3x3(const Matrix& m) noexcept
        : XMFLOAT3X3(m._11, m._12, m._13,
                     m._21, m._22, m._23,
                     m._31, m._32, m._33) {}

    // Constructs a rotation matrix from a forward and up vector
    explicit Matrix3x3(Vector3 forward, Vector3 up) noexcept
        : XMFLOAT3X3() {
        forward.Normalize();
        up.Normalize();
        auto right = up.Cross(forward);
        Right(right);
        Up(forward.Cross(right));
        Forward(forward);
    }

    Vector3 Up() const noexcept { return { _21, _22, _23 }; }

    void Up(const Vector3& v) noexcept {
        _21 = v.x;
        _22 = v.y;
        _23 = v.z;
    }

    Vector3 Down() const noexcept { return { -_21, -_22, -_23 }; }

    void Down(const Vector3& v) noexcept {
        _21 = -v.x;
        _22 = -v.y;
        _23 = -v.z;
    }

    Vector3 Right() const noexcept { return { _11, _12, _13 }; }

    void Right(const Vector3& v) noexcept {
        _11 = v.x;
        _12 = v.y;
        _13 = v.z;
    }

    Vector3 Left() const noexcept { return { -_11, -_12, -_13 }; }

    void Left(const Vector3& v) noexcept {
        _11 = -v.x;
        _12 = -v.y;
        _13 = -v.z;
    }

    Vector3 Forward() const noexcept { return { -_31, -_32, -_33 }; }

    void Forward(const Vector3& v) noexcept {
        _31 = -v.x;
        _32 = -v.y;
        _33 = -v.z;
    }

    Vector3 Backward() const noexcept { return { _31, _32, _33 }; }

    void Backward(const Vector3& v) noexcept {
        _31 = v.x;
        _32 = v.y;
        _33 = v.z;
    }

    Matrix3x3& operator *=(const Matrix& matrix) {
        DirectX::XMStoreFloat3x3(this, Matrix(*this) * matrix);
        return *this;
    }

    void Normalize() {
        Vector3 forward, up, right;
        Forward().Normalize(forward);
        Up().Normalize(up);
        Right().Normalize(right);
        Forward(forward);
        Up(up);
        Right(right);
    }

    static const Matrix3x3 Zero;
};

inline const Matrix3x3 Matrix3x3::Zero = Matrix3x3(
    Vector3::Zero,
    Vector3::Zero,
    Vector3::Zero
);



inline Vector3 AverageVectors(span<Vector3> verts) {
    Vector3 average;

    for (int i = 0; i < verts.size(); i++)
        average += verts[i];

    average /= (float)verts.size();
    return average;
}

inline Vector3 AverageVectors(span<const Vector3*> verts) {
    Vector3 average;

    for (int i = 0; i < verts.size(); i++)
        average += *verts[i];

    average /= (float)verts.size();
    return average;
}


inline Vector3 VectorMin(const Vector3& a, const Vector3& b) {
    Vector3 r;
    r.x = a.x < b.x ? a.x : b.x;
    r.y = a.y < b.y ? a.y : b.y;
    r.z = a.z < b.z ? a.z : b.z;
    return r;
}

inline Vector3 VectorMax(const Vector3& a, const Vector3& b) {
    Vector3 r;
    r.x = a.x > b.x ? a.x : b.x;
    r.y = a.y > b.y ? a.y : b.y;
    r.z = a.z > b.z ? a.z : b.z;
    return r;
}

constexpr DirectX::XMVECTORF32 UNIT_VECTOR_EPSILON = { { { 1.0e-4f, 1.0e-4f, 1.0e-4f, 1.0e-4f } } };

inline bool IsNormalized(const Vector3& v) {
    using namespace DirectX;
    auto difference = XMVectorSubtract(XMVector3Length(v), XMVectorSplatOne());
    return XMVector4Less(XMVectorAbs(difference), UNIT_VECTOR_EPSILON);
}

inline bool IsZero(const Vector3& v) {
    using namespace DirectX;
    return XMVector4Less(XMVectorAbs(v), UNIT_VECTOR_EPSILON);
}

// Converts a direction vector into a rotation matrix
inline Matrix DirectionToRotationMatrix(const Vector3& direction, float roll = 0) {
    assert(IsNormalized(direction));
    auto pitch = asin(std::clamp(direction.y, -1.0f, 1.0f));
    auto yaw = atan2(-direction.z, direction.x);
    return Matrix::CreateFromYawPitchRoll(yaw, roll, pitch);
}

// Creates a world rotation matrix from a vector
inline Matrix3x3 VectorToRotation(const Vector3& forward, Vector3 up = Vector3::Zero, Vector3 right = Vector3::Zero) {
    ASSERT(IsNormalized(forward));

    if (up == Vector3::Zero && right == Vector3::Zero) {
        // neither up or right provided
        if (forward.x == 0 && forward.z == 0) {
            // vec is straight up or down
            right = Vector3(1, 0, 0);
            up.z = forward.y < 0 ? 1.0f : -1.0f;
        }
        else {
            right = Vector3(forward.z, 0, -forward.x);
            right.Normalize();
            up = forward.Cross(right);
        }
    }
    else if (right != Vector3::Zero) {
        up = forward.Cross(right);
        up.Normalize();
        right = up.Cross(forward); // recalculate right vec in case it wasn't perpendicular
    }
    else if (up != Vector3::Zero) {
        right = up.Cross(forward);
        right.Normalize();
        up = forward.Cross(right);
    }

    return Matrix3x3{ right, up, forward };
}

// Projects a ray onto a plane. What happens if parallel?
inline Vector3 ProjectRayOntoPlane(const Ray& ray, const Vector3& planeOrigin, Vector3 planeNormal) {
    assert(IsNormalized(planeNormal));
    auto length = planeNormal.Dot(ray.position - planeOrigin) / planeNormal.Dot(-ray.direction);
    return ray.position + ray.direction * length;
}

inline Vector3 ProjectPointOntoPlane(const Vector3& point, const Vector3& planeOrigin, Vector3 planeNormal) {
    // q - dot(q - p, n) * n
    assert(IsNormalized(planeNormal));
    return point - (point - planeOrigin).Dot(planeNormal) * planeNormal;
}

inline Vector3 ProjectPointOntoPlane(const Vector3& point, const Plane& plane) {
    // p' = p - (n . p + d) * n
    return point - (plane.DotNormal(point) + plane.D()) * plane.Normal();
}

inline float DistanceFromPlane(const Vector3& point, const Vector3& planeOrigin, Vector3 planeNormal) {
    return planeNormal.Dot(point - planeOrigin);
}

inline float PointToLineDistance(const Vector3& point, const Vector3& v0, const Vector3& v1) {
    // normalize all points to vector 1
    auto A = v0 - point;
    auto B = v1 - point;

    // use formula from page 505 of "Calculase and Analytical Geometry" Fifth Addition
    // by Tommas/Finney, Addison-Wesley Publishing Company, June 1981
    //          B * A
    // B2 = B - ----- A
    //          A * A

    float a2 = A.Dot(A);
    float c = a2 != 0 ? B.Dot(A) / a2 : 0;
    auto C = B - (A * c);
    return C.Length();
}

inline float PointToPlaneDistance(const Vector3& point, const Vector3& planeOrigin, Vector3 planeNormal) {
    assert(IsNormalized(planeNormal));
    auto w = point - planeOrigin;
    auto v = planeNormal;
    return v.Dot(w) / v.Length();
}

// v0 and v1 must be normalized. Returns [-PI, PI]
inline float AngleBetweenVectors(const Vector3& v0, const Vector3& v1, const Vector3& normal) {
    auto dot = v0.Dot(v1);
    auto cross = v0.Cross(v1);
    auto angle = atan2(cross.Length(), dot);
    if (normal.Dot(cross) < 0) angle = -angle;
    return angle;
}

// v0 and v1 must be normalized. Returns [0, PI]
inline float AngleBetweenVectors(const Vector3& v0, const Vector3& v1) {
    auto dot = v0.Dot(v1);
    if (dot <= -0.999f) return (float)std::numbers::pi;
    return acos(v0.Dot(v1));
}

// v0 and v1 must be normalized. Returns [0, PI]
inline float AngleBetweenVectors(const Vector2& v0, const Vector2& v1) {
    return acos(v0.Dot(v1));
}

// Rotates vector around 0,0 by an angle in radians.
inline Vector2 RotateVector(const Vector2& v, float angle) {
    return {
        v.x * cos(angle) - v.y * sin(angle),
        v.x * sin(angle) + v.y * cos(angle)
    };
}

// Returns [-PI, PI]
inline float AngleBetweenPoints(const Vector3& a, const Vector3& b, const Vector3& origin, const Vector3& normal) {
    auto v0 = a - origin;
    auto v1 = b - origin;
    v0.Normalize();
    v1.Normalize();
    return AngleBetweenVectors(v0, v1, normal);
}

// Uniformly distributed direction on the unit sphere
inline Vector3 RandomDirection() {
    auto t = RandomFloat() * DirectX::XM_2PI;
    float u = RandomFloat(-1, 1);
    auto sqr = sqrt(1 - u * u);
    return { cos(t) * sqr, sin(t) * sqr, u };
}

inline Vector3 RandomPointOnHemisphere() {
    auto a = RandomFloat() * DirectX::XM_2PI;
    auto z = std::asin(std::sqrt(RandomFloat()));
    return { sin(z) * cos(a), sin(z) * sin(a), cos(z) };
}

// Uniformly distributed direction on the unit sphere
//inline Vector3 RandomDirection() {
//    float z = RandomN11();
//    float angle = Random() * DirectX::XM_2PI;
//    float r = sqrt(1.0f - z * z);
//    return { r * cos(angle), r * sin(angle), z };
//}

// Returns a random point on the edge of a circle
inline Vector3 RandomPointOnCircle(float radius = 1) {
    auto t = RandomFloat() * DirectX::XM_2PI;
    return { cos(t) * radius, sin(t) * radius, 0 };
}

// Random direction inside a cone. A coneRadius of 1 spreads particles 45 degrees off of direction.
inline Vector3 RandomConeDirection(const Matrix3x3& rotation, float coneRadius) {
    // sample a disc at a distance of one, tan(45 degrees) is one
    float spread = tan(std::min(coneRadius, 1.0f) * 45.0f * DegToRad);
    float r = sqrt(RandomFloat()) * spread;
    float angle = RandomFloat() * DirectX::XM_2PI;

    auto result = rotation.Right() * (r * cos(angle)) + rotation.Up() * (r * sin(angle)) + rotation.Forward();
    result.Normalize();
    return result;
}

// Uniformly distributed point inside a sphere
inline Vector3 RandomPointOnSphere(float radius) {
    // the cube root spreads points evenly by volume instead of clumping them in the center
    return RandomDirection() * (radius * pow(RandomFloat(), 1.0f / 3.0f));
}


constexpr float FixToFloat(fix f) {
    return (float)f / (float)(1 << 16);
}

constexpr int MAX_FIX = 32768; // Maximum fixed point value
constexpr int MIN_FIX = -32769; // Minimum fixed point value

constexpr fix FloatToFix(float f) {
    assert(f < MAX_FIX && f > MIN_FIX); // out of range
    return (fix)(f * (1 << 16));
}

constexpr float Step(float value, float step) {
    if (step == 0.0f) return value;
    return step * std::round(value / step);
}

// Modulus division without negative numbers
constexpr auto ModSafe(std::signed_integral auto k, std::signed_integral auto n) {
    return (k %= n) < 0 ? k + n : k;
}

inline float Saturate(float v) {
    return v < 0.0f ? 0.0f : v  > 1.0f ? 1.0f : v;
}

}

namespace DirectX {
// https://github.com/microsoft/DirectXMath/pull/334/changes

//------------------------------------------------------------------------------
// Infinite reversed-z perspective projection (FarZ = +infinity, depth maps NearZ->1, Infinity->0)
// This avoids NaN that would result from passing infinity to XMMatrixPerspectiveFovLH.
// Left-handed coordinate system.
inline XMMATRIX XM_CALLCONV XMMatrixPerspectiveFovInfiniteReverseZLH
(
    float FovAngleY,
    float AspectRatio,
    float NearZ
) noexcept {
    assert(NearZ > 0.f);
    assert(!XMScalarNearEqual(FovAngleY, 0.0f, 0.00001f * 2.0f));
    assert(!XMScalarNearEqual(AspectRatio, 0.0f, 0.00001f));

#if defined(_XM_NO_INTRINSICS_)

    float    SinFov;
    float    CosFov;
    XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);

    float Height = CosFov / SinFov;
    float Width = Height / AspectRatio;

    // As FarZ -> +inf in XMMatrixPerspectiveFovLH:
    //   fRange = FarZ / (FarZ - NearZ) -> 1
    //   -fRange * NearZ -> -NearZ
    // But for reversed-z we want depth in [1, 0] instead of [0, 1],
    // so M[2][2] = 0 and M[3][2] = NearZ (limit of fRange*(FarZ-NearZ)/FarZ * ... see derivation).
    // Derivation: reversed-z maps NearZ->1, inf->0. The standard row-major DX projection:
    //   z' = M22*z + M32,  w' = z
    //   NDC_z = z'/w' = M22 + M32/z
    // For NearZ->1: M22 + M32/NearZ = 1
    // For inf->0:   M22 = 0
    // Therefore: M32 = NearZ, M22 = 0.
    XMMATRIX M;
    M.m[0][0] = Width;
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;

    M.m[1][0] = 0.0f;
    M.m[1][1] = Height;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;

    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = 0.0f;
    M.m[2][3] = 1.0f;

    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = NearZ;
    M.m[3][3] = 0.0f;
    return M;

#elif defined(_XM_ARM_NEON_INTRINSICS_)
    float    SinFov;
    float    CosFov;
    XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);

    float Height = CosFov / SinFov;
    float Width = Height / AspectRatio;
    const float32x4_t Zero = vdupq_n_f32(0);

    XMMATRIX M;
    M.r[0] = vsetq_lane_f32(Width, Zero, 0);
    M.r[1] = vsetq_lane_f32(Height, Zero, 1);
    // M.r[2] = (0, 0, 0, 1)  -- g_XMIdentityR3 is {0,0,0,1}
    M.r[2] = g_XMIdentityR3.v;
    // M.r[3] = (0, 0, NearZ, 0)
    M.r[3] = vsetq_lane_f32(NearZ, Zero, 2);
    return M;
#elif defined(_XM_SSE_INTRINSICS_)
    float    SinFov;
    float    CosFov;
    XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);

    float Height = CosFov / SinFov;
    // Note: This is recorded on the stack
    XMVECTOR rMem = {
        Height / AspectRatio,
        Height,
        0.0f,
        NearZ
    };
    // Copy from memory to SSE register
    XMVECTOR vValues = rMem;
    XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp, vValues);
    // Width,0,0,0
    XMMATRIX M;
    M.r[0] = vTemp;
    // 0,Height,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp, g_XMMaskY);
    M.r[1] = vTemp;
    // 0,0,0,1
    M.r[2] = g_XMIdentityR3;
    // 0,0,NearZ,0 -- place NearZ into lane z of a zero vector.
    // _mm_set_ss(NearZ) = {NearZ, 0, 0, 0}. _MM_SHUFFLE(3,0,0,0) picks:
    //   result = {g_XMZero[0], g_XMZero[0], b[0], b[3]} = {0, 0, NearZ, 0}.
    vTemp = _mm_shuffle_ps(g_XMZero, _mm_set_ss(NearZ), _MM_SHUFFLE(3, 0, 0, 0)); // (0, 0, NearZ, 0)
    M.r[3] = vTemp;
    return M;
#endif
}

//------------------------------------------------------------------------------
// Infinite reversed-z perspective projection (FarZ = +infinity, depth maps NearZ->1, Infinity->0)
// This avoids NaN that would result from passing infinity to XMMatrixPerspectiveFovRH.
// Right-handed coordinate system.
inline XMMATRIX XM_CALLCONV XMMatrixPerspectiveFovInfiniteReverseZRH
(
    float FovAngleY,
    float AspectRatio,
    float NearZ
) noexcept {
    assert(NearZ > 0.f);
    assert(!XMScalarNearEqual(FovAngleY, 0.0f, 0.00001f * 2.0f));
    assert(!XMScalarNearEqual(AspectRatio, 0.0f, 0.00001f));

#if defined(_XM_NO_INTRINSICS_)

    float    SinFov;
    float    CosFov;
    XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);

    float Height = CosFov / SinFov;
    float Width = Height / AspectRatio;

    // As FarZ -> +inf in XMMatrixPerspectiveFovRH (RH flips the sign of z in clip):
    //   M22 = 0, M32 = -NearZ
    // Derivation (same as LH but w' = -z for RH):
    //   NDC_z = M22 + M32/z. For inf->0: M22=0. For NearZ->1: M32 = -NearZ.
    XMMATRIX M;
    M.m[0][0] = Width;
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;

    M.m[1][0] = 0.0f;
    M.m[1][1] = Height;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;

    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = 0.0f;
    M.m[2][3] = -1.0f;

    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = -NearZ;
    M.m[3][3] = 0.0f;
    return M;

#elif defined(_XM_ARM_NEON_INTRINSICS_)
    float    SinFov;
    float    CosFov;
    XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);

    float Height = CosFov / SinFov;
    float Width = Height / AspectRatio;
    const float32x4_t Zero = vdupq_n_f32(0);

    XMMATRIX M;
    M.r[0] = vsetq_lane_f32(Width, Zero, 0);
    M.r[1] = vsetq_lane_f32(Height, Zero, 1);
    // M.r[2] = (0, 0, 0, -1) -- g_XMNegIdentityR3 is {0,0,0,-1}
    M.r[2] = g_XMNegIdentityR3.v;
    // M.r[3] = (0, 0, -NearZ, 0)
    M.r[3] = vsetq_lane_f32(-NearZ, Zero, 2);
    return M;
#elif defined(_XM_SSE_INTRINSICS_)
    float    SinFov;
    float    CosFov;
    XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);

    float Height = CosFov / SinFov;
    // Note: This is recorded on the stack
    XMVECTOR rMem = {
        Height / AspectRatio,
        Height,
        0.0f,
        -NearZ
    };
    // Copy from memory to SSE register
    XMVECTOR vValues = rMem;
    XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp, vValues);
    // Width,0,0,0
    XMMATRIX M;
    M.r[0] = vTemp;
    // 0,Height,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp, g_XMMaskY);
    M.r[1] = vTemp;
    // 0,0,0,-1
    M.r[2] = g_XMNegIdentityR3;
    // 0,0,-NearZ,0 -- place -NearZ into lane z of a zero vector.
    // _mm_set_ss(-NearZ) = {-NearZ, 0, 0, 0}. Same shuffle: {0, 0, -NearZ, 0}.
    vTemp = _mm_shuffle_ps(g_XMZero, _mm_set_ss(-NearZ), _MM_SHUFFLE(3, 0, 0, 0)); // (0, 0, -NearZ, 0)
    M.r[3] = vTemp;
    return M;
#endif
}

}
