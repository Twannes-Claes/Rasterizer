#pragma once
#include <cmath>
#include <algorithm>

namespace dae
{
	/* --- HELPER STRUCTS --- */
	struct Int2
	{
		int x{};
		int y{};
	};

	/* --- CONSTANTS --- */
	constexpr auto PI = 3.14159f;
	constexpr auto PI_DIV_2 = 1.57079f;
	constexpr auto PI_DIV_4 = 0.78539f;
	constexpr auto PI_2 = 6.28318f;
	constexpr auto PI_4 = 12.56637f;

	constexpr auto TO_DEGREES = (180.f / PI);
	constexpr auto TO_RADIANS(PI / 180.f);

	/* --- HELPER FUNCTIONS --- */
	inline float Square(float a)
	{
		return a * a;
	}

	inline float Lerpf(float a, float b, float factor)
	{
		return ((1 - factor) * a) + (factor * b);
	}

	inline bool AreEqual(float a, float b, float epsilon = FLT_EPSILON)
	{
		return abs(a - b) < epsilon;
	}

	inline int Clamp(const int v, int min, int max)
	{
		if (v < min) return min;
		if (v > max) return max;
		return v;
	}

	inline float Clamp(const float v, float min, float max)
	{
		if (v < min) return min;
		if (v > max) return max;
		return v;
	}

	inline float Saturate(const float v)
	{
		if (v < 0.f) return 0.f;
		if (v > 1.f) return 1.f;
		return v;
	}

	inline void Remap(float& depthValue, const float min, const float max)
	{
		depthValue = std::clamp(depthValue, min, max);

		depthValue = (depthValue - min) / (max - min);
	}

	inline float Remap(float depthValue, const float min, const float max)
	{
		depthValue = std::clamp(depthValue, min, max);

		depthValue = (depthValue - min) / (max - min);

		return depthValue;
	}
}