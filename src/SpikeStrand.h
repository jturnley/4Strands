#pragma once

namespace FourStrands
{
	// Phase 0 check: one strand, a thin tube built as a BSDynamicTriShape, hung between two of the player's bones and
	// moved every frame. It borrows its shader from a vanilla mesh (Spike.StrandTemplate), and finds where the
	// engine keeps the dynamic vertex data by looking for the positions it was created with. Main thread only.
	class SpikeStrand
	{
	public:
		void Update(RE::PlayerCharacter* a_player, float a_delta);

		// Detaches the strand. Call before the player's 3D goes away (kPreLoadGame).
		void Reset();

	private:
		static constexpr std::uint32_t kSegments = 24;
		static constexpr std::uint32_t kSides = 4;
		static constexpr std::uint32_t kVertices = (kSegments + 1) * kSides;
		static constexpr std::uint32_t kTriangles = kSegments * kSides * 2;

		bool Create();
		bool BorrowShader();
		bool FindDynamicData(const RE::NiPoint3* a_positions);
		void Build(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float a_sag, std::array<RE::NiPoint3, kVertices>& a_out) const;

		RE::NiPointer<RE::BSTriShape> shape;
		RE::NiAVObject*               attachedTo{ nullptr };
		std::byte*                    dynamicData{ nullptr };
		std::uint32_t                 dynamicStride{ 0 };
		bool                          failed{ false };
		bool                          loggedFirstFrame{ false };
		float                         time{ 0.0f };
	};
}
