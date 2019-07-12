#include "Camera.h"
#define _USE_MATH_DEFINES
#include <cmath>
#include <Windows.h>

using namespace DirectX;

Camera::Camera()
{
}


Camera::~Camera()
{
}

void Camera::operator = (const Camera& camera)
{
    memcpy(this, &camera, sizeof(Camera));
}

void Camera::ComputeCameraParams(float aspectRatio, XMVECTOR& cameraU, XMVECTOR& cameraV, XMVECTOR& cameraW) const
{
    cameraW = m_direction;
    cameraU = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cameraW, m_up));
    cameraV = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cameraW, cameraU));

    float f = (float)tan(m_vFovDegree / 2.0 * (M_PI / 180.0));
    cameraU *= f * aspectRatio;
    cameraV *= f;
}

static inline bool IsKeyDown(int keyCode)
{
    return GetAsyncKeyState(keyCode) & 0x8000;
}

void ControllableCamera::Update(float secondsSinceLastUpdate)
{
    POINT newMousePos;
    GetCursorPos(&newMousePos);

    unsigned char keyState[256];
    GetKeyboardState(keyState);

    if (IsKeyDown(VK_RBUTTON))
    {
        DirectX::XMFLOAT3 dir; XMStoreFloat3(&dir, m_direction);

        float rotY = -m_rotSpeed * (newMousePos.y - m_lastMousePosY);
        float rotX = -m_rotSpeed * (newMousePos.x - m_lastMousePosX);

        float scaledMoveSpeed = m_moveSpeed;
        if (IsKeyDown(VK_SHIFT))
            scaledMoveSpeed *= m_moveSpeedSpeedupFactor;

        float forward = (IsKeyDown(VK_UP) || IsKeyDown('W')) ? 1.0f : 0.0f;
        float back = (IsKeyDown(VK_DOWN) || IsKeyDown('S')) ? 1.0f : 0.0f;
        float left = (IsKeyDown(VK_LEFT) || IsKeyDown('A')) ? 1.0f : 0.0f;
        float right = (IsKeyDown(VK_RIGHT) || IsKeyDown('D')) ? 1.0f : 0.0f;

        auto cameraLeft = XMVector3Cross(m_direction, m_up);
        auto rotateUpDown = XMQuaternionRotationAxis(cameraLeft, rotY);
        auto rotateLeftRight = XMQuaternionRotationAxis(m_up, rotX);
        m_up = XMVector3Rotate(m_up, rotateUpDown);
        m_direction = XMVector3Rotate(m_direction, rotateUpDown);
        m_direction = XMVector3Rotate(m_direction, rotateLeftRight);

        m_position = m_position + ((forward - back) * m_direction + (right - left) * cameraLeft) * scaledMoveSpeed * secondsSinceLastUpdate;
    }

    m_lastMousePosX = newMousePos.x;
    m_lastMousePosY = newMousePos.y;
}