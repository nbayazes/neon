#pragma once

#include "neon-math.h"
#include "neon-types.h"

namespace neon {

using DirectX::SimpleMath::Viewport;
using DirectX::SimpleMath::Quaternion;

// Returns a frustum for the camera perspective in world space
inline DirectX::BoundingFrustum GetFrustum(const Vector3& cameraPos, const Matrix& view, const Matrix& projection) {
    DirectX::BoundingFrustum frustum;
    DirectX::BoundingFrustum::CreateFromMatrix(frustum, projection);
    DirectX::XMVECTOR s, r, t;
    DirectX::XMMatrixDecompose(&s, &r, &t, view);
    r = DirectX::XMQuaternionInverse(r);
    frustum.Transform(frustum, 1.0f, r, cameraPos);
    return frustum;
}

// Descent uses LH coordinate system
//
// Camera that uses a target point and an up vector to control the orientation.
// Supports orbit and mouselook controls.
class Camera {
    Vector3 _lerpStart, _lerpEnd;
    float _lerpTime{}, _lerpDuration{};
    Viewport _viewport = { 0, 0, 1024, 768, 1, 3000 };

public:
    float fovDeg = 60; // FOV in degrees
    Vector3 position = { 40, 0, 0 };
    Matrix view;
    Matrix projection;
    Matrix inverseProjection;

    Vector3 target = Vector3::Zero;
    Vector3 up = Vector3::UnitY;

    float minimumZoom = 5; // closest the camera can get to the target

    DirectX::BoundingFrustum frustum;
    Matrix viewProjection;
    //SegID Segment;

    uint2 GetViewportSize() const {
        return { (uint)_viewport.width, (uint)_viewport.height };
    }

    void SetViewport(const uint2& size) {
        _viewport.width = (float)size.x;
        _viewport.height = (float)size.y;
    }

    void SetClipPlanes(float nearClip, float farClip) {
        _viewport.minDepth = nearClip;
        _viewport.maxDepth = farClip;
    }

    float GetNearClip() const { return _viewport.minDepth; }
    float GetFarClip() const { return _viewport.maxDepth; }

    //void LookAtPerspective(float fovDeg) {
    //    View = DirectX::XMMatrixLookAtLH(_position, Target, Up);
    //    Projection = DirectX::XMMatrixPerspectiveFovLH(fovDeg * DegToRad, _viewport.AspectRatio(), _viewport.minDepth, _viewport.maxDepth);
    //    InverseProjection = Projection.Invert();
    //}

    //void LookAtOrthographic() {
    //    //View = Matrix::CreateLookAt(Position, Target, Up);
    //    View = DirectX::XMMatrixLookAtLH(_position, Target, Up);
    //    Projection = Matrix::CreateOrthographicOffCenter(0, _viewport.width, _viewport.height, 0, _viewport.minDepth, _viewport.maxDepth);
    //    InverseProjection = Projection.Invert();
    //}

    Matrix3x3 GetOrientation() const { return Matrix3x3(GetForward(), up); }

    void MoveTo(const Vector3& position_, const Vector3& target_, const Vector3& up_) {
        if (position == position_ && target == target_ && up == up_) return;
        position = position_;
        target = target_;

        // Recalculate
        auto forward = target - position;
        forward.Normalize();
        auto right = forward.Cross(up_);
        this->up = right.Cross(forward);
        this->up.Normalize();
    }

    void Rotate(float yaw, float pitch) {
        auto qyaw = Quaternion::CreateFromAxisAngle(up, yaw);
        auto qpitch = Quaternion::CreateFromAxisAngle(GetRight(), pitch);

        Vector3 offset = target - position;
        target = Vector3::Transform(offset, qyaw * qpitch) + position;
        up = Vector3::Transform(up, qpitch);
        up.Normalize();
    }

    void Roll(float roll) {
        auto qroll = Quaternion::CreateFromAxisAngle(GetForward(), roll);
        up = Vector3::Transform(up, qroll);
        up.Normalize();
    }

    // Orbits around the target point
    void Orbit(float yaw, float pitch) {
        Vector3 offset = position - target;
        auto qyaw = Quaternion::CreateFromAxisAngle(up, yaw);
        auto qpitch = Quaternion::CreateFromAxisAngle(up.Cross(offset), -pitch);

        position = Vector3::Transform(offset, qyaw * qpitch) + target;
        up = Vector3::Transform(up, qpitch);
        up.Normalize();
    }

    Vector3 GetForward() const {
        auto forward = target - position;
        forward.Normalize();
        return forward;
    }

    Vector3 GetRight() const {
        auto right = GetForward().Cross(up);
        right.Normalize();
        return right;
    }

    void Pan(float horizontal, float vertical) {
        auto right = GetRight();
        auto translation = right * horizontal + up * vertical;
        target += translation;
        position += translation;
    }

    void MoveForward(float value) {
        if (value == 0) return;
        auto step = GetForward() * value;
        position += step;
        target += step;
        CancelLerp();
    }

    void MoveBack(float value) {
        if (value == 0) return;
        auto step = -GetForward() * value;
        position += step;
        target += step;
        CancelLerp();
    }

    void MoveLeft(float value) {
        if (value == 0) return;
        auto step = GetRight() * value;
        position += step;
        target += step;
        CancelLerp();
    }

    void MoveRight(float value) {
        if (value == 0) return;
        auto step = -GetRight() * value;
        position += step;
        target += step;
        CancelLerp();
    }

    void MoveUp(float value) {
        if (value == 0) return;
        auto step = up * value;
        position += step;
        target += step;
        CancelLerp();
    }

    void MoveDown(float value) {
        if (value == 0) return;
        auto step = -up * value;
        position += step;
        target += step;
        CancelLerp();
    }

    void Zoom(const float& value) {
        Vector3 delta = target - position;
        delta.Normalize();
        // add the value along the direction of the vector
        Vector3 pos = position + delta * value;

        if (Vector3::Distance(pos, target) > minimumZoom)
            position = pos;
    }

    void ZoomIn() {
        auto delta = target - position;
        auto direction = delta;
        direction.Normalize();

        // scale zoom amount based on distance from target
        auto value = std::clamp(delta.Length() / 6, minimumZoom, 100.0f);
        Vector3 pos = position + direction * value;

        if (Vector3::Distance(pos, target) > minimumZoom)
            position = pos;
    }

    void ZoomOut() {
        auto delta = target - position;
        auto direction = delta;
        direction.Normalize();

        // scale zoom amount based on distance from target
        auto value = std::clamp(delta.Length() / 6, 10.0f, 100.0f);
        Vector3 pos = position - direction * value;
        position = pos;
    }

    // Unprojects a screen coordinate into world space along the near plane
    Vector3 Unproject(Vector2 screen, const Matrix& world = Matrix::Identity) const {
        return _viewport.Unproject({ screen.x, screen.y, 0 }, projection, view, world);
    }

    Ray UnprojectRay(Vector2 screen, const Matrix& world = Matrix::Identity) const {
        auto direction = Unproject(screen, world) - position;
        direction.Normalize();
        return { position, direction };
    }

    // Projects a world coordinate into screen space
    Vector3 Project(Vector3 p, const Matrix& world = Matrix::Identity) const {
        return _viewport.Project(p, projection, view, world);
    }

    void MoveTo(const Vector3& target_) {
        auto translation = target_ - target;
        this->position += translation;
        this->target += translation;
    }

    void LerpTo(const Vector3& target_, float duration) {
        _lerpDuration = duration;
        _lerpTime = 0;
        _lerpEnd = target_;
        _lerpStart = target;
    }

    void CancelLerp() {
        _lerpTime = _lerpDuration = 0;
    }

    void UpdatePerspectiveMatrices() {
        view = DirectX::XMMatrixLookAtLH(position, target, up);
        projection = DirectX::XMMatrixPerspectiveFovLH(fovDeg * DegToRad, _viewport.AspectRatio(), _viewport.minDepth, _viewport.maxDepth);
        viewProjection = view * projection;
        inverseProjection = projection.Invert();
        frustum = GetFrustum(position, view, projection);
    }
};

}
