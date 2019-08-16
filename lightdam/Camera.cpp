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

bool Camera::operator ==(const Camera& camera)
{
    return
        XMVector3Equal(m_position, camera.m_position) &&
        XMVector3Equal(m_direction, camera.m_direction) &&
        XMVector3Equal(m_up, camera.m_up) &&
        m_fovRad == camera.m_fovRad;
}

void Camera::SnapUpToAxis()
{
    static constexpr XMFLOAT3 xpos(1, 0, 0);
    static constexpr XMFLOAT3 xneg(-1, 0, 0);
    static constexpr XMFLOAT3 ypos(0, 1, 0);
    static constexpr XMFLOAT3 yneg(0, -1, 0);
    static constexpr XMFLOAT3 zpos(0,0, 1);
    static constexpr XMFLOAT3 zneg(0,0, -1);

    XMFLOAT3 up;
    DirectX::XMStoreFloat3(&up, m_up);
    if (fabs(up.x) > fabs(up.y) && fabs(up.x) > fabs(up.z))
        m_up = XMLoadFloat3(up.x > 0 ? &xpos : &xneg);
    else if (fabs(up.y) > fabs(up.x) && fabs(up.y) > fabs(up.z))
        m_up = XMLoadFloat3(up.y > 0 ? &ypos : &yneg);
    else if (fabs(up.z) > fabs(up.y) && fabs(up.z) > fabs(up.y))
        m_up = XMLoadFloat3(up.z > 0 ? &zpos : &zneg);
}

void Camera::ComputeCameraParams(float aspectRatio, XMVECTOR& cameraU, XMVECTOR& cameraV, XMVECTOR& cameraW) const
{
    cameraW = m_direction;
    cameraU = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cameraW, m_up));
    cameraV = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cameraW, cameraU));
    

    float f = (float)tan(m_fovRad * 0.5f);
    cameraU *= f;
    cameraV *= f;

    if (aspectRatio > 1.0f)
        cameraU *= aspectRatio;
    else
        cameraV /= aspectRatio;
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
        //m_up = XMVector3Rotate(m_up, rotateUpDown);
        m_direction = XMVector3Rotate(m_direction, rotateUpDown);
        m_direction = XMVector3Rotate(m_direction, rotateLeftRight);

        m_position = m_position + ((forward - back) * m_direction + (right - left) * cameraLeft) * scaledMoveSpeed * secondsSinceLastUpdate;
    }

    m_lastMousePosX = newMousePos.x;
    m_lastMousePosY = newMousePos.y;
}