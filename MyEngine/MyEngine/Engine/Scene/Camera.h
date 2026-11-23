#pragma once
#include <directxtk12/SimpleMath.h>

// 한 프레임의 카메라 입력 (앱이 키보드/마우스에서 채워 넘김 → Camera는 입력 장치를 모름)
struct CameraInput
{
	float forward = 0, right = 0, up = 0;  // -1 ~ 1
	float mouseDx = 0, mouseDy = 0;        // 픽셀
	bool look = false;                     // 우클릭 중일 때만 회전
	bool fast = false;                     // Shift
};

class Camera
{
public:
	using Vector3 = DirectX::SimpleMath::Vector3;
	using Matrix = DirectX::SimpleMath::Matrix;

	void SetPosition(const Vector3& p) { m_pos = p; }
	void SetLens(float fovYDeg, float nearZ, float farZ) { m_fovY = fovYDeg; m_near = nearZ; m_far = farZ; }
	void SetAspect(float aspect) { m_aspect = aspect; }

	void Update(float dt, const CameraInput& in);

	Vector3 Position() const { return m_pos; }
	Vector3 Forward() const;
	Matrix View() const;
	Matrix Proj(bool usePerspectiveProjection) const;

	float moveSpeed = 2.0f;      // m/s
	float lookSpeed = 0.003f;    // rad/px

private:
	Vector3 m_pos{ 0.0f, 0.0f, -2.0f };
	float m_yaw = 0.0f, m_pitch = 0.0f;   // 라디안. yaw 0 = +Z를 봄
	float m_fovY = 70.0f, m_aspect = 1.0f, m_near = 0.01f, m_far = 100.0f;
};