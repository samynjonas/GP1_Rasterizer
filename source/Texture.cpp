#include "Texture.h"
#include "Vector2.h"
#include <SDL_image.h>

namespace dae
{
	Texture::Texture(SDL_Surface* pSurface) :
		m_pSurface{ pSurface },
		m_pSurfacePixels{ (uint32_t*)pSurface->pixels }
	{
	}

	Texture::~Texture()
	{
		if (m_pSurface)
		{
			SDL_FreeSurface(m_pSurface);
			m_pSurface = nullptr;
		}
	}

	Texture* Texture::LoadFromFile(const std::string& path)
	{
		return new Texture{IMG_Load(path.c_str())};
	}

	ColorRGB Texture::Sample(const Vector2& uv) const
	{
		Uint8 r;
		Uint8 g;
		Uint8 b;

		const size_t x{ static_cast<size_t>(uv.x * m_pSurface->w) };
		const size_t y{ static_cast<size_t>(uv.y * m_pSurface->h) };

		const Uint32 pixel{ m_pSurfacePixels[x + y * m_pSurface->w] };

		SDL_GetRGB(pixel, m_pSurface->format, &r, &g, &b);

		const constexpr float clamp{ 1 / 255.f };

		return { r * clamp,g * clamp,b * clamp };
	}
}