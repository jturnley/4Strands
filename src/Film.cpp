#include "Film.h"

#include <cmath>
#ifndef NOMINMAX
#	define NOMINMAX
#endif
#include <windows.h>

namespace FourStrands::Film
{
	namespace
	{
		using REX::W32::ID3D11ShaderResourceView;
		using REX::W32::ID3D11Texture2D;

		constexpr const char* kShapes[] = { "SoftBodyFilm", "SoftBodyFilmVulva" };

		std::mutex                 lock;  // the pixels, the dirty rows and what is attached
		std::vector<std::uint32_t> pixels;
		int                        dirtyFrom = kSize, dirtyTo = 0;
		ID3D11Texture2D*           texture{ nullptr };
		ID3D11ShaderResourceView*  view{ nullptr };

		struct Swapped
		{
			ID3D11ShaderResourceView** slot{ nullptr };  // the field of the placeholder's renderer texture that held its view
			ID3D11ShaderResourceView*  original{ nullptr };
		};
		std::vector<Swapped> swapped;

		using Present_t = HRESULT (*)(REX::W32::IDXGISwapChain*, std::uint32_t, std::uint32_t);
		Present_t originalPresent{ nullptr };
		bool      hooked{ false };
		bool      logged{ false };

		bool Readable(const void* a_address, std::size_t a_bytes)
		{
			MEMORY_BASIC_INFORMATION info{};
			if (!a_address || !VirtualQuery(a_address, &info, sizeof(info)) || info.State != MEM_COMMIT) {
				return false;
			}
			if (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) {
				return false;
			}
			const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
			return reinterpret_cast<std::uintptr_t>(a_address) + a_bytes <= end;
		}

		// Render thread, before the frame is shown: upload what changed
		HRESULT Present(REX::W32::IDXGISwapChain* a_chain, std::uint32_t a_sync, std::uint32_t a_flags)
		{
			{
				std::scoped_lock guard{ lock };
				if (texture && dirtyFrom < dirtyTo && pixels.size() == static_cast<std::size_t>(kSize) * kSize) {
					auto* renderer = RE::BSGraphics::GetRendererData();
					if (renderer && renderer->context) {
						REX::W32::D3D11_BOX box{ 0, static_cast<std::uint32_t>(dirtyFrom), 0, static_cast<std::uint32_t>(kSize),
							static_cast<std::uint32_t>(dirtyTo), 1 };
						renderer->context->UpdateSubresource(texture, 0, &box, pixels.data() + static_cast<std::size_t>(dirtyFrom) * kSize,
							kSize * 4, 0);
					}
					dirtyFrom = kSize;
					dirtyTo = 0;
				}
			}
			return originalPresent(a_chain, a_sync, a_flags);
		}

		bool Hook()
		{
			if (hooked) {
				return true;
			}
			auto* renderer = RE::BSGraphics::GetRendererData();
			auto* chain = renderer ? renderer->renderWindow[0].swapChain : nullptr;
			if (!chain) {
				return false;
			}
			auto** vtable = *reinterpret_cast<void***>(chain);
			constexpr std::size_t kPresent = 8;   // IDXGISwapChain::Present
			DWORD old{};
			if (!VirtualProtect(&vtable[kPresent], sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) {
				return false;
			}
			originalPresent = reinterpret_cast<Present_t>(vtable[kPresent]);
			vtable[kPresent] = reinterpret_cast<void*>(&Present);
			VirtualProtect(&vtable[kPresent], sizeof(void*), old, &old);
			hooked = true;
			REX::INFO("film: uploads hooked on the swap chain's Present");
			return true;
		}

		bool Create()
		{
			if (texture) {
				return true;
			}
			auto* renderer = RE::BSGraphics::GetRendererData();
			if (!renderer || !renderer->device) {
				return false;
			}
			REX::W32::D3D11_TEXTURE2D_DESC desc{};
			desc.width = kSize;
			desc.height = kSize;
			desc.mipLevels = 1;
			desc.arraySize = 1;
			desc.format = REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.sampleDesc.count = 1;
			desc.usage = REX::W32::D3D11_USAGE_DEFAULT;
			desc.bindFlags = REX::W32::D3D11_BIND_SHADER_RESOURCE;
			pixels.assign(static_cast<std::size_t>(kSize) * kSize, 0u);
			REX::W32::D3D11_SUBRESOURCE_DATA initial{ pixels.data(), kSize * 4, 0 };
			if (FAILED(renderer->device->CreateTexture2D(&desc, &initial, &texture)) || !texture) {
				REX::WARN("film: could not create the texture");
				return false;
			}
			if (FAILED(renderer->device->CreateShaderResourceView(texture, nullptr, &view)) || !view) {
				REX::WARN("film: could not create its view");
				texture->Release();
				texture = nullptr;
				return false;
			}
			REX::INFO("film: texture created ({} x {})", kSize, kSize);
			return true;
		}

		// The placeholder's renderer texture holds a shader resource view; FO4's layout of it isn't in CommonLibF4, so
		// the field is found by its vtable (every view the device makes shares ours)
		ID3D11ShaderResourceView** FindView(RE::BSGraphics::Texture* a_texture)
		{
			if (!a_texture || !Readable(a_texture, 0x80)) {
				return nullptr;
			}
			const void* table = *reinterpret_cast<void* const*>(view);
			auto*       fields = reinterpret_cast<void**>(a_texture);
			for (int k = 0; k < 16; ++k) {
				void* candidate = fields[k];
				if (candidate && Readable(candidate, sizeof(void*)) && *reinterpret_cast<void**>(candidate) == table) {
					if (!logged) {
						REX::INFO("film: the renderer texture keeps its view at +{:#x}", k * 8);
					}
					return reinterpret_cast<ID3D11ShaderResourceView**>(&fields[k]);
				}
			}
			return nullptr;
		}
	}

	void Attach(RE::NiAVObject* a_her)
	{
		if (!a_her || !Create() || !Hook()) {
			return;
		}
		std::scoped_lock guard{ lock };
		for (const auto* name : kShapes) {
			auto* object = a_her->GetObjectByName(RE::BSFixedString(name));
			auto* geometry = object ? netimmerse_cast<RE::BSGeometry*>(object) : nullptr;
			if (!geometry) {
				continue;
			}
			for (auto& property : geometry->properties) {
				auto* shader = property ? netimmerse_cast<RE::BSLightingShaderProperty*>(property.get()) : nullptr;
				auto* material = shader ? static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
				auto* diffuse = material ? material->diffuseTexture.get() : nullptr;
				if (!diffuse) {
					continue;
				}
				auto** slot = FindView(diffuse->rendererTexture);
				if (!slot) {
					if (!logged) {
						REX::WARN("film: no view found in {}'s placeholder texture", name);
					}
					continue;
				}
				if (*slot != view) {
					swapped.push_back({ slot, *slot });
					*slot = view;
					REX::INFO("film: drawing on {}", name);
				}
			}
		}
		logged = true;
	}

	void Detach()
	{
		std::scoped_lock guard{ lock };
		for (const auto& s : swapped) {
			if (*s.slot == view) {
				*s.slot = s.original;
			}
		}
		if (!swapped.empty()) {
			REX::INFO("film: placeholder restored");
		}
		swapped.clear();
	}

	void Submit(std::span<const std::uint32_t> a_pixels, int a_firstRow, int a_rows)
	{
		std::scoped_lock guard{ lock };
		if (pixels.size() != a_pixels.size()) {
			return;
		}
		const int from = std::clamp(a_firstRow, 0, kSize);
		const int to = std::clamp(a_firstRow + a_rows, 0, kSize);
		std::copy(a_pixels.begin() + static_cast<std::ptrdiff_t>(from) * kSize, a_pixels.begin() + static_cast<std::ptrdiff_t>(to) * kSize,
			pixels.begin() + static_cast<std::ptrdiff_t>(from) * kSize);
		dirtyFrom = std::min(dirtyFrom, from);
		dirtyTo = std::max(dirtyTo, to);
	}

	void TestPattern(float a_seconds)
	{
		static float                      time = 0.0f;
		static std::vector<std::uint32_t> image(static_cast<std::size_t>(kSize) * kSize);
		static int                        band = 0;
		time += a_seconds;
		// 64 rows a frame: a translucent grid with a glossy band sweeping down the UV layout
		const int from = band;
		const int rows = 64;
		for (int y = from; y < from + rows; ++y) {
			for (int x = 0; x < kSize; ++x) {
				const bool  line = (x % 128) < 6 || (y % 128) < 6;
				const float wave = 0.5f + 0.5f * std::sin(time * 2.0f + static_cast<float>(y) * 0.01f);
				const auto  a = static_cast<std::uint32_t>(line ? 200 : 60.0f * wave);
				const auto  r = static_cast<std::uint32_t>(line ? 255 : 120);
				const auto  g = static_cast<std::uint32_t>(line ? 80 : 200);
				const auto  b = static_cast<std::uint32_t>(255);
				image[static_cast<std::size_t>(y) * kSize + x] = r | (g << 8) | (b << 16) | (a << 24);
			}
		}
		Submit(image, from, rows);
		band = (band + rows) % kSize;
	}
}
