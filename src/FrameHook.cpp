#include "FrameHook.h"

namespace FourStrands::FrameHook
{
	namespace
	{
		// Actor::Update, vtable slot 0xCF (RE/A/Actor.h).
		constexpr std::size_t kUpdateSlot = 0xCF;

		using Update_t = void (*)(RE::PlayerCharacter*, float);

		Update_t originalUpdate{ nullptr };
		Callback callback{ nullptr };

		void Update(RE::PlayerCharacter* a_player, float a_delta)
		{
			originalUpdate(a_player, a_delta);
			if (callback) {
				callback(a_player, a_delta);
			}
		}
	}

	void Install(Callback a_callback)
	{
		callback = a_callback;
		REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::PlayerCharacter[0] };
		originalUpdate = reinterpret_cast<Update_t>(vtable.write_vfunc(kUpdateSlot, &Update));
		REX::INFO("frame hook installed on PlayerCharacter::Update");
	}
}
