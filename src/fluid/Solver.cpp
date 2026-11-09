#include "fluid/Solver.h"

#include "fluid/Clusters.h"
#include "fluid/Kinds.h"

#include <algorithm>
#include <stdexcept>

namespace FourStrands::Fluid
{
	namespace
	{
		constexpr float kMaxSpeed = 6.0f;
		constexpr float kDomain = 1000.0f;
		constexpr float kFloorFriction = 8.0f;
		constexpr float kLinkStiffness = 0.6f;
		constexpr float kLinkBreak = 12.0f;
		constexpr float kLinkSnap = 6.0f;
		constexpr float kSleepSpeed = 0.02f;
		constexpr float kWakeSpeed = 0.08f;
		constexpr float kSleepTime = 0.4f;
		constexpr float kSkinGravity = 0.25f;
		constexpr float kGroundSlide = 0.2f;
		constexpr float kStickReach = 2.0f;
		constexpr float kSeparation = 0.1f;
		constexpr float kAnchorBreak = 12.0f;
		constexpr float kAnchorFlow = 15.0f;
		constexpr float kCreep = 0.035f;
		constexpr std::size_t kMaxLoad = 64;
		constexpr float kCohesionFloor = 1.0f;
		constexpr float kFroth = 0.3f;       // metres of shaft sliding past it (about 2 s of brisk strokes) before a mix foams
		constexpr float kMixRate = 0.3f;     // per second: how fast touching semen and secretion even out their mix
	}

	std::size_t Solver::CellHash::operator()(Cell a_cell) const noexcept
	{
		return (static_cast<std::uint32_t>(a_cell.x) * 73856093u) ^ (static_cast<std::uint32_t>(a_cell.y) * 19349663u) ^
		       (static_cast<std::uint32_t>(a_cell.z) * 83492791u);
	}

	bool Solver::Valid(const Parameters& a_parameters)
	{
		const auto within = [](float a_value, float a_min, float a_max) { return std::isfinite(a_value) && a_value >= a_min && a_value <= a_max; };
		return a_parameters.iterations > 0 && a_parameters.iterations <= 16 && within(a_parameters.cohesion, 0.0f, 20.0f) &&
		       within(a_parameters.viscosity, 0.0f, 100.0f) && within(a_parameters.gravity, 0.0f, 30.0f) &&
		       within(a_parameters.stretch, 0.0f, 30.0f) && within(a_parameters.friction, 0.0f, 200.0f) && within(a_parameters.grip, 0.0f, 10.0f) &&
		       within(a_parameters.liquefy, 0.0f, 7200.0f);
	}

	void Solver::Configure(const Parameters& a_parameters)
	{
		if (Valid(a_parameters)) {
			_parameters = a_parameters;
		}
	}

	Solver::Solver(float a_spacing, Parameters a_parameters) :
		_spacing(a_spacing), _radiusSquared(4.0f * a_spacing * a_spacing), _parameters(a_parameters)
	{
		if (!std::isfinite(a_spacing) || a_spacing < 1e-4f || a_spacing > 0.1f || !Valid(a_parameters)) {
			throw std::invalid_argument("invalid fluid parameters");
		}
		for (int z = -2; z <= 2; ++z) {
			for (int y = -2; y <= 2; ++y) {
				for (int x = -2; x <= 2; ++x) {
					_restWeight += Weight(Vec3{ static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) } * _spacing);
				}
			}
		}
	}

	Solver::Cell Solver::CellOf(Vec3 a_point) const
	{
		const float size = 2.0f * _spacing;
		return { static_cast<int>(std::floor(a_point.x / size)), static_cast<int>(std::floor(a_point.y / size)),
			static_cast<int>(std::floor(a_point.z / size)) };
	}

	float Solver::Weight(Vec3 a_delta) const
	{
		const float q = std::max(0.0f, 1.0f - LengthSquared(a_delta) / _radiusSquared);
		return q * q * q;
	}

	Vec3 Solver::Gradient(Vec3 a_delta) const
	{
		const float q = std::max(0.0f, 1.0f - LengthSquared(a_delta) / _radiusSquared);
		return a_delta * (-6.0f * q * q / (_radiusSquared * _restWeight));
	}

	void Solver::FindNeighbors(std::span<const Particle> a_particles, bool a_all)
	{
		for (auto& [cell, members] : _grid) {
			members.clear();
		}
		for (std::size_t i = 0; i < _predicted.size(); ++i) {
			_grid[CellOf(_predicted[i])].push_back(i);
		}
		_neighbors.resize(_predicted.size());
		for (std::size_t i = 0; i < _predicted.size(); ++i) {
			auto& neighbors = _neighbors[i];
			neighbors.clear();
			if (!a_all && a_particles[i].asleep) {
				continue;
			}
			const auto cell = CellOf(_predicted[i]);
			for (int z = -1; z <= 1; ++z) {
				for (int y = -1; y <= 1; ++y) {
					for (int x = -1; x <= 1; ++x) {
						const auto found = _grid.find({ cell.x + x, cell.y + y, cell.z + z });
						if (found == _grid.end()) {
							continue;
						}
						for (const auto j : found->second) {
							if (i != j && LengthSquared(_predicted[i] - _predicted[j]) < _radiusSquared) {
								neighbors.push_back(j);
							}
						}
					}
				}
			}
		}
		if (_grid.size() > 4 * _predicted.size() + 64) {
			std::erase_if(_grid, [](const auto& a_entry) { return a_entry.second.empty(); });
		}
	}

	void Solver::Collide(std::span<const Particle> a_particles, const Colliders& a_colliders, bool a_pull)
	{
		const float margin = 0.5f * _spacing;
		for (std::size_t i = 0; i < _predicted.size(); ++i) {
			if (a_particles[i].asleep) {
				continue;
			}
			// inside a canal her body's colliders are all round it: only the canal holds it (Contain), and only a shaft
			// in it pushes and drags it
			const bool  inCanal = a_particles[i].canal >= 0;
			const auto  kind = Effective(a_particles[i].kind, a_particles[i].mix, a_particles[i].age, _parameters.liquefy);
			const float reach = margin + kStickReach * kind.stickReach * _spacing;
			const float hanging = kind.hang;
			Vec3& point = _predicted[i];
			if (!inCanal && std::ranges::any_of(a_colliders.holes, [&](const Hole& a_hole) { return LengthSquared(point - a_hole.center) < a_hole.radius * a_hole.radius; })) {
				continue;
			}
			for (std::size_t k = 0; k < a_colliders.capsules.size(); ++k) {
				const auto& capsule = a_colliders.capsules[k];
				if (inCanal && !capsule.shaft) {
					continue;
				}
				const float extent = capsule.radius + reach;
				if (point.x < std::min(capsule.a.x, capsule.b.x) - extent || point.x > std::max(capsule.a.x, capsule.b.x) + extent ||
					point.y < std::min(capsule.a.y, capsule.b.y) - extent || point.y > std::max(capsule.a.y, capsule.b.y) + extent ||
					point.z < std::min(capsule.a.z, capsule.b.z) - extent || point.z > std::max(capsule.a.z, capsule.b.z) + extent) {
					continue;
				}
				const Vec3  span = capsule.b - capsule.a;
				const float length = LengthSquared(span);
				const float t = length > 1e-12f ? std::clamp(Dot(point - capsule.a, span) / length, 0.0f, 1.0f) : 0.0f;
				const Vec3  closest = capsule.a + span * t;
				const Vec3  offset = point - closest;
				const float distance = Length(offset);
				const float limit = capsule.radius + margin;
				const Vec3  normal = distance > 1e-7f ? offset * (1.0f / distance) : Vec3{ 0.0f, 0.0f, 1.0f };
				if (distance < limit) {
					point = closest + normal * limit;
				} else if (a_pull && a_particles[i].touching && distance < capsule.radius + reach && normal.z > hanging) {
					point = closest + normal * limit;
				} else {
					continue;
				}
				_contact[i] = static_cast<int>(k);
				_along[i] = t;
				_normal[i] = normal;
			}
			if (!a_colliders.skin || inCanal) {
				continue;
			}
			const auto& patches = a_colliders.skin->Patches();
			for (int pass = 0; pass < 2; ++pass) {
				const std::size_t k = a_colliders.skin->Nearest(point);
				if (k == kNone) {
					break;
				}
				const auto& patch = patches[k];
				const float depth = Dot(point - patch.position, patch.normal);
				const bool  pulled = a_pull && a_particles[i].touching && depth < reach && patch.normal.z > hanging;
				if (depth >= margin && !pulled) {
					break;
				}
				point = point + patch.normal * (margin - depth);
				_contact[i] = static_cast<int>(a_colliders.capsules.size() + k);
				_along[i] = 0.0f;
				_normal[i] = patch.normal;
			}
		}
	}

	// Holds each particle that is in a canal inside it: within its tapering radius, and short of any end it may not pass
	// (a closed deep end, a closed entrance, or the way back out of a canal whose seep allowance is spent)
	// Which canals have a shaft through their open entrance (he is in her): it seals her (Contain, PassCanals)
	void Solver::Plugs(const Colliders& a_colliders)
	{
		_plug.assign(a_colliders.canals.size(), -1);
		for (std::size_t c = 0; c < a_colliders.canals.size(); ++c) {
			const auto& canal = a_colliders.canals[c];
			if (canal.previous >= 0 || !canal.open) {
				continue;
			}
			for (std::size_t k = 0; k < a_colliders.capsules.size(); ++k) {
				const auto& shaft = a_colliders.capsules[k];
				if (!shaft.shaft) {
					continue;
				}
				const Vec3  span = shaft.b - shaft.a;
				const float length2 = LengthSquared(span);
				if (length2 < 1e-10f) {
					continue;
				}
				// through the entrance: the entrance's middle lies alongside the shaft (not past either end), within reach
				const float s = Dot(canal.a - shaft.a, span) / length2;
				if (s > 0.0f && s < 1.0f && Length(canal.a - (shaft.a + span * s)) < canal.radiusA) {
					_plug[c] = static_cast<int>(k);
					break;
				}
			}
		}
	}

	void Solver::Contain(std::span<const Particle> a_particles, const Colliders& a_colliders)
	{
		const auto  a_canals = a_colliders.canals;
		const float margin = 0.5f * _spacing;
		for (std::size_t i = 0; i < _predicted.size(); ++i) {
			const int c = a_particles[i].canal;
			if (c < 0 || static_cast<std::size_t>(c) >= a_canals.size()) {
				continue;
			}
			const auto  index = static_cast<std::size_t>(c);
			const auto& canal = a_canals[index];
			const Vec3  span = canal.b - canal.a;
			const float length2 = LengthSquared(span);
			if (length2 < 1e-10f) {
				continue;
			}
			const float length = std::sqrt(length2);
			Vec3&       point = _predicted[i];
			const float t = Dot(point - canal.a, span) / length2;
			// a shaft through the entrance seals it: what is in her stays until he pulls out (and what coats him comes out on him)
			const bool  plugged = canal.previous < 0 && index < _plug.size() && _plug[index] >= 0;
			const bool  outA = canal.previous >= 0 ?
			                       (canal.seep < 0.0f || (index < _seepBudget.size() && _seepBudget[index] >= 1.0f)) :
			                       canal.open && !plugged;
			const float lo = outA ? -1e9f : std::min(0.5f, margin / length);
			const bool  inB = canal.next >= 0 && static_cast<std::size_t>(canal.next) < a_canals.size() &&
			                 (a_canals[static_cast<std::size_t>(canal.next)].inflow < 0.0f ||
			                     (static_cast<std::size_t>(canal.next) < _inBudget.size() && _inBudget[static_cast<std::size_t>(canal.next)] >= 1.0f));
			const float hi = inB ? 1e9f : std::max(0.5f, 1.0f - margin / length);
			const float held = std::clamp(t, lo, hi);
			const float inside = std::clamp(held, 0.0f, 1.0f);
			const Vec3  axis = canal.a + span * held;
			Vec3        radial = point - axis;
			if (held == inside) {  // between its ends: within the wall
				const float radius = std::max(0.25f * _spacing, canal.radiusA + (canal.radiusB - canal.radiusA) * inside - margin);
				const float distance = Length(radial);
				if (distance > radius) {
					radial = radial * (radius / distance);
				}
			}
			point = axis + radial;
			// where his shaft fills her canal (no room beside him for a particle), what lies ahead of his tip is pushed and
			// held ahead of it, and what lies alongside him stays against her wall as a film
			for (const auto& shaft : a_colliders.capsules) {
				if (!shaft.shaft) {
					continue;
				}
				const Vec3  along = shaft.b - shaft.a;
				const float tip = Length(along);
				if (tip < 1e-5f) {
					continue;
				}
				const Vec3  unit = along * (1.0f / tip);
				const float s = Dot(point - shaft.a, unit);
				Vec3        across = point - (shaft.a + unit * s);
				const float off = Length(across);
				const float wall = canal.radiusA + (canal.radiusB - canal.radiusA) * inside;
				if (s < 0.0f || s > tip + _spacing || off > shaft.radius + margin || wall > shaft.radius + 2.0f * margin) {
					continue;
				}
				if (s > tip - _spacing) {
					point = shaft.a + unit * (tip + _spacing) + across;
				} else {
					const Vec3 out = off > 1e-6f ? across * (1.0f / off) : Normalize(Cross(unit, Vec3{ 0.0f, 0.0f, 1.0f }), Vec3{ 1.0f, 0.0f, 0.0f });
					point = shaft.a + unit * s + out * std::max(wall - margin, 0.5f * _spacing);
				}
			}
		}
	}

	// At the end of a step: particles past a canal's end move into the next one, back into the previous one (spending
	// the seep allowance where there is one), or out through an open entrance; free ones that cross an open entrance
	// inward join its canal
	void Solver::PassCanals(std::span<Particle> a_particles, const Colliders& a_colliders, float a_seconds)
	{
		const auto a_canals = a_colliders.canals;
		_seepBudget.resize(a_canals.size(), 0.0f);
		_inBudget.resize(a_canals.size(), 0.0f);
		for (std::size_t c = 0; c < a_canals.size(); ++c) {
			if (a_canals[c].seep >= 0.0f) {
				_seepBudget[c] = std::min(2.0f, _seepBudget[c] + a_canals[c].seep * a_seconds);
			}
			if (a_canals[c].inflow >= 0.0f) {
				_inBudget[c] = std::min(2.0f, _inBudget[c] + a_canals[c].inflow * a_seconds);
			}
		}
		for (std::size_t i = 0; i < a_particles.size(); ++i) {
			auto&     particle = a_particles[i];
			const int c = particle.canal;
			if (c >= 0 && static_cast<std::size_t>(c) < a_canals.size()) {
				const auto  index = static_cast<std::size_t>(c);
				const auto& canal = a_canals[index];
				const Vec3  span = canal.b - canal.a;
				const float length2 = LengthSquared(span);
				const float t = length2 > 1e-10f ? Dot(_predicted[i] - canal.a, span) / length2 : 0.5f;
				if (t > 1.0f && canal.next >= 0 && static_cast<std::size_t>(canal.next) < a_canals.size()) {
					const auto next = static_cast<std::size_t>(canal.next);
					if (a_canals[next].inflow < 0.0f) {
						particle.canal = static_cast<std::int16_t>(canal.next);
					} else if (_inBudget[next] >= 1.0f) {
						_inBudget[next] -= 1.0f;
						particle.canal = static_cast<std::int16_t>(canal.next);
					}
				} else if (t < 0.0f && canal.previous >= 0) {
					if (canal.seep < 0.0f) {
						particle.canal = static_cast<std::int16_t>(canal.previous);
					} else if (_seepBudget[index] >= 1.0f) {
						_seepBudget[index] -= 1.0f;
						particle.canal = static_cast<std::int16_t>(canal.previous);
					}
				} else if (t < 0.0f && canal.open && (index >= _plug.size() || _plug[index] < 0)) {
					particle.canal = -1;
				}
				continue;
			}
			if (c >= 0) {  // its canal is gone (fewer this frame): let it go
				particle.canal = -1;
				continue;
			}
			for (std::size_t k = 0; k < a_canals.size(); ++k) {
				const auto& canal = a_canals[k];
				if (canal.previous >= 0 || !canal.open) {
					continue;
				}
				const Vec3  span = canal.b - canal.a;
				const float length2 = LengthSquared(span);
				if (length2 < 1e-10f) {
					continue;
				}
				const float t = Dot(_predicted[i] - canal.a, span) / length2;
				const Vec3  radial = _predicted[i] - (canal.a + span * t);
				if (t >= 0.0f && t <= 0.3f && LengthSquared(radial) < canal.radiusA * canal.radiusA) {
					particle.canal = static_cast<std::int16_t>(k);
					break;
				}
			}
		}
	}

	std::optional<Vec3> Solver::Carrier(const Colliders& a_colliders, int a_surface, float a_along, Vec3 a_position)
	{
		if (a_surface < 0) {
			return std::nullopt;
		}
		const auto index = static_cast<std::size_t>(a_surface);
		if (index < a_colliders.capsules.size()) {
			const auto& capsule = a_colliders.capsules[index];
			return capsule.velocityA + (capsule.velocityB - capsule.velocityA) * a_along;
		}
		if (!a_colliders.skin) {
			return std::nullopt;
		}
		const std::size_t nearest = a_colliders.skin->Nearest(a_position);
		if (nearest == kNone) {
			return std::nullopt;
		}
		return a_colliders.skin->Patches()[nearest].velocity;
	}

	void Solver::Step(std::span<Particle> a_particles, std::span<Anchor> a_anchors, const Colliders& a_colliders, const Ground& a_ground, float a_seconds)
	{
		if (!std::isfinite(a_seconds) || a_seconds <= 0.0f || a_seconds > 0.05f) {
			throw std::invalid_argument("invalid fluid step");
		}
		for (const auto& particle : a_particles) {
			if (!IsFinite(particle.position) || !IsFinite(particle.velocity) ||
				std::max({ std::abs(particle.position.x), std::abs(particle.position.y), std::abs(particle.position.z) }) > kDomain) {
				throw std::invalid_argument("a fluid particle left the simulation domain");
			}
		}
		const std::size_t count = a_particles.size();
		if (count == 0) {
			return;
		}
		_predicted.resize(count);
		_velocity.resize(count);
		_correction.resize(count);
		_lambda.resize(count);
		_contact.assign(count, -1);
		_along.resize(count);
		_normal.resize(count);
		const float lift = _spacing * 0.5f;
		_floor.resize(count);
		_anchored.assign(count, false);
		for (const auto& anchor : a_anchors) {
			if (anchor.rest > 0.0f && anchor.particle < count) {
				_anchored[anchor.particle] = true;
			}
		}
		for (std::size_t i = 0; i < count; ++i) {
			_predicted[i] = a_particles[i].position;
		}
		FindNeighbors(a_particles, true);
		for (std::size_t i = 0; i < count; ++i) {
			auto& particle = a_particles[i];
			if (!particle.asleep) {
				continue;
			}
			const bool stirred = _anchored[i] || std::ranges::any_of(_neighbors[i], [&](std::size_t j) {
				return !a_particles[j].asleep && LengthSquared(a_particles[j].velocity) > kWakeSpeed * kWakeSpeed;
			});
			if (stirred) {
				particle.asleep = false;
				particle.idle = 0.0f;
			}
		}
		_weights.assign(count, 0.0f);
		for (std::size_t i = 0; i < count; ++i) {
			for (const auto j : _neighbors[i]) {
				_weights[i] += Weight(a_particles[j].position - a_particles[i].position);
			}
		}
		for (std::size_t i = 0; i < count; ++i) {
			if (a_particles[i].asleep) {
				continue;
			}
			Vec3 cohesion{};
			for (const auto j : _neighbors[i]) {
				const auto  delta = a_particles[j].position - a_particles[i].position;
				const float shared = std::max(0.5f * (_weights[i] + _weights[j]), kCohesionFloor);
				// a pair holds together as the less cohesive of its two kinds does (water barely pulls on anything)
				const float pair = std::min(Effective(a_particles[i].kind, a_particles[i].mix, a_particles[i].age, _parameters.liquefy).cohesion,
					Effective(a_particles[j].kind, a_particles[j].mix, a_particles[j].age, _parameters.liquefy).cohesion);
				cohesion += delta * (Weight(delta) / shared * pair);
			}
			cohesion = cohesion * (_parameters.cohesion / _spacing);
			const auto& particle = a_particles[i];
			Vec3        moving = particle.velocity;
			if (particle.touching) {
				if (const auto carrier = Carrier(a_colliders, particle.surface, particle.along, particle.position)) {
					moving = moving - particle.carried + *carrier;
				}
			}
			const float gravity = particle.touching ? _parameters.gravity * kSkinGravity : _parameters.gravity;
			Vec3        slide{};
			if (particle.position.z <= a_ground.Height(particle.position.x, particle.position.y) + lift * 1.05f) {
				const Vec3 slope = a_ground.Gradient(particle.position.x, particle.position.y);
				slide = slope * (-_parameters.gravity * kGroundSlide / (1.0f + LengthSquared(slope)));
			}
			const auto velocity = moving + (cohesion + slide + Vec3{ 0.0f, 0.0f, -gravity }) * a_seconds;
			_predicted[i] = a_particles[i].position + LimitLength(velocity, kMaxSpeed) * a_seconds;
			_floor[i] = a_ground.Height(_predicted[i].x, _predicted[i].y) + lift;
			_predicted[i].z = std::max(_predicted[i].z, _floor[i]);
		}
		for (std::size_t i = 0; i < count; ++i) {
			if (a_particles[i].asleep) {
				_floor[i] = a_ground.Height(_predicted[i].x, _predicted[i].y) + lift;
			}
		}
		Collide(a_particles, a_colliders, true);
		Plugs(a_colliders);

		for (std::uint32_t iteration = 0; iteration < _parameters.iterations; ++iteration) {
			FindNeighbors(a_particles, false);
			for (std::size_t i = 0; i < count; ++i) {
				if (a_particles[i].asleep) {
					_lambda[i] = 0.0f;
					continue;
				}
				float density = 1.0f;
				float denominator = 0.01f / (_spacing * _spacing);
				Vec3  gradients{};
				for (const auto j : _neighbors[i]) {
					const auto delta = _predicted[i] - _predicted[j];
					density += Weight(delta);
					const auto gradient = Gradient(delta);
					gradients += gradient;
					denominator += Dot(gradient, gradient);
				}
				denominator += Dot(gradients, gradients);
				_lambda[i] = -std::max(0.0f, density / _restWeight - 1.0f) / denominator;
			}
			for (std::size_t i = 0; i < count; ++i) {
				Vec3 correction{};
				if (a_particles[i].asleep) {
					_correction[i] = correction;
					continue;
				}
				for (const auto j : _neighbors[i]) {
					correction += Gradient(_predicted[i] - _predicted[j]) * (_lambda[i] + _lambda[j]);
				}
				_correction[i] = LimitLength(correction, 0.25f * _spacing);
			}
			for (std::size_t i = 0; i < count; ++i) {
				_predicted[i] += _correction[i];
			}
			for (std::size_t i = 0; i + 1 < count; ++i) {
				const float rest = a_particles[i].link;
				if (rest <= 0.0f) {
					continue;
				}
				const bool  first = !a_particles[i].asleep;
				const bool  second = !a_particles[i + 1].asleep;
				const Vec3  delta = _predicted[i + 1] - _predicted[i];
				const float length = Length(delta);
				if (length > 1e-6f && (first || second)) {
					const float share = first && second ? 0.5f : 1.0f;
					const Vec3  correction = LimitLength(delta * ((length - rest) / length * share * kLinkStiffness), 0.25f * _spacing);
					if (first) {
						_predicted[i] += correction;
					}
					if (second) {
						_predicted[i + 1] = _predicted[i + 1] - correction;
					}
				}
			}
			for (const auto& anchor : a_anchors) {
				if (anchor.rest <= 0.0f || anchor.particle >= count) {
					continue;
				}
				auto&       position = _predicted[anchor.particle];
				const Vec3  delta = position - anchor.point;
				const float length = Length(delta);
				if (length > anchor.rest) {
					position = position - LimitLength(delta * ((length - anchor.rest) / length * kLinkStiffness), 0.25f * _spacing);
				}
			}
			Contain(a_particles, a_colliders);
			for (std::size_t i = 0; i < count; ++i) {
				if (a_particles[i].canal < 0) {
					_predicted[i].z = std::max(_predicted[i].z, _floor[i]);
				}
			}
		}

		const float flow = std::min(1.0f, _parameters.stretch * a_seconds);
		for (std::size_t i = 0; i + 1 < count; ++i) {
			float& rest = a_particles[i].link;
			if (rest <= 0.0f) {
				continue;
			}
			const float length = Length(_predicted[i + 1] - _predicted[i]);
			if (length > rest) {
				rest += (length - rest) * flow;
			}
			if (rest > kLinkBreak * _spacing || length > kLinkSnap * rest) {
				rest = 0.0f;
			}
		}
		Collide(a_particles, a_colliders, false);
		Contain(a_particles, a_colliders);
		for (std::size_t i = 0; i < count; ++i) {
			if (a_particles[i].canal < 0) {
				_predicted[i].z = std::max(_predicted[i].z, _floor[i]);
			}
		}
		PassCanals(a_particles, a_colliders, a_seconds);

		const float follow = std::min(1.0f, kAnchorFlow * a_seconds);
		for (auto& anchor : a_anchors) {
			if (anchor.rest <= 0.0f || anchor.particle >= count) {
				anchor.rest = 0.0f;
				continue;
			}
			const Vec3  delta = _predicted[anchor.particle] - anchor.point;
			const float length = Length(delta);
			if (length > anchor.rest) {
				anchor.rest += (length - anchor.rest) * follow;
			}
			if (delta.z < 0.0f) {
				std::size_t load = 1;
				for (std::size_t k = anchor.particle; k > 0 && load < kMaxLoad && a_particles[k - 1].link > 0.0f; --k) {
					++load;
				}
				anchor.rest += kCreep * std::sqrt(static_cast<float>(load)) * a_seconds;
			}
			if (anchor.rest > kAnchorBreak * _spacing) {
				anchor.rest = 0.0f;
			}
		}

		for (std::size_t i = 0; i < count; ++i) {
			_velocity[i] = a_particles[i].asleep ? Vec3{} : LimitLength((_predicted[i] - a_particles[i].position) * (1.0f / a_seconds), kMaxSpeed);
		}
		FindNeighbors(a_particles, false);
		for (std::size_t i = 0; i < count; ++i) {
			_weights[i] = 0.0f;
			for (const auto j : _neighbors[i]) {
				_weights[i] += Weight(_predicted[i] - _predicted[j]);
			}
		}
		const float friction = std::exp(-kFloorFriction * a_seconds);
		for (std::size_t i = 0; i < count; ++i) {
			auto& particle = a_particles[i];
			if (particle.asleep) {
				particle.velocity = {};
				continue;
			}
			const auto  kind = Effective(particle.kind, particle.mix, particle.age, _parameters.liquefy);
			const float viscosity = 1.0f - std::exp(-_parameters.viscosity * kind.viscosity * a_seconds);
			Vec3        smoothing{};
			for (const auto j : _neighbors[i]) {
				const float shared = 1.0f + 0.5f * (_weights[i] + (a_particles[j].asleep ? _weights[i] : _weights[j]));
				smoothing += (_velocity[j] - _velocity[i]) * (Weight(_predicted[i] - _predicted[j]) / shared);
			}
			auto velocity = _velocity[i] + smoothing * viscosity;
			particle.touching = _contact[i] >= 0;
			if (particle.touching) {
				const Vec3  surface = Carrier(a_colliders, _contact[i], _along[i], _predicted[i]).value_or(Vec3{});
				const Vec3  relative = velocity - surface;
				const float normal = Dot(relative, _normal[i]);
				Vec3        sliding = relative - _normal[i] * normal;
				const float slip = Length(sliding);
				const float grip = _parameters.grip * kind.grip * a_seconds;
				// dragged along a moving shaft, a semen-and-secretion mix is whipped toward foam
				// (air is worked in only at her entrance and outside her: deeper in, it just coats him)
				const bool atEntrance = particle.canal < 0 || (static_cast<std::size_t>(particle.canal) < a_colliders.canals.size() &&
				                                                  a_colliders.canals[static_cast<std::size_t>(particle.canal)].previous < 0);
				if (atEntrance && static_cast<std::size_t>(_contact[i]) < a_colliders.capsules.size() &&
					a_colliders.capsules[static_cast<std::size_t>(_contact[i])].shaft) {
					particle.whip += Length(surface) * a_seconds;  // sheared between his moving shaft and her still wall
				}
				sliding = slip > grip ? sliding * ((slip - grip) / slip * std::exp(-_parameters.friction * kind.friction * a_seconds)) : Vec3{};
				velocity = surface + sliding + _normal[i] * std::clamp(normal, 0.0f, kSeparation);
				particle.carried = surface;
				particle.surface = _contact[i];
				particle.along = _along[i];
			} else {
				particle.surface = -1;
			}
			if (particle.canal >= 0 && static_cast<std::size_t>(particle.canal) < a_colliders.canals.size()) {
				const auto& canal = a_colliders.canals[static_cast<std::size_t>(particle.canal)];
				velocity += Normalize(canal.b - canal.a, Vec3{}) * (canal.pull * a_seconds);
				velocity = velocity * std::exp(-canal.drag * a_seconds);
			}
			if (_predicted[i].z <= _floor[i] + _spacing * 0.01f) {
				velocity.z = std::max(velocity.z, 0.0f);
				velocity.x *= friction;
				velocity.y *= friction;
			}
			particle.position = _predicted[i];
			particle.velocity = velocity;
			if (_anchored[i] || particle.touching || LengthSquared(velocity) > kSleepSpeed * kSleepSpeed) {
				particle.idle = 0.0f;
			} else if ((particle.idle += a_seconds) > kSleepTime) {
				particle.asleep = true;
				particle.velocity = {};
			}
		}
		Blend(a_particles, a_seconds);
	}

	// Semen and secretion that touch even out their mix (a cloudy blend between clear and white); a mix whipped along a
	// moving shaft long enough foams; everything ages (semen liquefies with age)
	void Solver::Blend(std::span<Particle> a_particles, float a_seconds)
	{
		const std::size_t  count = a_particles.size();
		std::vector<float> share(count, -1.0f);
		for (std::size_t i = 0; i < count; ++i) {
			if (Mixes(a_particles[i].kind)) {
				share[i] = SemenShare(a_particles[i].kind, a_particles[i].mix);
			}
		}
		const float rate = std::min(1.0f, kMixRate * a_seconds);
		for (std::size_t i = 0; i < count; ++i) {
			auto& particle = a_particles[i];
			particle.age += a_seconds;
			if (share[i] < 0.0f) {
				continue;
			}
			float sum = 0.0f, weight = 0.0f;
			for (const auto j : _neighbors[i]) {
				if (share[j] >= 0.0f) {
					const float w = Weight(_predicted[i] - _predicted[j]);
					sum += (share[j] - share[i]) * w;
					weight += w;
				}
			}
			if (weight > 0.0f && std::abs(sum) > 1e-6f * weight) {
				particle.mix = std::clamp(share[i] + rate * sum / weight, 0.0f, 1.0f);
			}
			if (particle.kind == static_cast<std::uint8_t>(Kind::kFoam)) {
				continue;
			}
			const float m = SemenShare(particle.kind, particle.mix);
			if (particle.whip > kFroth && m > 0.15f && m < 0.85f) {
				particle.kind = static_cast<std::uint8_t>(Kind::kFoam);
			} else if (particle.mix >= 0.0f) {
				particle.kind = static_cast<std::uint8_t>(m >= 0.5f ? Kind::kSemen : Kind::kSecretion);
			}
		}
	}
}
