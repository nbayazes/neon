#pragma once
#include <DirectXMath.h>
#include <DirectXTK12/SimpleMath.h>

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
