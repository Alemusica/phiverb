#pragma once

#include "glm/glm.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace wayverb {
namespace waveguide {

struct voxels_and_mesh;

struct sdf_volume final {
    glm::vec3 origin{};
    glm::ivec3 dims{};
    float voxel_pitch{};
    std::vector<float> sdf;
    std::vector<glm::vec3> normals;
    std::vector<int16_t> labels;
    std::vector<std::string> label_names;

    size_t total_voxels() const {
        return static_cast<size_t>(dims.x) * dims.y * dims.z;
    }

    int label_at(size_t idx) const {
        if (idx >= labels.size()) {
            return -1;
        }
        return labels[idx];
    }
};

struct dif_material final {
    std::string name;
    std::vector<double> alpha;
    double scattering = 0.0;
};

struct precomputed_boundary_state final {
    std::shared_ptr<sdf_volume> volume;
    std::unordered_map<std::string, cl_uint> label_to_coefficient;
    cl_uint default_coefficient = 0;
};

struct precomputed_inputs final {
    std::shared_ptr<sdf_volume> sdf;
    std::unordered_map<std::string, dif_material> dif_materials;
    std::vector<std::string> surface_names;
};

std::shared_ptr<precomputed_inputs> load_precomputed_inputs(
        const std::string& scene_path);

void apply_precomputed_inputs(voxels_and_mesh& voxels_and_mesh,
                              const precomputed_inputs& inputs,
                              double speed_of_sound);

}  // namespace waveguide
}  // namespace wayverb
