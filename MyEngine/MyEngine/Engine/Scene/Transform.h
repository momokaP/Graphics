#pragma once
#include <directxtk12/SimpleMath.h>

// 위치·회전·스케일을 원본으로 저장하고, 행렬은 필요할 때 만든다
struct Transform
{
	using Vector3 = DirectX::SimpleMath::Vector3;
	using Matrix = DirectX::SimpleMath::Matrix;

	Vector3 position{ 0.0f, 0.0f, 0.0f };
	Vector3 rotation{ 0.0f, 0.0f, 0.0f };
	Vector3 scale{ 1.0f, 1.0f, 1.0f };

	Matrix World() const
	{
		// 회전 순서: Z(roll) → X(pitch) → Y(yaw). Quaternion::ToEuler()와 짝
		return Matrix::CreateScale(scale) *
			Matrix::CreateFromYawPitchRoll(rotation.y, rotation.x, rotation.z) *
			Matrix::CreateTranslation(position);
	}
};