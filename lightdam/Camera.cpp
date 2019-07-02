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

void Camera::ComputeCameraParams(XMVECTOR& cameraU, XMVECTOR& cameraV, XMVECTOR& cameraW) const
{
    cameraW = m_direction;
    cameraU = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cameraW, m_up));
    cameraV = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cameraU, cameraW));

    float f = (float)tan(m_vFovDegree / 2.0 * (M_PI / 180.0));
    cameraU *= f * m_aspectRatio;
    cameraV *= f;
}

static inline bool IsKeyDown(int keyCode)
{
    return GetAsyncKeyState(keyCode) & 0x8000;
}

void Camera::Update(float secondsSinceLastUpdate)
{
    POINT newMousePos;
    GetCursorPos(&newMousePos);

    unsigned char keyState[256];
    GetKeyboardState(keyState);

    if (IsKeyDown(VK_RBUTTON))
    {
        DirectX::XMFLOAT3 dir; XMStoreFloat3(&dir, m_direction);

        float rotY = asinf(dir.y);
        float rotX = atan2f(dir.x, dir.z);
        rotX += -m_rotSpeed * (newMousePos.x - m_lastMousePosX);
        rotY += m_rotSpeed * (newMousePos.y - m_lastMousePosY);

        float scaledMoveSpeed = m_moveSpeed;
        if (IsKeyDown(VK_SHIFT))
            scaledMoveSpeed *= m_moveSpeedSpeedupFactor;

        float forward = (IsKeyDown(VK_UP) || IsKeyDown('W')) ? 1.0f : 0.0f;
        float back = (IsKeyDown(VK_DOWN) || IsKeyDown('S')) ? 1.0f : 0.0f;
        float left = (IsKeyDown(VK_LEFT) || IsKeyDown('A')) ? 1.0f : 0.0f;
        float right = (IsKeyDown(VK_RIGHT) || IsKeyDown('D')) ? 1.0f : 0.0f;

        m_direction = XMLoadFloat3(&DirectX::XMFLOAT3(static_cast<float>(sin(rotX) * cos(rotY)), static_cast<float>(sin(rotY)), static_cast<float>(cos(rotX) * cos(rotY))));
        auto cameraLeft = XMVector3Cross(m_direction, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        m_position = m_position + ((forward - back) * m_direction + (right - left) * cameraLeft) * scaledMoveSpeed * secondsSinceLastUpdate;
    }

    m_lastMousePosX = newMousePos.x;
    m_lastMousePosY = newMousePos.y;
}