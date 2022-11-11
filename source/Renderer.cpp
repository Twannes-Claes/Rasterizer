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

	m_pDepthBufferPixels = new float[m_Width * m_Height];

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });

	m_AspectRatio = static_cast<float>(m_Width) / m_Height;

	m_NrOfPixels = m_Width * m_Height;

	m_Vertices_World = 
	{

		{ { 0.f, 2.f, 0.f }, { 1.f, 0.f, 0.f } },
		{ { 1.5f, -1.f, 0.f }, { 1.f, 0.f, 0.f} },
		{ { -1.5f, -1.f, 0.f }, { 1.f, 0.f, 0.f} },

		{ { 0.f, 4.f, 2.f }, { 1.f, 0.f, 0.f} },
		{ { 3.f, -2.f, 2.f }, { 0.f, 1.f, 0.f} },
		{ { -3.f, -2.f, 2.f }, { 0.f, 0.f, 1.f} }

	};

	m_TriangleSize = m_Vertices_World.size();

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
	//fill depth buffer
	std::fill_n(m_pDepthBufferPixels, m_NrOfPixels, FLT_MAX);

	ClearBackGround();

	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	std::vector<Vector2> vertices_screenSpace{};

	VertexTransformationFunction(m_Vertices_World, vertices_screenSpace);

	for (size_t vertexIndex1{}; vertexIndex1 < m_TriangleSize; vertexIndex1 += 3)
	{

		const size_t vertexIndex2{ vertexIndex1 + 1 };
		const size_t vertexIndex3{ vertexIndex1 + 2 };

		const Vector2 v0{ vertices_screenSpace[vertexIndex1] };
		const Vector2 v1{ vertices_screenSpace[vertexIndex2] };
		const Vector2 v2{ vertices_screenSpace[vertexIndex3] };

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
		

		int minX = (int)boundingBox.minAABB.x;
		int minY = (int)boundingBox.minAABB.y;

		int maxX = (int)boundingBox.maxAABB.x;
		int maxY = (int)boundingBox.maxAABB.y;

		for (int px{ minX }; px < maxX; ++px)
		{
			for (int py{ minY }; py < maxY; ++py)
			{

				ColorRGB finalColor{};

				const Vector2 point{ float(px), float(py) };

				const Vector2 pointToEdgeSide1{ point - vertices_screenSpace[vertexIndex1] };
				float weightV0{ Vector2::Cross(edgeV0V1, pointToEdgeSide1) };

				if (weightV0 < 0) continue;

				const Vector2 pointToEdgeSide2{ point - vertices_screenSpace[vertexIndex2] };
				float weightV1{ Vector2::Cross(edgeV1V2, pointToEdgeSide2) };

				if (weightV1 < 0) continue;

				const Vector2 pointToEdgeSide3{ point - vertices_screenSpace[vertexIndex3] };
				float weightV2{ Vector2::Cross(edgeV2V0, pointToEdgeSide3) };

				if (weightV2 < 0) continue;

				weightV0 *= invTriangleArea;
				weightV1 *= invTriangleArea;
				weightV2 *= invTriangleArea;

				const float depthWeight
				{
					weightV0 * (m_Vertices_World[vertexIndex1].position.z - m_Camera.origin.z) +
					weightV1 * (m_Vertices_World[vertexIndex2].position.z - m_Camera.origin.z) +
					weightV2 * (m_Vertices_World[vertexIndex3].position.z - m_Camera.origin.z)
				};

				const int pixelIndex{ px + py * m_Width };

				if (m_pDepthBufferPixels[pixelIndex] < depthWeight) continue;

				m_pDepthBufferPixels[pixelIndex] = depthWeight;

				finalColor = { weightV0 * m_Vertices_World[vertexIndex1].color + weightV1 * m_Vertices_World[vertexIndex2].color + weightV2 * m_Vertices_World[vertexIndex3].color };

				//Update Color in Buffer
				finalColor.MaxToOne();

				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
		}
	}

	//@END 
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);

}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vector2>& screenspace) const
{

	std::vector<Vertex> vertices_ndc{};

	vertices_ndc.reserve(vertices_in.size());

	screenspace.reserve(vertices_in.size());

	for (Vertex vertex : vertices_in)
	{
		vertex.position = m_Camera.invViewMatrix.TransformPoint(vertex.position);

		vertex.position.x = vertex.position.x / (m_AspectRatio * m_Camera.fov) / vertex.position.z;
		vertex.position.y = vertex.position.y / m_Camera.fov / vertex.position.z;

		vertices_ndc.emplace_back(vertex);
	}

	for (const Vertex& vertice : vertices_ndc)
	{
		Vector2 v{
		((vertice.position.x + 1) / 2) * m_Width,
		((1 - vertice.position.y) / 2) * m_Height
		};

		screenspace.push_back(v);
	}

}

void dae::Renderer::RenderPixel()
{
	
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
