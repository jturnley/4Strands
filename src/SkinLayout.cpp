#include "SkinLayout.h"

namespace FourStrands::Skin
{
	bool SafeCopy(void* a_destination, const void* a_source, std::size_t a_size) noexcept
	{
		__try {
			std::memcpy(a_destination, a_source, a_size);
			return true;
		} __except (1 /* EXCEPTION_EXECUTE_HANDLER */) {
			return false;
		}
	}
}
