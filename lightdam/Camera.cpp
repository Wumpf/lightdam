#include "Camera.h"
#define _USE_MATH_DEFINES
#include <cmath>

using namespace DirectX;

Camera::Camera()
{
}


Camera::~Camera()
{
}

void Camera::ComputeCameraParams(XMVECTOR& cameraU, XMVECTOR& cameraV, XMVECTOR& cameraW) const
{
    cameraW = DirectX::XMVector3Normalize(m_lookAt - m_position);
    cameraU = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cameraW, m_up));
    cameraV = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cameraU, cameraW));

    float f = (float)tan(m_hFovDegree / 2.0 * (M_PI / 180.0));
    cameraU *= f * m_aspectRatio;
    cameraV *= f;
}
