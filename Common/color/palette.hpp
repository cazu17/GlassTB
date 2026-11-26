#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "../util/color.hpp"

// Dominant color extraction for adaptive taskbar colors. Pure functions: the caller
// supplies a small, already downscaled image (e.g. 64x64 pixels), so the cost is
// bounded and nothing here touches the screen or the file system.
namespace palette {
	struct Hsl {
		float h, s, l; // h in [0, 360), s and l in [0, 1]
	};

	inline Hsl ToHsl(Util::Color c) noexcept
	{
		const float r = c.R / 255.0f, g = c.G / 255.0f, b = c.B / 255.0f;
		const float maxv = std::max({ r, g, b }), minv = std::min({ r, g, b });
		const float l = (maxv + minv) / 2.0f;
		const float d = maxv - minv;
		if (d < 1e-6f)
		{
			return { 0.0f, 0.0f, l };
		}

		const float s = l > 0.5f ? d / (2.0f - maxv - minv) : d / (maxv + minv);
		float h;
		if (maxv == r)
		{
			h = (g - b) / d + (g < b ? 6.0f : 0.0f);
		}
		else if (maxv == g)
		{
			h = (b - r) / d + 2.0f;
		}
		else
		{
			h = (r - g) / d + 4.0f;
		}
		return { h * 60.0f, s, l };
	}

	inline Util::Color FromHsl(Hsl hsl, std::uint8_t alpha = 255) noexcept
	{
		const auto hue = [](float p, float q, float t)
		{
			if (t < 0) t += 1;
			if (t > 1) t -= 1;
			if (t < 1.0f / 6) return p + (q - p) * 6 * t;
			if (t < 1.0f / 2) return q;
			if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
			return p;
		};

		float r, g, b;
		const float s = std::clamp(hsl.s, 0.0f, 1.0f), l = std::clamp(hsl.l, 0.0f, 1.0f);
		if (s < 1e-6f)
		{
			r = g = b = l;
		}
		else
		{
			const float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
			const float p = 2 * l - q;
			const float h = std::fmod(std::fmod(hsl.h, 360.0f) + 360.0f, 360.0f) / 360.0f;
			r = hue(p, q, h + 1.0f / 3);
			g = hue(p, q, h);
			b = hue(p, q, h - 1.0f / 3);
		}

		const auto to8 = [](float v) { return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
		return { to8(r), to8(g), to8(b), alpha };
	}

	inline float HueDistance(float a, float b) noexcept
	{
		const float d = std::fabs(a - b);
		return std::min(d, 360.0f - d);
	}

	struct Result {
		Util::Color dominant;
		Util::Color secondary;
	};

	// Quantizes to 4 bits per channel (4096 buckets) and weights each bucket so that
	// colorful pixels win over large areas of near-gray/near-black/near-white.
	inline std::optional<Result> Extract(std::span<const Util::Color> pixels)
	{
		struct Bucket {
			double weight = 0, r = 0, g = 0, b = 0;
		};
		std::vector<Bucket> buckets(4096);

		std::size_t opaque = 0;
		for (const auto &p : pixels)
		{
			if (p.A < 128)
			{
				continue;
			}
			++opaque;
			const auto hsl = ToHsl(p);
			// Prefer saturated, mid-lightness pixels; keep a floor so gray images still get a result.
			const double weight = 0.15 + hsl.s * (1.0 - std::fabs(hsl.l - 0.5) * 1.6);
			auto &bucket = buckets[(p.R >> 4) << 8 | (p.G >> 4) << 4 | (p.B >> 4)];
			bucket.weight += weight;
			bucket.r += p.R * weight;
			bucket.g += p.G * weight;
			bucket.b += p.B * weight;
		}

		if (opaque == 0)
		{
			return std::nullopt;
		}

		const auto average = [](const Bucket &bucket)
		{
			return Util::Color(
				static_cast<std::uint8_t>(bucket.r / bucket.weight),
				static_cast<std::uint8_t>(bucket.g / bucket.weight),
				static_cast<std::uint8_t>(bucket.b / bucket.weight));
		};

		std::vector<std::size_t> order(buckets.size());
		for (std::size_t i = 0; i < order.size(); ++i)
		{
			order[i] = i;
		}
		std::ranges::sort(order, [&buckets](std::size_t a, std::size_t b) { return buckets[a].weight > buckets[b].weight; });

		Result result { average(buckets[order[0]]), average(buckets[order[0]]) };
		const auto dominantHsl = ToHsl(result.dominant);
		for (std::size_t i = 1; i < order.size() && buckets[order[i]].weight > 0; ++i)
		{
			const auto candidate = average(buckets[order[i]]);
			const auto hsl = ToHsl(candidate);
			if (HueDistance(hsl.h, dominantHsl.h) >= 40.0f || std::fabs(hsl.l - dominantHsl.l) >= 0.3f)
			{
				result.secondary = candidate;
				break;
			}
		}

		return result;
	}

	struct Tuning {
		float saturation = 1.0f;     // multiplier, 0..2
		float contrast = 0.5f;       // 0 = keep lightness, 1 = push fully to the theme's extreme
		std::uint8_t intensity = 160; // resulting alpha
	};

	// Adapts a wallpaper color to be usable as a taskbar tint: dark themes get a darker
	// tone (so white text stays readable), light themes a lighter one.
	inline Util::Color ForTheme(Util::Color color, bool darkTheme, const Tuning &tuning) noexcept
	{
		auto hsl = ToHsl(color);
		hsl.s = std::clamp(hsl.s * tuning.saturation, 0.0f, 1.0f);
		const float target = darkTheme ? 0.18f : 0.86f;
		const float contrast = std::clamp(tuning.contrast, 0.0f, 1.0f);
		hsl.l = hsl.l + (target - hsl.l) * contrast;
		// Hard readability bounds independent of the contrast setting.
		hsl.l = darkTheme ? std::min(hsl.l, 0.45f) : std::max(hsl.l, 0.6f);
		return FromHsl(hsl, tuning.intensity);
	}
}
