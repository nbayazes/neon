#pragma once
#include <DirectXMath.h>
#include <DirectXTK12/SimpleMath.h>
#include <numbers>

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
};


}



namespace DirectX {

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

}
