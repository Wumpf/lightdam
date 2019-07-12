#pragma once

#include <DirectXMath.h>

class Camera
{
public:
    Camera();
    ~Camera();

    void operator = (const Camera& camera);

    void SetVFovDegree(float vfovDegree)            { m_vFovDegree = vfovDegree; }
    void SetPosition(DirectX::FXMVECTOR position)   { m_position = position; }
    void SetDirection(DirectX::FXMVECTOR direction) { m_direction = direction; } // Assumes normalized
    void SetUp(DirectX::FXMVECTOR up)               { m_up = up; } // Assumes normalized

    DirectX::FXMVECTOR GetPosition() const   { return m_position; }
    DirectX::FXMVECTOR GetDirection() const  { return m_direction; }
    DirectX::FXMVECTOR GetUp() const         { return m_up; }
    float GetHFovDegree() const              { return m_vFovDegree; }

    void ComputeCameraParams(float aspectRatio, DirectX::XMVECTOR& cameraU, DirectX::XMVECTOR& cameraV, DirectX::XMVECTOR& cameraW) const;

protected:
    DirectX::XMVECTOR m_position;
    DirectX::XMVECTOR m_direction;
    DirectX::XMVECTOR m_up;
    float m_vFovDegree = 70.0f;
};

class ControllableCamera : public Camera
{
public:
    void Update(float secondsSinceLastUpdate);

    void operator = (const Camera& camera) { Camera::operator=(camera); }

    float GetMoveSpeed() const { return m_moveSpeed; }
    void SetMoveSpeed(float moveSpeed) { m_moveSpeed = moveSpeed; }

private:
    long m_lastMousePosX = 0;
    long m_lastMousePosY = 0;
    float m_rotSpeed = 0.01f;
    float m_moveSpeed = 4.0f;
    const float m_moveSpeedSpeedupFactor = 10.0f;
};
