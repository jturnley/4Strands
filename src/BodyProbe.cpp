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
			if (!Skin::SafeCopy(bones.nodes.data(), instance.bones.data, bones.nodes.size() * sizeof(RE::NiAVObject*)) ||
				!Skin::SafeCopy(bones.skinToBone.data(), boneData.transforms.data, bones.skinToBone.size() * sizeof(Skin::BoneTransform))) {
				REX::WARN("    bone arrays unreadable");
				return std::nullopt;
			}
			std::string names;
			for (std::size_t i = 0; i < bones.nodes.size() && i < 12; ++i) {
				names += std::format("{}{}", i ? ", " : "", NameOf(bones.nodes[i]));
			}
			REX::INFO("    first bones: {}{}", names, bones.nodes.size() > 12 ? ", ..." : "");
			return bones;
		}

		struct SampledVertex
		{
			RE::NiPoint3  position;
			float         weights[4]{};
			std::uint8_t  bones[4]{};
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
				RE::NiPoint3 world{};
				float        total = 0.0f;
				int          main = -1;
				for (int i = 0; i < 4; ++i) {
					const auto bone = vertex->bones[i];
					const auto weight = vertex->weights[i];
					if (weight <= 0.0f || bone >= a_bones.nodes.size() || !a_bones.nodes[bone]) {
						continue;
					}
					const auto& boneWorld = a_bones.nodes[bone]->world;
					world += (boneWorld * (a_bones.skinToBone[bone].skinToBone * vertex->position)) * weight;
					total += weight;
					if (main < 0 || weight > vertex->weights[main]) {
						main = i;
					}
				}
				if (main < 0) {
					REX::INFO("    vertex {}: local {} has no usable weights (bones {} {} {} {})", index, Format(vertex->position),
						vertex->bones[0], vertex->bones[1], vertex->bones[2], vertex->bones[3]);
					continue;
				}
				const auto* mainBone = a_bones.nodes[vertex->bones[main]];
				REX::INFO("    vertex {}: local {} -> world {} (weights sum {:.2f}); main bone {} ({:.2f}) at {}, {:.1f} units away",
					index, Format(vertex->position), Format(world), total, NameOf(mainBone), vertex->weights[main],
					Format(mainBone->world.translate), (world - mainBone->world.translate).Length());
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
	}
}
