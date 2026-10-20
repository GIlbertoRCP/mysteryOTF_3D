// Seeded RNG (xmur3 hash + mulberry32), bit-for-bit the same as the web game's, so a seed
// gives the same case in both games. JavaScript's 32-bit integer ops (Math.imul, >>>, |)
// map onto uint32_t arithmetic; next() returns the same double.
#pragma once

#include "MysteryCore/Text.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace mystery
{

class SeededRandom
{
public:
	explicit SeededRandom(const std::string& seed)
	{
		const std::u16string units = ToUtf16(seed);
		uint32_t h = 1779033703u ^ static_cast<uint32_t>(units.size());
		for (const char16_t c : units)
		{
			h = (h ^ c) * 3432918353u;
			h = (h << 13) | (h >> 19);
		}
		h = (h ^ (h >> 16)) * 2246822507u;
		h = (h ^ (h >> 13)) * 3266489909u;
		state_ = h ^ (h >> 16);
	}

	double Next()
	{
		state_ += 0x6d2b79f5u;
		uint32_t a = state_;
		a = (a ^ (a >> 15)) * (a | 1u);
		a ^= a + (a ^ (a >> 7)) * (a | 61u);
		return static_cast<double>(a ^ (a >> 14)) / 4294967296.0;
	}

	/** Integer in [min, max]. */
	int Int(int min, int max)
	{
		return min + static_cast<int>(std::floor(Next() * (max - min + 1)));
	}

	/** Index of a uniformly chosen element (draws even for an empty list, as JavaScript does). */
	size_t PickIndex(size_t size)
	{
		return static_cast<size_t>(std::floor(Next() * static_cast<double>(size)));
	}

	template <typename T>
	const T& Pick(const std::vector<T>& items)
	{
		return items[PickIndex(items.size())];
	}

	/** Fisher-Yates on a copy. */
	template <typename T>
	std::vector<T> Shuffle(const std::vector<T>& items)
	{
		std::vector<T> result = items;
		for (size_t i = result.size(); i-- > 1;)
		{
			const size_t j = static_cast<size_t>(std::floor(Next() * static_cast<double>(i + 1)));
			std::swap(result[i], result[j]);
		}
		return result;
	}

	/** "prefix-0a1b2c": random, so ids never leak the answer. */
	std::string Id(const std::string& prefix)
	{
		const unsigned value = static_cast<unsigned>(std::floor(Next() * 0xffffff));
		char hex[16];
		std::snprintf(hex, sizeof(hex), "%06x", value);
		return prefix + "-" + hex;
	}

private:
	uint32_t state_ = 0;
};

} // namespace mystery
