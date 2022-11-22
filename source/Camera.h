#pragma once
#include <cassert>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>

#include "Math.h"
#include "Timer.h"
#include <algorithm>

namespace dae
{
	struct Camera
	{
		Camera() = default;

		Camera(const Vector3& _origin, float _fovAngle):
			origin{_origin},
			fovAngle{_fovAngle}
		{
		}


		Vector3 origin{};
		float fovAngle{90.f};
		float fov{ tanf((fovAngle * TO_RADIANS) / 2.f) };

		Vector3 forward{Vector3::UnitZ};
		Vector3 up{Vector3::UnitY};
		Vector3 right{Vector3::UnitX};

		float totalPitch{};
		float totalYaw{};

		float aspectRatio{};

		const float nearPlane{ 0.1f };
		const float farPlane{ 100.f };

		const float minPitch{ -89.99f * TO_RADIANS };
		const float maxPitch{ 89.99f * TO_RADIANS };

		int speed{ 10 };
		float speedRot{ 10 * TO_RADIANS };

		Matrix invViewMatrix{};
		Matrix viewMatrix{};
		Matrix projectionMatrix{};

		const int sprintSpeedMultiplier{ 3 };

		void Initialize(float aspecRatio, float _fovAngle = 90.f, Vector3 _origin = {0.f,0.f,0.f})
		{
			fovAngle = _fovAngle;
			fov = tanf((fovAngle * TO_RADIANS) / 2.f);

			aspectRatio = aspecRatio;

			origin = _origin;
		}

		void CalculateViewMatrix()
		{
			
			right = Vector3::Cross(Vector3::UnitY, forward).Normalized();

			up = Vector3::Cross(forward, right);

			invViewMatrix = Matrix
			{
				right,
				up,
				forward,
				origin
			};

			viewMatrix = invViewMatrix.Inverse();

			//ViewMatrix => Matrix::CreateLookAtLH(...) [not implemented yet]
			//DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixlookatlh
		}

		void CalculateProjectionMatrix()
		{

			projectionMatrix = Matrix::CreatePerspectiveFovLH(fov, aspectRatio, nearPlane, farPlane);

			//DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovlh
		}

		void Update(Timer* pTimer)
		{
			const float deltaTime = pTimer->GetElapsed();

			const uint8_t* pKeyboardState = SDL_GetKeyboardState(nullptr);

			int mouseX{}, mouseY{};
			const uint32_t mouseState = SDL_GetRelativeMouseState(&mouseX, &mouseY);

			float moveSpeed{ speed * deltaTime };
			float rotSpeed{ speedRot * deltaTime };

			moveSpeed = (pKeyboardState[SDL_SCANCODE_LSHIFT] * (sprintSpeedMultiplier) * moveSpeed) + moveSpeed;

			origin += (pKeyboardState[SDL_SCANCODE_W] || pKeyboardState[SDL_SCANCODE_UP]) * forward * moveSpeed;
			origin -= (pKeyboardState[SDL_SCANCODE_S] || pKeyboardState[SDL_SCANCODE_DOWN]) * forward * moveSpeed;

			origin -= (pKeyboardState[SDL_SCANCODE_A] || pKeyboardState[SDL_SCANCODE_LEFT]) * right * moveSpeed;
			origin += (pKeyboardState[SDL_SCANCODE_D] || pKeyboardState[SDL_SCANCODE_RIGHT]) * right * moveSpeed;

			origin -= pKeyboardState[SDL_SCANCODE_Q] * up * moveSpeed;
			origin += pKeyboardState[SDL_SCANCODE_E] * up * moveSpeed;

			bool lmb = mouseState == SDL_BUTTON_LMASK;
			bool rmb = mouseState == SDL_BUTTON_RMASK;

			bool lrmb = mouseState == (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK);

			origin -= lmb * forward * moveSpeed * float(mouseY);
			origin -= lrmb * up * (moveSpeed / 3) * float(mouseY);

			totalPitch -= rmb * rotSpeed * mouseY;
			totalPitch = std::clamp(totalPitch, minPitch, maxPitch);

			totalYaw += lmb * rotSpeed * mouseX;
			totalYaw += rmb * rotSpeed * mouseX;

			forward = (Matrix::CreateRotationX(totalPitch) * Matrix::CreateRotationY(totalYaw)).TransformVector(Vector3::UnitZ);

			//Update Matrices
			CalculateViewMatrix();
			CalculateProjectionMatrix(); //Try to optimize this - should only be called once or when fov/aspectRatio changes
		}
	};
}