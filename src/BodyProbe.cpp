#include "BodyProbe.h"

#include "SkinLayout.h"

namespace FourStrands::BodyProbe
{
	namespace
	{
		using RE::BSGraphics::Vertex;

		constexpr std::uint32_t kMaxBones = 1024;
		constexpr std::uint32_t kSamples = 6;

		std::string Format(const RE::NiPoint3& a_point)
		{
			return std::format("({:.1f}, {:.1f}, {:.1f})", a_point.x, a_point.y, a_point.z);
		}

		const char* NameOf(const RE::NiAVObject* a_object)
		{
			const char* name = a_object ? a_object->name.c_str() : nullptr;
			return name && *name ? name : "<unnamed>";
		}

		const char* TypeOf(RE::NiAVObject* a_object)
		{
			const auto* rtti = a_object->GetRTTI();
			return rtti && rtti->GetName() ? rtti->GetName() : "?";
		}

		void Collect(RE::NiAVObject* a_object, std::vector<RE::BSGeometry*>& a_skinned)
		{
			if (!a_object) {
				return;
			}
			if (auto* geometry = a_object->IsGeometry()) {
				if (geometry->skinInstance) {
					a_skinned.push_back(geometry);
				}
				return;
			}
			if (auto* node = a_object->IsNode()) {
				for (auto& child : node->children) {
					Collect(child.get(), a_skinned);
				}
			}
		}

		std::uint32_t VertexCount(RE::BSGeometry* a_geometry)
		{
			auto* shape = a_geometry->IsTriShape();
			return shape ? shape->numVertices : 0;
		}

		struct Bones
		{
			std::vector<RE::NiAVObject*>    nodes;
			std::vector<RE::NiTransform*>   worldTransforms;
			std::vector<std::optional<RE::NiTransform>> world;  // from worldTransforms: most entries in nodes are null
			std::vector<Skin::BoneTransform> skinToBone;
		};

		// Reads the skin instance through the assumed layout, and checks that it's consistent.
		std::optional<Bones> ReadBones(RE::BSGeometry* a_geometry)
		{
			Skin::Instance instance{};
			if (!Skin::SafeRead(a_geometry->skinInstance.get(), instance)) {
				REX::WARN("    skin instance unreadable");
				return std::nullopt;
			}
			Skin::BoneData boneData{};
			const bool hasBoneData = Skin::SafeRead(instance.boneData, boneData);
			REX::INFO("    skin: {} bones (capacity {}), {} world transforms, bone data {} with {} skin-to-bone transforms, root {}",
				instance.bones.size, instance.bones.capacity, instance.worldTransforms.size, static_cast<void*>(instance.boneData),
				hasBoneData ? boneData.transforms.size : 0, static_cast<void*>(instance.rootNode));

			if (!hasBoneData || instance.bones.size == 0 || instance.bones.size > kMaxBones || instance.bones.size != boneData.transforms.size) {
				REX::WARN("    skin layout doesn't check out (bone count and transform count should match): BSSkin layout is wrong for this build");
				return std::nullopt;
			}
			Bones bones;
			bones.nodes.resize(instance.bones.size);
			bones.skinToBone.resize(instance.bones.size);
			if (instance.worldTransforms.size == instance.bones.size) {
				bones.worldTransforms.resize(instance.bones.size);
				if (!Skin::SafeCopy(bones.worldTransforms.data(), instance.worldTransforms.data, bones.worldTransforms.size() * sizeof(RE::NiTransform*))) {
					bones.worldTransforms.clear();
				}
			}
			if (!Skin::SafeCopy(bones.nodes.data(), instance.bones.data, bones.nodes.size() * sizeof(RE::NiAVObject*)) ||
				!Skin::SafeCopy(bones.skinToBone.data(), boneData.transforms.data, bones.skinToBone.size() * sizeof(Skin::BoneTransform))) {
				REX::WARN("    bone arrays unreadable");
				return std::nullopt;
			}
			bones.world.resize(bones.nodes.size());
			for (std::size_t i = 0; i < bones.world.size(); ++i) {
				RE::NiTransform transform{};
				if (i < bones.worldTransforms.size() && Skin::SafeRead(bones.worldTransforms[i], transform)) {
					bones.world[i] = transform;
				}
			}
			for (std::size_t i = 0; i < bones.nodes.size() && i < 6; ++i) {
				auto* bone = bones.nodes[i];
				auto* worldTransform = i < bones.worldTransforms.size() ? bones.worldTransforms[i] : nullptr;
				RE::NiTransform world{};
				const bool hasWorld = Skin::SafeRead(worldTransform, world);
				REX::INFO("    bone {}: {} {} '{}' at {}; world transform {} {} ({}); skin-to-bone translate {}", i, static_cast<void*>(bone),
					bone ? TypeOf(bone) : "-", NameOf(bone), bone ? Format(bone->world.translate) : "-", static_cast<void*>(worldTransform),
					hasWorld ? Format(world.translate) : "-", worldTransform == (bone ? &bone->world : nullptr) ? "is the bone's world" : "separate",
					Format(bones.skinToBone[i].skinToBone.translate));
			}
			return bones;
		}

		struct SampledVertex
		{
			RE::NiPoint3  position;
			float         weights[4]{};
			std::uint8_t  bones[4]{};
			std::uint8_t  skinRaw[12]{};
		};

		std::optional<SampledVertex> ReadVertex(const std::byte* a_data, std::uint32_t a_stride, RE::BSGraphics::VertexDesc a_desc, std::uint32_t a_index)
		{
			std::vector<std::byte> bytes(a_stride);
			if (!Skin::SafeCopy(bytes.data(), a_data + static_cast<std::size_t>(a_index) * a_stride, a_stride)) {
				return std::nullopt;
			}
			SampledVertex vertex;
			const auto positionAt = a_desc.GetAttributeOffset(Vertex::VA_POSITION);
			if (a_desc.HasFlag(Vertex::VF_FULLPREC)) {
				std::memcpy(&vertex.position, bytes.data() + positionAt, sizeof(RE::NiPoint3));
			} else {
				std::uint16_t half[3];
				std::memcpy(half, bytes.data() + positionAt, sizeof(half));
				vertex.position = { Skin::HalfToFloat(half[0]), Skin::HalfToFloat(half[1]), Skin::HalfToFloat(half[2]) };
			}
			if (a_desc.HasFlag(Vertex::VF_SKINNED)) {
				const auto skinAt = a_desc.GetAttributeOffset(Vertex::VA_SKINNING);
				std::uint16_t half[4];
				std::memcpy(half, bytes.data() + skinAt, sizeof(half));
				std::memcpy(vertex.bones, bytes.data() + skinAt + sizeof(half), sizeof(vertex.bones));
				std::memcpy(vertex.skinRaw, bytes.data() + skinAt, sizeof(vertex.skinRaw));
				for (int i = 0; i < 4; ++i) {
					vertex.weights[i] = Skin::HalfToFloat(half[i]);
				}
			}
			return vertex;
		}

		// Skins a few vertices with the bones' current world transforms. If the layout and the math are right,
		// each lands a few units from its main bone; if not, the distances are huge or the vertex is garbage.
		void SampleVertices(RE::BSGeometry* a_geometry, const Bones& a_bones)
		{
			auto* rendererData = static_cast<RE::BSGraphics::TriShape*>(a_geometry->rendererData);
			RE::BSGraphics::TriShape triShape{};
			RE::BSGraphics::VertexBuffer buffer{};
			if (!Skin::SafeRead(rendererData, triShape) || !Skin::SafeRead(triShape.vertexBuffer, buffer)) {
				REX::WARN("    renderer data or vertex buffer unreadable");
				return;
			}
			auto desc = a_geometry->vertexDesc;
			auto       stride = desc.GetSize();
			const auto count = VertexCount(a_geometry);
			REX::INFO("    renderer data: vertex desc {:016X} ({}), CPU copy {} ({} of {} bytes, offset {}, invalid {}), d3d buffer {}",
				triShape.vertexDesc.desc, triShape.vertexDesc.desc == desc.desc ? "matches" : "differs from the geometry's", buffer.data,
				buffer.dataSize, buffer.maxDataSize, buffer.dataOffset, buffer.invalidCpuData, static_cast<void*>(buffer.buffer));
			if (!buffer.data) {
				REX::WARN("    NO CPU COPY of the vertices: body sampling needs the NIF read from disk or a GPU readback");
				return;
			}
			if (stride == 0 || count == 0) {
				REX::WARN("    stride {} or vertex count {} is zero", stride, count);
				return;
			}
			if (buffer.dataSize && buffer.dataSize < stride * count) {
				REX::WARN("    CPU copy is {} bytes, smaller than {} vertices x {} bytes", buffer.dataSize, count, stride);
			}

			const auto* data = static_cast<const std::byte*>(buffer.data);
			for (std::uint32_t s = 0; s < kSamples; ++s) {
				const auto index = count * s / kSamples;
				const auto vertex = ReadVertex(data, stride, desc, index);
				if (!vertex) {
					REX::WARN("    vertex {} unreadable", index);
					continue;
				}
				std::string raw;
				for (const auto byte : vertex->skinRaw) {
					raw += std::format("{:02X} ", byte);
				}
				REX::INFO("    vertex {} skinning bytes: {}(weights {:.3f} {:.3f} {:.3f} {:.3f} as half floats)", index, raw, vertex->weights[0],
					vertex->weights[1], vertex->weights[2], vertex->weights[3]);
				int main = -1;
				for (int i = 0; i < 4; ++i) {
					const auto bone = vertex->bones[i];
					if (vertex->weights[i] > 0.0f && bone < a_bones.world.size() && a_bones.world[bone] &&
						(main < 0 || vertex->weights[i] > vertex->weights[main])) {
						main = i;
					}
				}
				if (main < 0) {
					REX::INFO("    vertex {}: local {} has no usable weights (bones {} {} {} {})", index, Format(vertex->position),
						vertex->bones[0], vertex->bones[1], vertex->bones[2], vertex->bones[3]);
					continue;
				}
				const auto mainIndex = vertex->bones[main];
				const auto mainAt = a_bones.world[mainIndex]->translate;
				REX::INFO("    vertex {}: local {}, main bone {} '{}' ({:.2f}) at {}; rigid through the geometry's world: {:.1f} units away",
					index, Format(vertex->position), mainIndex, NameOf(a_bones.nodes[mainIndex]), vertex->weights[main], Format(mainAt),
					((a_geometry->world * vertex->position) - mainAt).Length());

				// Run 3's vertices landed 25-107 units from their bones, so try the conventions side by side. The right one
				// puts the vertex a few units from its main bone in bone space, and in world space.
				const std::pair<const char*, RE::NiTransform (*)(const RE::NiTransform&)> conventions[]{
					{ "skin-to-bone", [](const RE::NiTransform& a_t) { return a_t; } },
					{ "transposed", [](const RE::NiTransform& a_t) { return RE::NiTransform(a_t.rotate.Transpose(), a_t.translate, a_t.scale); } },
					{ "inverted", [](const RE::NiTransform& a_t) { return a_t.Invert(); } },
				};
				for (const auto& [label, convert] : conventions) {
					RE::NiPoint3 world{};
					for (int i = 0; i < 4; ++i) {
						const auto bone = vertex->bones[i];
						if (vertex->weights[i] > 0.0f && bone < a_bones.world.size() && a_bones.world[bone]) {
							world += (*a_bones.world[bone] * (convert(a_bones.skinToBone[bone].skinToBone) * vertex->position)) * vertex->weights[i];
						}
					}
					const auto boneSpace = convert(a_bones.skinToBone[mainIndex].skinToBone) * vertex->position;
					REX::INFO("      {}: {:.1f} from the bone in bone space, world {} {:.1f} away", label, boneSpace.Length(), Format(world),
						(world - mainAt).Length());
				}
			}
		}
	}

	namespace
	{
		// The spike's own BSDynamicTriShape gets no per-frame vertex buffer. Head parts are BSDynamicTriShapes the engine
		// made itself. Run 5 showed +170 holds vertices x 12 (float3 positions), so read each pointer field as that many
		// float3s and see which one looks like a head: all finite, and within a few hundred units of the origin.
		void ProbeDynamic(RE::BSGeometry* a_geometry)
		{
			auto* shape = a_geometry->IsTriShape();
			if (!shape) {
				return;
			}
			const auto* bytes = reinterpret_cast<const std::byte*>(shape);
			std::string fields;
			for (std::size_t offset = 0x170; offset < 0x1C0; offset += 8) {
				std::uint64_t value = 0;
				Skin::SafeRead(bytes + offset, value);
				fields += std::format(" {:X}={:X}", offset, value);
			}
			std::uint32_t dataSize = 0;
			Skin::SafeRead(bytes + 0x170, dataSize);
			REX::INFO("  dynamic '{}': {} vertices, +170 = {} ({} per vertex); fields{}", NameOf(a_geometry), shape->numVertices, dataSize,
				shape->numVertices ? static_cast<float>(dataSize) / shape->numVertices : 0.0f, fields);

			const std::size_t count = std::min<std::size_t>(shape->numVertices, 4096);
			std::vector<RE::NiPoint3> points(count);
			for (std::size_t offset = 0x170; offset < 0x1C0; offset += 8) {
				std::uintptr_t value = 0;
				if (!Skin::SafeRead(bytes + offset, value) || value < 0x10000 || value > 0x7FFF'FFFF'FFFF || value % 8) {
					continue;
				}
				if (!Skin::SafeCopy(points.data(), reinterpret_cast<const void*>(value), points.size() * sizeof(RE::NiPoint3))) {
					REX::INFO("    field +{:X}: {} float3s unreadable", offset, count);
					continue;
				}
				std::size_t  plausible = 0;
				std::size_t  zero = 0;
				RE::NiPoint3 low{ FLT_MAX, FLT_MAX, FLT_MAX };
				RE::NiPoint3 high{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
				for (const auto& point : points) {
					if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z) || std::abs(point.x) > 500.0f ||
						std::abs(point.y) > 500.0f || std::abs(point.z) > 500.0f) {
						continue;
					}
					++plausible;
					zero += point == RE::NiPoint3{};
					low = { std::min(low.x, point.x), std::min(low.y, point.y), std::min(low.z, point.z) };
					high = { std::max(high.x, point.x), std::max(high.y, point.y), std::max(high.z, point.z) };
				}
				REX::INFO("    field +{:X} as {} float3s: {} plausible ({} zero), box {} to {}, first {}", offset, count, plausible, zero,
					plausible ? Format(low) : "-", plausible ? Format(high) : "-", Format(points[0]));
			}
		}
	}

	void Probe(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		auto* root = a_actor->Get3D();
		const char* actorName = a_actor->GetDisplayFullName();
		REX::INFO("body probe: {} [{:08X}] at {}, 3D root {} at {}", actorName ? actorName : "?", a_actor->GetFormID(),
			Format(a_actor->GetPosition()), root ? NameOf(root) : "<none>", root ? Format(root->world.translate) : "-");
		if (!root) {
			return;
		}

		std::vector<RE::BSGeometry*> skinned;
		Collect(root, skinned);
		RE::BSGeometry* largest = nullptr;
		for (auto* geometry : skinned) {
			auto desc = geometry->vertexDesc;
			REX::INFO("  {} '{}': {} vertices, vertex desc {:016X} (flags {:04X}, stride {}, skinning at {}, {} precision)",
				TypeOf(geometry), NameOf(geometry), VertexCount(geometry), desc.desc, static_cast<std::uint16_t>(desc.GetFlags()),
				desc.GetSize(), desc.GetAttributeOffset(Vertex::VA_SKINNING), desc.HasFlag(Vertex::VF_FULLPREC) ? "full" : "half");
			if (!largest || VertexCount(geometry) > VertexCount(largest)) {
				largest = geometry;
			}
		}
		if (!largest) {
			REX::INFO("  no skinned geometry");
			return;
		}
		REX::INFO("  sampling '{}':", NameOf(largest));
		if (const auto bones = ReadBones(largest)) {
			SampleVertices(largest, *bones);
		}
		int dynamicCount = 0;
		for (auto* geometry : skinned) {
			if (geometry->IsDynamicTriShape() && dynamicCount++ < 3) {
				ProbeDynamic(geometry);
			}
		}
	}
}
