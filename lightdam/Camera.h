#pragma once

#include <DirectXMath.h>

namespace pbrt
{
    class Camera;
}

class Camera
{
public:
    Camera();
    ~Camera();

    void ResetCamera(const pbrt::Camera& sceneCamera);

    void SetAspectRatio(float aspect)               { m_aspectRatio = aspect; }
    void SetVFovDegree(float hfovDegree)            { m_hFovDegree = hfovDegree; }
    void SetPosition(DirectX::FXMVECTOR position)   { m_position = position; }
    void SetLookAt(DirectX::FXMVECTOR lookAt)       { m_lookAt = lookAt; }
    void SetUp(DirectX::FXMVECTOR up)               { m_up = up; }

    DirectX::FXMVECTOR GetPosition() const   { return m_position; }
    DirectX::FXMVECTOR GetLookAt() const     { return m_lookAt; }
    DirectX::FXMVECTOR GetUp() const         { return m_up; }
    float GetHFovDegree() const              { return m_hFovDegree; }
    float GetAspectRatio() const            { return m_aspectRatio; }

    void ComputeCameraParams(DirectX::XMVECTOR& cameraU, DirectX::XMVECTOR& cameraV, DirectX::XMVECTOR& cameraW) const;

private:
    DirectX::XMVECTOR m_position;
    DirectX::XMVECTOR m_lookAt;
    DirectX::XMVECTOR m_up;
    float m_hFovDegree = 70.0f;
    float m_aspectRatio = 1.0f;
};
