#pragma once
#include <cassert>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>

#include "Math.h"
#include "Timer.h"

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

		const float movementSpeed{ 5.f };
		const float shiftSpeed{ 4.0f };

		Vector3 origin{};
		float fovAngle{90.f};
		float fov{ tanf((fovAngle * TO_RADIANS) / 2.f) };

		Vector3 forward{Vector3::UnitZ};
		Vector3 up{Vector3::UnitY};
		Vector3 right{Vector3::UnitX};

		float totalPitch{};
		float totalYaw{};

		Matrix invViewMatrix{};
		Matrix viewMatrix{};

		void Initialize(float _fovAngle = 90.f, Vector3 _origin = {0.f,0.f,0.f})
		{
			fovAngle = _fovAngle;
			fov = tanf((fovAngle * TO_RADIANS) / 2.f);

			origin = _origin;
		}

		void CalculateViewMatrix()
		{
			right = Vector3::Cross(Vector3::UnitY, forward);
			up = Vector3::Cross(forward, right);

			invViewMatrix =
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
			//TODO W2

			//ProjectionMatrix => Matrix::CreatePerspectiveFovLH(...) [not implemented yet]
			//DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovlh
		}

		void Update(Timer* pTimer)
		{
			const float deltaTime = pTimer->GetElapsed();
			const float rotationSpeed{ 0.5f * TO_RADIANS };

			Vector3 directionVector{};

			//Keyboard Input
			const uint8_t* pKeyboardState = SDL_GetKeyboardState(nullptr);

			if (pKeyboardState[SDL_SCANCODE_Z] || pKeyboardState[SDL_SCANCODE_W])
			{
				directionVector += forward * movementSpeed * deltaTime;
			}
			if (pKeyboardState[SDL_SCANCODE_S])
			{
				directionVector -= forward * movementSpeed * deltaTime;
			}
			if (pKeyboardState[SDL_SCANCODE_Q] || pKeyboardState[SDL_SCANCODE_A])
			{
				directionVector -= right * movementSpeed * deltaTime;
			}
			if (pKeyboardState[SDL_SCANCODE_D])
			{
				directionVector += right * movementSpeed * deltaTime;
			}
			origin += directionVector;

			//Mouse Input
			int mouseX{}, mouseY{};
			const uint32_t mouseState = SDL_GetRelativeMouseState(&mouseX, &mouseY);

			if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT))
			{
				// RMB
				if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT))
				{
					origin += right * (-mouseY * movementSpeed * deltaTime);
				}
				else
				{
					origin += forward * (-mouseY * movementSpeed * deltaTime);
				}
			}

			if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT))
			{
				forward = Matrix::CreateRotationY(mouseX * rotationSpeed).TransformVector(forward);
				forward = Matrix::CreateRotationX(-mouseY * rotationSpeed).TransformVector(forward);
			}


			//Shift multiplier
			const float shiftSpeed{ 4.0f };
			if (pKeyboardState[SDL_SCANCODE_LSHIFT])
			{
				directionVector *= shiftSpeed;
			}





			//Camera Update Logic
			//...

			//Update Matrices
			CalculateViewMatrix();
			CalculateProjectionMatrix(); //Try to optimize this - should only be called once or when fov/aspectRatio changes
		}
	};
}
