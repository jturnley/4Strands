#include "SpikeStrand.h"

#include "Settings.h"
#include "SkinLayout.h"

#include <numbers>

namespace FourStrands
{
	namespace
	{
		// Where to look for BSDynamicTriShape's vertex pointer: past BSTriShape (0x170), before the guessed end.
		constexpr std::size_t kScanFrom = 0x170;
		constexpr std::size_t kScanTo = 0x1C0;

		// IRendererResourceManager declares two CreateDynamicTriShape overloads in a row, and MSVC lays out adjacent
		// overloads in reverse, so calling the long one through CommonLibF4's header reaches the short one (slot 9) and
		// crashes. Call the long one by its slot instead.
		constexpr std::size_t kCreateDynamicTriShapeSlot = 8;

		RE::BSTriShape* CreateDynamicTriShape(RE::BSShaderResourceManager* a_manager, std::uint32_t a_dynamicFlags, std::uint32_t a_numTriangles,
			std::uint16_t* a_indices, std::uint32_t a_numVertices, RE::NiPoint3* a_positions, RE::NiPoint2* a_texCoords0, RE::NiColorA* a_texCoords1,
			RE::NiPoint3* a_normals, RE::NiPoint3* a_binormals, RE::NiPoint3* a_tangents, RE::NiColorA* a_colors, RE::NiColorA* a_skinBoneWeights,
			std::uint8_t* a_skinBoneIndices, RE::NiColorA* a_landscapeData1, RE::NiColorA* a_landscapeData2, float* a_eyeData)
		{
			using func_t = RE::BSTriShape* (*)(RE::IRendererResourceManager*, std::uint32_t, std::uint32_t, std::uint16_t*, std::uint32_t, RE::NiPoint3*,
				RE::NiPoint2*, RE::NiColorA*, RE::NiPoint3*, RE::NiPoint3*, RE::NiPoint3*, RE::NiColorA*, RE::NiColorA*, std::uint8_t*, RE::NiColorA*,
				RE::NiColorA*, float*);
			RE::IRendererResourceManager* manager = a_manager;
			const auto* vtable = *reinterpret_cast<func_t* const*>(manager);
			return vtable[kCreateDynamicTriShapeSlot](manager, a_dynamicFlags, a_numTriangles, a_indices, a_numVertices, a_positions, a_texCoords0,
				a_texCoords1, a_normals, a_binormals, a_tangents, a_colors, a_skinBoneWeights, a_skinBoneIndices, a_landscapeData1, a_landscapeData2,
				a_eyeData);
		}

		RE::NiObject* Clone(RE::NiObject* a_object)
		{
			RE::NiCloningProcess process{};
			process.copyType = RE::NiCloningProcess::CopyType::kCopyExact;
			process.appendChar = '$';
			process.scale = { 1.0f, 1.0f, 1.0f };
			auto* clone = a_object->CreateClone(process);
			if (clone) {
				a_object->ProcessClone(process);
			}
			return clone;
		}

		RE::BSGeometry* FirstGeometry(RE::NiAVObject* a_object)
		{
			if (!a_object) {
				return nullptr;
			}
			if (auto* geometry = a_object->IsGeometry()) {
				return geometry;
			}
			if (auto* node = a_object->IsNode()) {
				for (auto& child : node->children) {
					if (auto* geometry = FirstGeometry(child.get())) {
						return geometry;
					}
				}
			}
			return nullptr;
		}

		const char* TypeOf(RE::NiObject* a_object)
		{
			const auto* rtti = a_object ? a_object->GetRTTI() : nullptr;
			return rtti && rtti->GetName() ? rtti->GetName() : "?";
		}

		RE::NiAVObject* FindBone(RE::NiAVObject* a_root, const std::string& a_name)
		{
			return a_root->GetObjectByName(RE::BSFixedString(a_name.c_str()));
		}

		RE::NiPoint3 Normalized(RE::NiPoint3 a_vector, const RE::NiPoint3& a_fallback)
		{
			return a_vector.Unitize() > 1e-5f ? a_vector : a_fallback;
		}
	}

	bool SpikeStrand::Create()
	{
		const auto& settings = Settings::Get();

		std::array<RE::NiPoint3, kVertices> positions;
		std::array<RE::NiPoint2, kVertices> uvs;
		std::array<RE::NiPoint3, kVertices> normals;
		std::array<RE::NiColorA, kVertices> colors;
		std::array<std::uint16_t, kTriangles * 3> indices;
		for (std::uint32_t ring = 0, v = 0; ring <= kSegments; ++ring) {
			for (std::uint32_t side = 0; side < kSides; ++side, ++v) {
				// Distinct, recognisable positions, so FindDynamicData can spot the vertex copy.
				positions[v] = { 0.5f + 1.25f * static_cast<float>(v), 7.0f, -3.0f };
				uvs[v] = { static_cast<float>(side) / kSides, static_cast<float>(ring) / kSegments };
				const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(side) / kSides;
				normals[v] = { std::cos(angle), std::sin(angle), 0.0f };
				colors[v] = { 1.0f, 1.0f, 1.0f, 1.0f };
			}
		}
		std::size_t i = 0;
		for (std::uint32_t ring = 0; ring < kSegments; ++ring) {
			for (std::uint32_t side = 0; side < kSides; ++side) {
				const auto a = static_cast<std::uint16_t>(ring * kSides + side);
				const auto b = static_cast<std::uint16_t>(ring * kSides + (side + 1) % kSides);
				const auto c = static_cast<std::uint16_t>(a + kSides);
				const auto d = static_cast<std::uint16_t>(b + kSides);
				indices[i++] = a;
				indices[i++] = c;
				indices[i++] = b;
				indices[i++] = b;
				indices[i++] = c;
				indices[i++] = d;
			}
		}

		auto* manager = RE::BSShaderResourceManager::GetSingleton();
		if (!manager) {
			REX::ERROR("strand: no BSShaderResourceManager");
			return false;
		}
		auto* created = CreateDynamicTriShape(manager, settings.strandDynamicFlags, kTriangles, indices.data(), kVertices,
			positions.data(), uvs.data(), nullptr, normals.data(), nullptr, nullptr, colors.data(), nullptr, nullptr, nullptr, nullptr, nullptr);
		if (!created) {
			REX::ERROR("strand: CreateDynamicTriShape returned nothing");
			return false;
		}
		shape.reset(created);
		REX::INFO("strand: created {} with {} vertices, {} triangles, vertex desc {:016X} (stride {}), renderer data {}", TypeOf(created),
			created->numVertices, created->numTriangles, created->vertexDesc.desc, created->vertexDesc.GetSize(), created->rendererData);
		if (!created->IsDynamicTriShape()) {
			REX::ERROR("strand: the factory didn't make a BSDynamicTriShape");
			return false;
		}
		created->name = "4Strands spike strand";
		return FindDynamicData(positions.data()) && BorrowShader();
	}

	bool SpikeStrand::FindDynamicData(const RE::NiPoint3* a_positions)
	{
		const auto* bytes = reinterpret_cast<const std::byte*>(shape.get());
		std::string fields;
		for (auto offset = kScanFrom; offset < kScanTo; offset += sizeof(void*)) {
			std::uintptr_t value = 0;
			Skin::SafeRead(bytes + offset, value);
			fields += std::format(" {:X}={:X}", offset, value);
		}
		REX::INFO("strand: fields after BSTriShape:{}", fields);

		for (auto offset = kScanFrom; offset < kScanTo; offset += sizeof(void*)) {
			std::byte* candidate = nullptr;
			if (!Skin::SafeRead(bytes + offset, candidate) || !candidate) {
				continue;
			}
			RE::NiPoint3 first{};
			if (!Skin::SafeRead(candidate, first) || first != a_positions[0]) {
				continue;
			}
			for (const std::uint32_t stride : { 16u, 12u }) {
				RE::NiPoint3 second{};
				if (Skin::SafeRead(candidate + stride, second) && second == a_positions[1]) {
					dynamicData = candidate;
					dynamicStride = stride;
					REX::INFO("strand: dynamic vertex data at +{:X} ({}), stride {}", offset, static_cast<void*>(candidate), stride);
					return true;
				}
			}
		}
		// Run 2 found pointers at +180 and +190, neither starting with the positions. Dump what they and the vertex
		// buffer hold, and look for the first position anywhere in their first 4 KB, as floats or half floats.
		const auto dump = [&](const char* a_label, const std::byte* a_address) {
			std::array<float, 12> floats{};
			if (!Skin::SafeRead(a_address, floats)) {
				REX::INFO("strand: {} {} unreadable", a_label, static_cast<const void*>(a_address));
				return 0uz;
			}
			std::string text;
			for (const auto value : floats) {
				text += std::format(" {:.3g}", value);
			}
			REX::INFO("strand: {} {} starts with floats{}", a_label, static_cast<const void*>(a_address), text);

			std::array<std::byte, 4096> block{};
			if (!Skin::SafeCopy(block.data(), a_address, block.size())) {
				return 0uz;
			}
			const auto leadingZeros = static_cast<std::size_t>(
				std::ranges::find_if(block, [](std::byte a_byte) { return a_byte != std::byte{ 0 }; }) - block.begin());
			REX::INFO("strand:   {} leading zero bytes", leadingZeros);
			const std::uint16_t half[3]{ 0x3800, 0x4700, 0xC200 };  // 0.5, 7, -3
			for (std::size_t at = 0; at + sizeof(RE::NiPoint3) <= block.size(); at += 2) {
				if (std::memcmp(block.data() + at, &a_positions[0], sizeof(RE::NiPoint3)) == 0) {
					REX::INFO("strand:   first position found at +{:X} as floats", at);
				}
				if (std::memcmp(block.data() + at, half, sizeof(half)) == 0) {
					REX::INFO("strand:   first position found at +{:X} as half floats", at);
				}
			}
			return leadingZeros;
		};
		// Run 3: the creation positions only reach the vertex buffer (as half floats), and +190 points at zeroed memory:
		// likely the per-frame float4 positions, left unfilled. Take a field only if its zeroed run fits all the vertices.
		std::size_t zeroedField = 0;
		for (auto offset = kScanFrom; offset < kScanTo; offset += sizeof(void*)) {
			std::uintptr_t value = 0;
			if (Skin::SafeRead(bytes + offset, value) && value > 0x10000 && value < 0x7FFF'FFFF'FFFF && value % 8 == 0) {
				const auto zeros = dump(std::format("field +{:X} ->", offset).c_str(), reinterpret_cast<const std::byte*>(value));
				if (!zeroedField && zeros >= kVertices * 16) {
					zeroedField = offset;
				}
			}
		}
		RE::BSGraphics::TriShape     triShape{};
		RE::BSGraphics::VertexBuffer buffer{};
		if (Skin::SafeRead(shape->rendererData, triShape) && Skin::SafeRead(triShape.vertexBuffer, buffer)) {
			REX::INFO("strand: renderer data desc {:016X}, vertex buffer CPU copy {} ({} bytes)", triShape.vertexDesc.desc, buffer.data, buffer.dataSize);
			if (buffer.data) {
				dump("vertex buffer CPU copy", static_cast<const std::byte*>(buffer.data));
			}
		}

		const auto& settings = Settings::Get();
		if (!settings.strandDynamicOffset && zeroedField) {
			Skin::SafeRead(bytes + zeroedField, dynamicData);
			dynamicStride = 16;
			REX::WARN("strand: guessing the zeroed block at +{:X} ({}) holds float4 positions, stride 16", zeroedField, static_cast<void*>(dynamicData));
			return true;
		}
		if (settings.strandDynamicOffset) {
			std::byte* forced = nullptr;
			if (Skin::SafeRead(bytes + settings.strandDynamicOffset, forced) && forced) {
				dynamicData = forced;
				dynamicStride = settings.strandDynamicStride;
				REX::WARN("strand: using StrandDynamicOffset +{:X} ({}), stride {} from the INI", settings.strandDynamicOffset, static_cast<void*>(forced),
					dynamicStride);
				return true;
			}
		}
		REX::ERROR("strand: no field points at the vertex positions; BSDynamicTriShape's layout is unknown, the strand stays off");
		return false;
	}

	bool SpikeStrand::BorrowShader()
	{
		const auto& path = Settings::Get().strandTemplate;
		RE::NiPointer<RE::NiNode>              model;
		RE::BSModelDB::DBTraits::ArgsType      args{};
		args.prepareAfterLoad = true;
		args.loadTextures = true;
		const auto error = RE::BSModelDB::Demand(path.c_str(), &model, args);
		if (error != RE::BSResource::ErrorCode::kNone || !model) {
			REX::ERROR("strand: could not load template mesh {} (error {})", path, std::to_underlying(error));
			return false;
		}
		auto* source = FirstGeometry(model.get());
		if (!source) {
			REX::ERROR("strand: template mesh {} has no geometry", path);
			return false;
		}
		for (std::size_t i = 0; i < 2; ++i) {
			auto* property = source->properties[i].get();
			if (property) {
				shape->properties[i].reset(static_cast<RE::NiProperty*>(Clone(property)));
			}
			REX::INFO("strand: property {} from {} '{}': {} -> {}", i, path, source->name.c_str(), TypeOf(property),
				static_cast<void*>(shape->properties[i].get()));
		}
		if (!shape->properties[1]) {
			REX::ERROR("strand: template mesh has no shader property to borrow");
			return false;
		}
		return true;
	}

	void SpikeStrand::Build(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float a_sag, std::array<RE::NiPoint3, kVertices>& a_out) const
	{
		const auto& settings = Settings::Get();
		const RE::NiPoint3 down{ 0.0f, 0.0f, -1.0f };
		std::array<RE::NiPoint3, kSegments + 1> centers;
		for (std::uint32_t ring = 0; ring <= kSegments; ++ring) {
			const float t = static_cast<float>(ring) / kSegments;
			centers[ring] = a_from + (a_to - a_from) * t + down * (a_sag * 4.0f * t * (1.0f - t));
		}
		for (std::uint32_t ring = 0; ring <= kSegments; ++ring) {
			const auto& before = centers[ring > 0 ? ring - 1 : ring];
			const auto& after = centers[std::min(ring + 1, kSegments)];
			const auto  along = Normalized(after - before, { 1.0f, 0.0f, 0.0f });
			const auto  side = Normalized(along.Cross(down), { 0.0f, 1.0f, 0.0f });
			const auto  up = side.Cross(along);
			for (std::uint32_t s = 0; s < kSides; ++s) {
				const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(s) / kSides;
				a_out[ring * kSides + s] = centers[ring] + (side * std::cos(angle) + up * std::sin(angle)) * settings.strandRadius;
			}
		}
	}

	void SpikeStrand::Update(RE::PlayerCharacter* a_player, float a_delta)
	{
		if (failed) {
			return;
		}
		auto* root = a_player->Get3D();
		if (!root || !root->IsNode()) {
			return;
		}
		if (attachedTo && attachedTo != root) {
			// The player's 3D was rebuilt; the old tree owns the old attachment.
			shape.reset();
			attachedTo = nullptr;
			dynamicData = nullptr;
		}
		const auto& settings = Settings::Get();
		auto*       boneA = FindBone(root, settings.strandBoneA);
		auto*       boneB = FindBone(root, settings.strandBoneB);
		if (!boneA || !boneB) {
			REX::ERROR("strand: player has no bone {} or {}; the strand stays off", settings.strandBoneA, settings.strandBoneB);
			failed = true;
			return;
		}
		if (!shape) {
			if (!Create()) {
				shape.reset();
				failed = true;
				return;
			}
			root->IsNode()->AttachChild(shape.get(), true);
			attachedTo = root;
			REX::INFO("strand: attached to '{}' between {} and {}", root->name.c_str(), settings.strandBoneA, settings.strandBoneB);
		}

		time += a_delta;
		const float sag = 6.0f + 3.0f * std::sin(time * 2.0f);
		std::array<RE::NiPoint3, kVertices> world;
		Build(boneA->world.translate, boneB->world.translate, sag, world);

		// The shape sits under the root with an identity local transform, so its vertices are in root space.
		const auto toLocal = root->world.Invert();
		for (std::uint32_t v = 0; v < kVertices; ++v) {
			const auto local = toLocal * world[v];
			std::memcpy(dynamicData + static_cast<std::size_t>(v) * dynamicStride, &local, sizeof(RE::NiPoint3));
		}
		const auto fromLocal = toLocal * boneA->world.translate;
		const auto toLocalB = toLocal * boneB->world.translate;
		shape->modelBound.center = (fromLocal + toLocalB) * 0.5f;
		shape->modelBound.fRadius = (toLocalB - fromLocal).Length() * 0.5f + sag + 10.0f;

		if (!loggedFirstFrame) {
			loggedFirstFrame = true;
			REX::INFO("strand: first frame, ends at {:.1f},{:.1f},{:.1f} and {:.1f},{:.1f},{:.1f} (root space), parent '{}'", fromLocal.x, fromLocal.y,
				fromLocal.z, toLocalB.x, toLocalB.y, toLocalB.z, shape->parent ? shape->parent->name.c_str() : "<none>");
		}
	}

	void SpikeStrand::Reset()
	{
		if (shape && attachedTo && shape->parent) {
			shape->parent->DetachChild(shape.get());
		}
		shape.reset();
		attachedTo = nullptr;
		dynamicData = nullptr;
		dynamicStride = 0;
		loggedFirstFrame = false;
		failed = false;
	}
}
