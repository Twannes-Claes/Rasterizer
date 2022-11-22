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
#include <thread>
#include <ppl.h>
#include <future>
#include <algorithm>

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow),
	m_pTexture { Texture::LoadFromFile("Resources/uv_grid_2.png")}
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = static_cast<uint32_t*>(m_pBackBuffer->pixels);

	m_pDepthBufferPixels = new float[m_Width * m_Height];

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });

	m_AspectRatio = static_cast<float>(m_Width) / m_Height;

	m_NrOfPixels = m_Width * m_Height;

}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;

	delete m_pTexture;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render()
{

	ClearDepthBuffer();
	ClearBackGround();

	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	for (const Mesh& mesh : m_Meshesworld)
	{
		VertexTransformationFunction(mesh);

		switch (mesh.primitiveTopology)
		{
			case PrimitiveTopology::TriangleList:
			{

				for (size_t vertexIndex{}; vertexIndex < mesh.indices.size(); vertexIndex += 3)
				{
					DrawTriangle(vertexIndex, mesh, false);
				}

			}
			break;
				
			case PrimitiveTopology::TriangleStrip:
			{
				for (size_t vertexIndex{}; vertexIndex < mesh.indices.size() - 2; ++vertexIndex)
				{
					DrawTriangle(vertexIndex, mesh, vertexIndex % 2);
				}
			}
			break;

		}

	}

	//@END 
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);

}

void Renderer::VertexTransformationFunction(const Mesh& mesh)
{

	m_Vertices_ScreenSpace.clear();

	m_Vertices_NDC.clear();

	for (Vertex vertex : mesh.vertices)
	{
		vertex.position = m_Camera.invViewMatrix.TransformPoint(vertex.position);

		vertex.position.x = vertex.position.x / (m_AspectRatio * m_Camera.fov) / vertex.position.z;
		vertex.position.y = vertex.position.y / m_Camera.fov / vertex.position.z;

		m_Vertices_NDC.emplace_back(vertex);
	}

	for (const Vertex& vertice : m_Vertices_NDC)
	{
		Vector2 v{
		((vertice.position.x + 1) / 2) * m_Width,
		((1 - vertice.position.y) / 2) * m_Height};

		m_Vertices_ScreenSpace.push_back(v);
	}

}

void Renderer::DrawTriangle(const size_t Index, const Mesh& mesh, const bool swapVertices)
{
	const size_t index0{ mesh.indices[Index]};
	const size_t index1{ mesh.indices[Index + 1 + swapVertices] };
	const size_t index2{ mesh.indices[Index + 1 + !swapVertices]  };

	if (index0 == index1 || index1 == index2 || index0 == index2) return;

	const Vector2 v0{ m_Vertices_ScreenSpace[index0] };
	const Vector2 v1{ m_Vertices_ScreenSpace[index1] };
	const Vector2 v2{ m_Vertices_ScreenSpace[index2] };

	const Vector2 edgeV0V1{ v1 - v0 };
	const Vector2 edgeV1V2{ v2 - v1 };
	const Vector2 edgeV2V0{ v0 - v2 };

	const float invTriangleArea{ 1 / Vector2::Cross(edgeV0V1, edgeV1V2) };

	AABB boundingBox
	{
		boundingBox.minAABB = Vector2::Min(v0, Vector2::Min(v1, v2)),
		boundingBox.maxAABB = Vector2::Max(v0, Vector2::Max(v1, v2))
	};

	boundingBox.minAABB.Clamp((float)m_Width, (float)m_Height);
	boundingBox.maxAABB.Clamp((float)m_Width, (float)m_Height);

	const int margin{ 1 };

	const int minX {std::clamp(static_cast<int>(boundingBox.minAABB.x - margin),0, m_Width)};
	const int minY {std::clamp(static_cast<int>(boundingBox.minAABB.y - margin),0, m_Height)};
 								
	const int maxX {std::clamp(static_cast<int>(boundingBox.maxAABB.x + margin),0, m_Width)};
	const int maxY {std::clamp(static_cast<int>(boundingBox.maxAABB.y + margin),0, m_Height)};

	for (int px{ minX }; px < maxX; ++px)
	{
		for (int py{ minY }; py < maxY; ++py)
		{

			const Vector2 point{ static_cast<float>(px), static_cast<float>(py) };

			const Vector2 pointToEdgeSide1{ point - v0};
			float edge0{ Vector2::Cross(edgeV0V1, pointToEdgeSide1) };
			if (edge0 < 0) continue;

			const Vector2 pointToEdgeSide2{ point - v1 };
			float edge1{ Vector2::Cross(edgeV1V2, pointToEdgeSide2) };
			if (edge1 < 0) continue;

			const Vector2 pointToEdgeSide3{ point - v2 };
			float edge2{ Vector2::Cross(edgeV2V0, pointToEdgeSide3) };
			if (edge2 < 0) continue;

			const float weightV0 = edge1 * invTriangleArea;
			const float weightV1 = edge2 * invTriangleArea;
			const float weightV2 = edge0 * invTriangleArea;

			const float depthV0{ m_Vertices_NDC[index0].position.z };
			const float depthV1{ m_Vertices_NDC[index1].position.z };
			const float depthV2{ m_Vertices_NDC[index2].position.z };

			const float interpolateDepth{ 1 / ( weightV0 / depthV0 + weightV1 / depthV1 + weightV2 / depthV2 ) };

			const int pixelIndex{ px + py * m_Width };

			if (m_pDepthBufferPixels[pixelIndex] < interpolateDepth) continue;

			m_pDepthBufferPixels[pixelIndex] = interpolateDepth;

			const Vector2 uvPixel 
			{	
				(CalcUVComponent(weightV0, depthV0, index0, mesh) 
				+ CalcUVComponent(weightV1, depthV1, index1, mesh) 
				+ CalcUVComponent(weightV2, depthV2, index2, mesh)) * interpolateDepth
			};

			ColorRGB finalColor{ m_pTexture->Sample(uvPixel) };

			//Update Color in Buffer
			finalColor.MaxToOne();

			m_pBackBufferPixels[pixelIndex] = SDL_MapRGB(m_pBackBuffer->format,
				static_cast<uint8_t>(finalColor.r * 255),
				static_cast<uint8_t>(finalColor.g * 255),
				static_cast<uint8_t>(finalColor.b * 255));
		}
	}
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}

Vector2 Renderer::CalcUVComponent(const float weight, const float depth, const size_t index, const Mesh& mesh) const
{
	return (weight * mesh.vertices[index].uv) / depth;
}
