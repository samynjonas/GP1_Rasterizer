//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"
#include "Math.h"
#include "Matrix.h"
#include "Texture.h"
#include "Utils.h"

#include <iostream>

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow)
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	m_AspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
	m_pDepthBufferPixels = new float[m_Width * m_Height];
	ResetDepthBuffer();

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	//Define triangle - vertices in world space
	std::vector<Vertex> vertices_world
	{
		// Triangle 0
		{ { 0.0f, 2.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 1.5f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { -1.5f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },

		// Triangle 1
		{ { 0.0f, 4.0f, 2.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 3.0f, -2.0f, 2.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { -3.0f, -2.0f, 2.0f }, { 0.0f, 0.0f, 1.0f } }

	};

	std::vector<Vertex> vertices_ndc{};
	VertexTransformationFunction(vertices_world, vertices_ndc);

	//converting coordinates to screen space
	std::vector<Vector2> ScreenSpaceTriangle;
	for (const Vertex& vertex : vertices_ndc)
	{
		Vector2 ScreenSpaceVertex{};

		ScreenSpaceVertex.x = (vertex.position.x + 1) / 2 * m_Width;
		ScreenSpaceVertex.y = (1 - vertex.position.y) / 2 * m_Height;

		ScreenSpaceTriangle.push_back(ScreenSpaceVertex);
	}



	ResetDepthBuffer();
	ClearBackground();
	//RENDER LOGIC


	//Go over each triangle
	for (int index = 0; index < vertices_world.size(); index += 3)
	{
		Vector2 topLeft{	FLT_MAX, FLT_MAX };
		Vector2 botRight{	FLT_MIN, FLT_MIN };
		for (int i{ index }; i < index + 3; ++i)
		{
			topLeft.x	= std::min(topLeft.x,	ScreenSpaceTriangle[i].x);
			topLeft.y	= std::min(topLeft.y,	ScreenSpaceTriangle[i].y);
			botRight.x	= std::max(botRight.x,	ScreenSpaceTriangle[i].x);
			botRight.y	= std::max(botRight.y,	ScreenSpaceTriangle[i].y);
		}
		topLeft.x	= Clamp(topLeft.x,	0.f, static_cast<float>(m_Width));
		topLeft.y	= Clamp(topLeft.y,	0.f, static_cast<float>(m_Height));
		botRight.x	= Clamp(botRight.x, 0.f, static_cast<float>(m_Width));
		botRight.y	= Clamp(botRight.y, 0.f, static_cast<float>(m_Height));

		const int startX{	static_cast<int>(topLeft.x)  };
		const int endX{		static_cast<int>(botRight.x) };
		const int startY{	static_cast<int>(topLeft.y)	 };
		const int endY{		static_cast<int>(botRight.y) };


		//Go over each pixel
		for (int px{startX}; px < endX; ++px)
		{
			for (int py{startY}; py < endY; ++py)
			{
				Vector2 currentPixel{ static_cast<float>(px), static_cast<float>(py) };
				const int pixelIndex{ px + py * m_Width };

				bool hitTriangle{ Utils::IsInTriangel(currentPixel, ScreenSpaceTriangle[index + 0], ScreenSpaceTriangle[index + 1], ScreenSpaceTriangle[index + 2]) };
				if (hitTriangle)
				{
					ColorRGB finalColor{};
					
					
					// weights
					float weight0, weight1, weight2;
					weight0 = Vector2::Cross((currentPixel - ScreenSpaceTriangle[index + 1]), (ScreenSpaceTriangle[index + 1] - ScreenSpaceTriangle[index + 2]));
					weight1 = Vector2::Cross((currentPixel - ScreenSpaceTriangle[index + 2]), (ScreenSpaceTriangle[index + 2] - ScreenSpaceTriangle[index + 0]));
					weight2 = Vector2::Cross((currentPixel - ScreenSpaceTriangle[index + 0]), (ScreenSpaceTriangle[index + 0] - ScreenSpaceTriangle[index + 1]));
					// divide by total triangle area
					const float totalTriangleArea{ Vector2::Cross(ScreenSpaceTriangle[index + 1] - ScreenSpaceTriangle[index + 0],ScreenSpaceTriangle[index + 2] - ScreenSpaceTriangle[index + 0]) };
					const float invTotalTriangleArea{ 1 / totalTriangleArea };
					weight0 *= invTotalTriangleArea;
					weight1 *= invTotalTriangleArea;
					weight2 *= invTotalTriangleArea;

					const Vector3 middleOfTriangle{ weight0 * vertices_world[index + 0].position + weight1 * vertices_world[index + 1].position + weight2 * vertices_world[index + 2].position };
					const float depthValue{ (m_Camera.origin - middleOfTriangle).SqrMagnitude() };
					if (m_pDepthBufferPixels[pixelIndex] < depthValue)
					{
						continue;
					}

					m_pDepthBufferPixels[pixelIndex] = depthValue;

					finalColor = { weight0 * vertices_world[index + 0].color + weight1 * vertices_world[index + 1].color + weight2 * vertices_world[index + 2].color };					
					finalColor.MaxToOne();
					m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
						static_cast<uint8_t>(finalColor.r * 255),
						static_cast<uint8_t>(finalColor.g * 255),
						static_cast<uint8_t>(finalColor.b * 255));
				}


			}
		}
	}
	

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const
{
	vertices_out.reserve(vertices_in.size());

	for (Vertex vertex : vertices_in)
	{
		vertex.position = m_Camera.invViewMatrix.TransformPoint(vertex.position);

		vertex.position.x = (vertex.position.x / (m_AspectRatio * m_Camera.fov)) / vertex.position.z;
		vertex.position.y = (vertex.position.y / m_Camera.fov) / vertex.position.z;

		vertices_out.emplace_back(vertex);
	}
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}

void Renderer::ClearBackground() const
{
	SDL_FillRect(m_pBackBuffer, NULL, SDL_MapRGB(m_pBackBuffer->format, 100, 100, 100));
}

void Renderer::ResetDepthBuffer()
{
	std::fill_n(m_pDepthBufferPixels, (m_Width * m_Height), FLT_MAX);
}
