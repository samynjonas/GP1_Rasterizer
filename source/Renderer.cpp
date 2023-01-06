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
	m_Camera.Initialize(60.f, { .0f,.5f,-30.f }, m_AspectRatio);

	m_pTexture = Texture::LoadFromFile("Resources/tuktuk.png");
	m_pMesh = new Mesh();

	Utils::ParseOBJ("Resources/tuktuk.obj", m_pMesh->vertices, m_pMesh->indices);

}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;

	delete m_pTexture;
	m_pTexture = nullptr;

	delete m_pMesh;
	m_pMesh = nullptr;
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

	VertexTransformationFunction(*m_pMesh);
	std::vector<Mesh> meshes_world{ *m_pMesh };


	//Go over all meshes
	for (auto& mesh : meshes_world)
	{
		VertexTransformationFunction(mesh);

		std::vector<Vector2> screenSpaceVertices;
		for (const Vertex_Out& ndcVertex : mesh.vertices_out)
		{
			// Formula from slides
			// NDC --> Screenspace
			screenSpaceVertices.push_back({ (ndcVertex.position.x + 1) / 2.0f * m_Width, (1.0f - ndcVertex.position.y) / 2.0f * m_Height });
		}

		ResetDepthBuffer();
		ClearBackground();

		//RENDER LOGIC

		switch (mesh.primitiveTopology)
		{
		case PrimitiveTopology::TriangleList:
			for (int index{ 0 }; index < mesh.indices.size(); index += 3)
			{
				RenderMeshTriangle(mesh, screenSpaceVertices, index, false);
			}
			break;
		case PrimitiveTopology::TriangleStrip:
			for (int index{ 0 }; index < mesh.indices.size() - 2; ++index)
			{
				RenderMeshTriangle(mesh, screenSpaceVertices, index, index % 2);
			}
			break;
		default:
			std::cout << "no primitive\n";
			break;
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

void Renderer::VertexTransformationFunction(const std::vector<Mesh>& meshes_in, std::vector<Mesh>& meshes_out) const
{
	meshes_out.reserve(meshes_in.size());
	for (const Mesh& mesh : meshes_in)
	{
		std::vector<Vertex> vertices_out;
		VertexTransformationFunction(mesh.vertices, vertices_out);
		meshes_out.emplace_back
		(
			Mesh
			{
					vertices_out,
					mesh.indices,
					mesh.primitiveTopology
			}
		);
	}
}

void Renderer::VertexTransformationFunction(Mesh& mesh)
{
	Matrix worldViewProjectionMatrix{ mesh.worldMatrix * m_Camera.viewMatrix * m_Camera.projectionMatrix };
	mesh.vertices_out.clear();
	mesh.vertices_out.reserve(mesh.vertices.size());

	for (const Vertex& v : mesh.vertices)
	{
		Vertex_Out vertex_out{ Vector4{}, v.color, v.uv, v.normal, v.tangent };

		vertex_out.position = worldViewProjectionMatrix.TransformPoint({ v.position, 1.0f });
		vertex_out.viewDirection = Vector3{ vertex_out.position.x, vertex_out.position.y, vertex_out.position.z }.Normalized();

		vertex_out.normal = mesh.worldMatrix.TransformVector(v.normal);
		vertex_out.tangent = mesh.worldMatrix.TransformVector(v.tangent);


		const float invVw{ 1 / vertex_out.position.w };
		vertex_out.position.x *= invVw;
		vertex_out.position.y *= invVw;
		vertex_out.position.z *= invVw;

		// emplace back because we made vOut just to store in this vector
		mesh.vertices_out.emplace_back(vertex_out);
	}
}

void dae::Renderer::RenderMeshTriangle(const Mesh& mesh, const std::vector<Vector2>& screenSpace, int vertexIndex, bool swapVertices)
{
	const size_t vertexIndex0{ mesh.indices[vertexIndex + (2 * swapVertices)] };
	const size_t vertexIndex1{ mesh.indices[vertexIndex + 1] };
	const size_t vertexIndex2{ mesh.indices[vertexIndex + (!swapVertices * 2)] };

	if (vertexIndex0 == vertexIndex1 || vertexIndex1 == vertexIndex2 || vertexIndex2 == vertexIndex0)
	{
		return;
	}

	const Vector2 vertex0{ screenSpace[vertexIndex0] };
	const Vector2 vertex1{ screenSpace[vertexIndex1] };
	const Vector2 vertex2{ screenSpace[vertexIndex2] };

	Vector2 boundTopLeft{	Vector2::Min(vertex0,Vector2::Min(vertex1,vertex2)) };
	Vector2 boundBotRight{	Vector2::Max(vertex0,Vector2::Max(vertex1,vertex2)) };

	const float margin{ 1.f };
	{
		const Vector2 marginVect{ margin,margin };
		boundTopLeft	-= marginVect;
		boundBotRight	+= marginVect;
	}

	boundTopLeft.x	= Clamp(boundTopLeft.x,  0.f, static_cast<float>(m_Width));
	boundTopLeft.y	= Clamp(boundTopLeft.y,  0.f, static_cast<float>(m_Height));
	boundBotRight.x = Clamp(boundBotRight.x, 0.f, static_cast<float>(m_Width));
	boundBotRight.y = Clamp(boundBotRight.y, 0.f, static_cast<float>(m_Height));

	const int startX{	static_cast<int>(boundTopLeft.x)  };
	const int endX{		static_cast<int>(boundBotRight.x)  };
	const int startY{	static_cast<int>(boundTopLeft.y) };
	const int endY{		static_cast<int>(boundBotRight.y) };

	// For each pixel
	for (int px{ startX }; px < endX; ++px)
	{
		for (int py{ startY }; py < endY; ++py)
		{
			const Vector2 currentPixel{ static_cast<float>(px), static_cast<float>(py) };
			const int pixelIdx{ px + py * m_Width };

			const bool hitTriangle{ Utils::IsInTriangel(currentPixel,vertex0,vertex1,vertex2) };
			if (hitTriangle)
			{
				ColorRGB finalColor{};
				float weight0, weight1, weight2;
				weight0 = Vector2::Cross((currentPixel - vertex1), (vertex1 - vertex2));
				weight1 = Vector2::Cross((currentPixel - vertex2), (vertex2 - vertex0));
				weight2 = Vector2::Cross((currentPixel - vertex0), (vertex0 - vertex1));

				const float totalTriangleArea{ Vector2::Cross(vertex1 - vertex0,vertex2 - vertex0) };
				const float invTotalTriangleArea{ 1 / totalTriangleArea };
				weight0 *= invTotalTriangleArea;
				weight1 *= invTotalTriangleArea;
				weight2 *= invTotalTriangleArea;

				const float depth0{ mesh.vertices_out[vertexIndex0].position.z };
				const float depth1{ mesh.vertices_out[vertexIndex1].position.z };
				const float depth2{ mesh.vertices_out[vertexIndex2].position.z };

				const float interpolatedDepth = 1.f / ( ((1.f / depth0) * weight0) + ((1.f / depth1) * weight1) + ((1.f / depth2) * weight2));

				if (m_pDepthBufferPixels[pixelIdx] < interpolatedDepth || interpolatedDepth < 0.f || interpolatedDepth > 1.f)
				{
					continue;
				}

				m_pDepthBufferPixels[pixelIdx] = interpolatedDepth;
				
				//finalColor = { weight0 * mesh.vertices[vertexIndex0].color + weight1 * mesh.vertices[vertexIndex1].color + weight2 * mesh.vertices[vertexIndex2].color };
				
				//Vector2 UVinterpolated{ (((mesh.vertices[vertexIndex0].uv / depth0) * weight0) + ((mesh.vertices[vertexIndex1].uv / depth1) * weight1) + ((mesh.vertices[vertexIndex2].uv / depth2) * weight2)) * interpolatedDepth };
				//finalColor = m_pTexture->Sample(UVinterpolated);


				const float depthCol{ Remap(interpolatedDepth,0.985f,1.f) };
				finalColor = { depthCol,depthCol,depthCol };

				//Update Color in Buffer
				finalColor.MaxToOne();

				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
		}
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
