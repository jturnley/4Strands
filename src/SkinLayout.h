#pragma once

// Fallout 4 types that CommonLibF4 doesn't define yet. The layouts follow F4SE's NiObjects.h and are
// unconfirmed for 1.11: the Phase 0 body probe checks them in game. Read them only through SafeRead.

namespace FourStrands::Skin
{
	template <class T>
	struct RawArray  // BSTArray
	{
		T*            data;      // 00
		std::uint32_t capacity;  // 08
		std::uint32_t pad0C;     // 0C
		std::uint32_t size;      // 10
		std::uint32_t pad14;     // 14
	};
	static_assert(sizeof(RawArray<void*>) == 0x18);

	struct BoneTransform  // BSSkin::BoneData entry
	{
		RE::NiBound     bound;        // 00
		RE::NiTransform skinToBone;   // 10
	};
	static_assert(sizeof(BoneTransform) == 0x50);

	struct BoneData  // BSSkin::BoneData : NiObject
	{
		std::uint8_t            niObject[0x10];  // 00
		RawArray<BoneTransform> transforms;      // 10
	};
	static_assert(offsetof(BoneData, transforms) == 0x10);

	struct Instance  // BSSkin::Instance : NiObject
	{
		std::uint8_t                 niObject[0x10];   // 00
		RawArray<RE::NiAVObject*>    bones;            // 10
		RawArray<RE::NiTransform*>   worldTransforms;  // 28
		BoneData*                    boneData;         // 40
		RE::NiAVObject*              rootNode;         // 48
	};
	static_assert(offsetof(Instance, boneData) == 0x40);

	// Copies a_size bytes, or returns false if the source can't be read.
	bool SafeCopy(void* a_destination, const void* a_source, std::size_t a_size) noexcept;

	template <class T>
	bool SafeRead(const void* a_source, T& a_value) noexcept
	{
		return a_source && SafeCopy(std::addressof(a_value), a_source, sizeof(T));
	}

	inline float HalfToFloat(std::uint16_t a_half)
	{
		const std::uint32_t sign = (a_half & 0x8000u) << 16;
		const std::uint32_t exponent = (a_half >> 10) & 0x1F;
		const std::uint32_t mantissa = a_half & 0x3FF;
		std::uint32_t       bits;
		if (exponent == 0) {
			if (mantissa == 0) {
				bits = sign;
			} else {
				float value = std::ldexp(static_cast<float>(mantissa), -24);
				return sign ? -value : value;
			}
		} else if (exponent == 31) {
			bits = sign | 0x7F800000u | (mantissa << 13);
		} else {
			bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
		}
		return std::bit_cast<float>(bits);
	}
}
