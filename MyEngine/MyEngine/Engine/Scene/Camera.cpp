#include "stdafx.h"
#include "Scene/Camera.h"
#include <algorithm>

using namespace DirectX;
using namespace DirectX::SimpleMath;

Vector3 Camera::Forward() const
{
	// 왼손 좌표계(+Z 앞, +Y 위): yaw는 Y축 회전, pitch는 위아래
	return Vector3(
		std::sin(m_yaw) * std::cos(m_pitch),
		std::sin(m_pitch),
		std::cos(m_yaw) * std::cos(m_pitch));
}

void Camera::Update(float dt, const CameraInput& in)
{
	if (in.look)
	{
		m_yaw += in.mouseDx * lookSpeed;
		m_pitch -= in.mouseDy * lookSpeed;
		const float limit = XMConvertToRadians(89.0f);
		m_pitch = std::clamp(m_pitch, -limit, limit);
	}

	const Vector3 fwd = Forward();
	Vector3 right = Vector3::UnitY.Cross(fwd);
	right.Normalize();

	const float speed = moveSpeed * (in.fast ? 4.0f : 1.0f) * dt;
	Vector3 move = fwd * in.forward + right * in.right + Vector3::UnitY * in.up;
	if (move.LengthSquared() > 0.f) move.Normalize();
	m_pos += move * speed;
}

Matrix Camera::View() const
{
	return XMMatrixLookToLH(m_pos, Forward(), Vector3::UnitY);
}

Matrix Camera::Proj(bool usePerspectiveProjection) const
{
	if (usePerspectiveProjection)
		return XMMatrixPerspectiveFovLH(XMConvertToRadians(m_fovY), m_aspect, m_near, m_far);
	else
		return Matrix(XMMatrixOrthographicOffCenterLH(-m_aspect, m_aspect, -1.0f, 1.0f, m_near, m_far));
}