#include "waveguide/boundary_layout.h"

#include "waveguide/config.h"
#include "waveguide/mesh_descriptor.h"

#include "core/conversions.h"
#include "core/indexing.h"
#include "core/geo/geometric.h"
#include "core/spatial_division/voxel_collection.h"

#include "glm/common.hpp"
#include "glm/geometric.hpp"
#include "glm/gtx/component_wise.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <unordered_map>

namespace wayverb {
namespace waveguide {

namespace {

constexpr uint32_t kGuardMask = 0xA5A5A5A5u;
constexpr std::array<boundary_type, 6> kFaceBits{
        id_nx, id_px, id_ny, id_py, id_nz, id_pz};
constexpr std::array<glm::ivec3, 6> kFaceOffsets{
        glm::ivec3{-1, 0, 0},
        glm::ivec3{1, 0, 0},
        glm::ivec3{0, -1, 0},
        glm::ivec3{0, 1, 0},
        glm::ivec3{0, 0, -1},
        glm::ivec3{0, 0, 1}};

struct boundary_entry {
    size_t node_index{};
    glm::ivec3 locator{};
    uint64_t morton{};
    boundary_type type{};
};

inline bool is_boundary_node(boundary_type bt) {
    return is_boundary<1>(bt) || is_boundary<2>(bt) || is_boundary<3>(bt);
}

uint64_t encode_morton3D(uint32_t x, uint32_t y, uint32_t z) {
    auto part = [](uint32_t v) {
        uint64_t x = v;
        x = (x | (x << 32)) & 0x1F00000000FFFF;
        x = (x | (x << 16)) & 0x1F0000FF0000FF;
        x = (x | (x << 8)) & 0x100F00F00F00F00F;
        x = (x | (x << 4)) & 0x10C30C30C30C30C3;
        x = (x | (x << 2)) & 0x1249249249249249;
        return x;
    };
    return (part(z) << 2) | (part(y) << 1) | part(x);
}

struct signed_distance_solver final {
    signed_distance_solver(
            const mesh_descriptor& descriptor,
            const util::aligned::vector<condensed_node>& nodes,
            const core::voxelised_scene_data<
                    cl_float3,
                    core::surface<core::simulation_bands>>& voxelised)
            : descriptor_{descriptor}
            , nodes_{nodes}
            , voxelised_{voxelised}
            , cache_(nodes.size(),
                     std::numeric_limits<float>::quiet_NaN())
            , voxel_dims_(core::voxel_dimensions(voxelised_.get_voxels()))
            , scene_min_(voxelised_.get_voxels().get_aabb().get_min())
            , scene_max_(voxelised_.get_voxels().get_aabb().get_max())
            , diag_(glm::length(scene_max_ - scene_min_))
            , side_(static_cast<int>(voxelised_.get_voxels().get_side())) {}

    float operator()(size_t node_index) {
        auto& cached = cache_[node_index];
        if (!std::isnan(cached)) {
            return cached;
        }

        const auto position =
                compute_position(descriptor_, static_cast<uint32_t>(node_index));
        const auto unsigned_distance =
                compute_unsigned_distance(core::to_vec3{}(position));
        const bool is_inside_node =
                (nodes_[node_index].boundary_type & id_inside) != 0;
        cached = is_inside_node ? -unsigned_distance : unsigned_distance;
        return cached;
    }

private:
    float compute_unsigned_distance(const glm::vec3& point) const {
        float best = std::numeric_limits<float>::infinity();
        bool found = false;

        const auto base_idx = to_voxel_index(point);

        const auto& voxels = voxelised_.get_voxels();
        const auto& triangles =
                voxelised_.get_scene_data().get_triangles();
        const auto& vertices =
                voxelised_.get_scene_data().get_vertices();

        for (float radius = glm::compMax(voxel_dims_); radius <= diag_;
             radius *= 1.5f) {
            const glm::ivec3 span = glm::max(
                    glm::ivec3(1),
                    glm::ivec3(glm::ceil(glm::vec3(radius) / voxel_dims_)));
            const glm::ivec3 min_idx =
                    glm::max(glm::ivec3(0), base_idx - span);
            const glm::ivec3 max_idx =
                    glm::min(glm::ivec3(side_ - 1), base_idx + span);

            for (int x = min_idx.x; x <= max_idx.x; ++x) {
                for (int y = min_idx.y; y <= max_idx.y; ++y) {
                    for (int z = min_idx.z; z <= max_idx.z; ++z) {
                        const auto& voxel = voxels.get_voxel(
                                core::indexing::index_t<3>{static_cast<unsigned>(x),
                                                           static_cast<unsigned>(y),
                                                           static_cast<unsigned>(z)});
                        for (auto tri_index : voxel) {
                            const auto tri_vec =
                                    core::geo::get_triangle_vec3(
                                            triangles[tri_index],
                                            vertices.data());
                            const float dist_sq =
                                    core::geo::point_triangle_distance_squared(
                                            tri_vec, point);
                            if (dist_sq < best) {
                                best = dist_sq;
                                found = true;
                            }
                        }
                    }
                }
            }

            if (found) {
                break;
            }
        }

        if (!found) {
            return 0.0f;
        }
        return std::sqrt(best);
    }

    glm::ivec3 to_voxel_index(const glm::vec3& point) const {
        const glm::vec3 rel = (point - scene_min_) / voxel_dims_;
        glm::ivec3 idx = glm::ivec3(glm::floor(rel));
        idx = glm::clamp(idx, glm::ivec3(0), glm::ivec3(side_ - 1));
        return idx;
    }

    const mesh_descriptor& descriptor_;
    const util::aligned::vector<condensed_node>& nodes_;
    const core::voxelised_scene_data<cl_float3,
                                     core::surface<core::simulation_bands>>&
            voxelised_;
    mutable util::aligned::vector<float> cache_;
    glm::vec3 voxel_dims_;
    glm::vec3 scene_min_;
    glm::vec3 scene_max_;
    float diag_;
    int side_;
};

coefficients_canonical identity_coefficients() {
    coefficients_canonical coeff{};
    coeff.b[0] = static_cast<filt_real>(1);
    coeff.a[0] = static_cast<filt_real>(1);
    return coeff;
}

std::array<uint32_t, 6> gather_face_coeff_indices(
        const condensed_node& node,
        const boundary_index_data& index_data) {
    std::array<uint32_t, 6> indices{};
    indices.fill(std::numeric_limits<uint32_t>::max());

    size_t cursor = 0;
    const auto assign = [&](auto arr) {
        for (int face = 0; face < static_cast<int>(kFaceBits.size()); ++face) {
            if (node.boundary_type & kFaceBits[face]) {
                indices[face] = arr->array[cursor++];
            }
        }
    };

    if (is_boundary<1>(node.boundary_type)) {
        assign(&index_data.b1[node.boundary_index]);
    } else if (is_boundary<2>(node.boundary_type)) {
        assign(&index_data.b2[node.boundary_index]);
    } else if (is_boundary<3>(node.boundary_type)) {
        assign(&index_data.b3[node.boundary_index]);
    }

    return indices;
}

glm::vec3 compute_gradient_for_node(
        size_t node_index,
        const mesh_descriptor& descriptor,
        const util::aligned::vector<condensed_node>& nodes,
        signed_distance_solver& distance_solver) {
    const auto dims = core::to_ivec3{}(descriptor.dimensions);
    const auto locator =
            compute_locator(descriptor, static_cast<uint32_t>(node_index));
    const float spacing = descriptor.spacing;

    const auto sample = [&](const glm::ivec3& loc) -> std::optional<float> {
        if (glm::any(glm::lessThan(loc, glm::ivec3(0))) ||
            glm::any(glm::greaterThanEqual(loc, dims))) {
            return std::nullopt;
        }
        const auto idx = compute_index(descriptor, loc);
        return distance_solver(static_cast<size_t>(idx));
    };

    const float center = distance_solver(node_index);
    glm::vec3 gradient{0.0f};

    for (int axis = 0; axis < 3; ++axis) {
        const glm::ivec3 unit =
                glm::ivec3(axis == 0, axis == 1, axis == 2);
        const auto plus = sample(locator + unit);
        const auto minus = sample(locator - unit);

        float component = 0.0f;
        if (plus && minus) {
            component = (*plus - *minus) / (2.0f * spacing);
        } else if (plus) {
            component = (*plus - center) / spacing;
        } else if (minus) {
            component = (center - *minus) / spacing;
        }

        gradient[axis] = component;
    }

    const float len = glm::length(gradient);
    if (len < 1e-6f) {
        return glm::vec3{0.0f};
    }
    return gradient / len;
}

}  // namespace

boundary_layout build_boundary_layout(
        const mesh_descriptor& descriptor,
        const util::aligned::vector<condensed_node>& nodes,
        const boundary_index_data& index_data,
        const util::aligned::vector<coefficients_canonical>& surface_coeffs,
        const core::voxelised_scene_data<cl_float3,
                                         core::surface<core::simulation_bands>>&
                voxelised) {
    boundary_layout layout;

    util::aligned::vector<boundary_entry> entries;
    entries.reserve(nodes.size());

    const auto dims = core::to_ivec3{}(descriptor.dimensions);

    for (size_t idx = 0; idx < nodes.size(); ++idx) {
        const auto type = nodes[idx].boundary_type;
        if (!is_boundary_node(static_cast<boundary_type>(type))) {
            continue;
        }
        const auto locator =
                compute_locator(descriptor, static_cast<uint32_t>(idx));
        const uint64_t morton = encode_morton3D(
                static_cast<uint32_t>(locator.x),
                static_cast<uint32_t>(locator.y),
                static_cast<uint32_t>(locator.z));
        entries.push_back(boundary_entry{
                idx, locator, morton, static_cast<boundary_type>(type)});
    }

    std::sort(entries.begin(),
              entries.end(),
              [](const boundary_entry& a, const boundary_entry& b) {
                  if (a.morton != b.morton) {
                      return a.morton < b.morton;
                  }
                  return a.node_index < b.node_index;
              });

    layout.headers.resize(entries.size());
    layout.sdf_distance.resize(entries.size());
    layout.sdf_normal.resize(entries.size());
    layout.coeff_block_offsets.resize(entries.size());
    layout.filter_memories.resize(entries.size() * kFaceBits.size());
    layout.node_indices.resize(entries.size());
    layout.node_lookup.assign(nodes.size(),
                              std::numeric_limits<uint32_t>::max());

    signed_distance_solver distance_solver{descriptor, nodes, voxelised};

    auto coeff_identity = identity_coefficients();

    for (size_t entry_idx = 0; entry_idx < entries.size(); ++entry_idx) {
        const auto& entry = entries[entry_idx];
        layout.node_indices[entry_idx] =
                static_cast<uint32_t>(entry.node_index);
        layout.node_lookup[entry.node_index] =
                static_cast<uint32_t>(entry_idx);

        auto& header = layout.headers[entry_idx];
        header.guard = static_cast<uint32_t>(entry.node_index) ^ kGuardMask;

        const auto face_indices =
                gather_face_coeff_indices(nodes[entry.node_index], index_data);
        uint32_t first_material =
                face_indices[0] == std::numeric_limits<uint32_t>::max()
                        ? 0u
                        : face_indices[0];
        header.material_index =
                static_cast<uint16_t>(first_material & 0xFFFFu);

        uint8_t face_mask = 0;
        for (size_t face = 0; face < kFaceBits.size(); ++face) {
            if (entry.type & kFaceBits[face]) {
                face_mask |= static_cast<uint8_t>(1u << face);
            }
        }

        const uint32_t block_offset =
                static_cast<uint32_t>(layout.coeff_blocks.size());
        layout.coeff_block_offsets[entry_idx] = block_offset;

        for (size_t face = 0; face < kFaceBits.size(); ++face) {
            const auto coeff_idx = face_indices[face];
            if (coeff_idx == std::numeric_limits<uint32_t>::max()) {
                layout.coeff_blocks.emplace_back(coeff_identity);
            } else {
                layout.coeff_blocks.emplace_back(surface_coeffs[coeff_idx]);
            }
        }

        const uint32_t block_id = block_offset / kFaceBits.size();
        const uint16_t dif_payload =
                static_cast<uint16_t>((block_id & 0x3FFu) << 6);
        header.dif = static_cast<uint16_t>((face_mask & 0x3Fu) | dif_payload);

        layout.sdf_distance[entry_idx] =
                distance_solver(entry.node_index);
        layout.sdf_normal[entry_idx] =
                compute_gradient_for_node(entry.node_index,
                                          descriptor,
                                          nodes,
                                          distance_solver);
    }

    return layout;
}

}  // namespace waveguide
}  // namespace wayverb
