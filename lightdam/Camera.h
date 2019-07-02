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

    void Update(float secondsSinceLastUpdate);

    void SetAspectRatio(float aspect)               { m_aspectRatio = aspect; }
    void SetVFovDegree(float vfovDegree)            { m_vFovDegree = vfovDegree; }
    void SetPosition(DirectX::FXMVECTOR position)   { m_position = position; }
    void SetDirection(DirectX::FXMVECTOR direction) { m_direction = direction; } // Assumes normalized
    void SetUp(DirectX::FXMVECTOR up)               { m_up = up; }

    DirectX::FXMVECTOR GetPosition() const   { return m_position; }
    DirectX::FXMVECTOR GetDirection() const  { return m_direction; }
    DirectX::FXMVECTOR GetUp() const         { return m_up; }
    float GetHFovDegree() const              { return m_vFovDegree; }
    float GetAspectRatio() const             { return m_aspectRatio; }

    void ComputeCameraParams(DirectX::XMVECTOR& cameraU, DirectX::XMVECTOR& cameraV, DirectX::XMVECTOR& cameraW) const;

private:
    
    // Camera parameters.
    DirectX::XMVECTOR m_position;
    DirectX::XMVECTOR m_direction;
    DirectX::XMVECTOR m_up;
    float m_vFovDegree = 70.0f;
    float m_aspectRatio = 1.0f;

    // Camera control.
    long m_lastMousePosX = 0;
    long m_lastMousePosY = 0;
    float m_rotSpeed = 0.01f;
    float m_moveSpeed = 4.0f;
    const float m_moveSpeedSpeedupFactor = 10.0f;
};
