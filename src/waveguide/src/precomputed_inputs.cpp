#include "waveguide/precomputed_inputs.h"

#include "cereal/external/rapidjson/document.h"
#include "cereal/external/rapidjson/istreamwrapper.h"

#include <cstring>
#include <filesystem>
#include <fstream>

namespace wayverb {
namespace waveguide {

namespace {

namespace fs = std::filesystem;

std::vector<char> read_file_bytes(const fs::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("Failed to open binary file: " + path.string());
    }
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    input.seekg(0, std::ios::beg);

    std::vector<char> bytes(static_cast<size_t>(size));
    input.read(bytes.data(), size);
    return bytes;
}

template <typename T>
std::vector<T> read_typed_array(const fs::path& path, size_t count) {
    auto bytes = read_file_bytes(path);
    if (bytes.size() != count * sizeof(T)) {
        throw std::runtime_error("Unexpected binary size for " + path.string());
    }
    std::vector<T> data(count);
    std::memcpy(data.data(), bytes.data(), bytes.size());
    return data;
}

rapidjson::Document load_json(const fs::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Failed to open JSON: " + path.string());
    }
    rapidjson::IStreamWrapper wrapper(input);
    rapidjson::Document doc;
    doc.ParseStream(wrapper);
    if (doc.HasParseError()) {
        throw std::runtime_error("Invalid JSON: " + path.string());
    }
    return doc;
}

std::shared_ptr<sdf_volume> load_sdf_volume(const fs::path& meta_path) {
    if (!fs::exists(meta_path)) {
        return {};
    }

    auto doc = load_json(meta_path);
    const auto& files = doc["files"];
    const fs::path base_dir = meta_path.parent_path();

    auto volume = std::make_shared<sdf_volume>();
    const auto& origin = doc["origin"];
    volume->origin = glm::vec3(origin[0].GetFloat(),
                               origin[1].GetFloat(),
                               origin[2].GetFloat());

    const auto& dims = doc["dims"];
    volume->dims = glm::ivec3(dims[0].GetInt(), dims[1].GetInt(), dims[2].GetInt());
    volume->voxel_pitch = doc["voxel_pitch"].GetFloat();

    const size_t total = static_cast<size_t>(volume->dims.x) * volume->dims.y *
                         volume->dims.z;

    const fs::path sdf_bin = base_dir / files["sdf"].GetString();
    const fs::path normals_bin = base_dir / files["normals"].GetString();
    const fs::path labels_bin = base_dir / files["labels"].GetString();

    volume->sdf = read_typed_array<float>(sdf_bin, total);

    auto normals_raw = read_typed_array<float>(normals_bin, total * 3);
    volume->normals.resize(total);
    for (size_t i = 0; i < total; ++i) {
        volume->normals[i] = glm::vec3(normals_raw[i * 3 + 0],
                                       normals_raw[i * 3 + 1],
                                       normals_raw[i * 3 + 2]);
    }

    volume->labels = read_typed_array<int16_t>(labels_bin, total);

    const auto& labels = doc["labels"];
    for (auto it = labels.Begin(); it != labels.End(); ++it) {
        volume->label_names.emplace_back(it->GetString());
    }

    return volume;
}

std::unordered_map<std::string, dif_material> load_dif_materials(
        const fs::path& dif_path) {
    if (!fs::exists(dif_path)) {
        return {};
    }

    auto doc = load_json(dif_path);
    const auto& materials = doc["materials"];

    for (auto itr = materials.MemberBegin(); itr != materials.MemberEnd();
         ++itr) {
        dif_material mat;
        mat.name = itr->name.GetString();
        const auto& alpha = itr->value["alpha"];
        for (auto it = alpha.Begin(); it != alpha.End(); ++it) {
            mat.alpha.push_back(it->GetDouble());
        }
        if (itr->value.HasMember("scattering")) {
            mat.scattering = itr->value["scattering"].GetDouble();
        }
        result.emplace(mat.name, std::move(mat));
    }
    return result;
}

}  // namespace

std::shared_ptr<precomputed_inputs> load_precomputed_inputs(
        const std::string& scene_path) {
    const fs::path scene(scene_path);
    fs::path stem = scene;
    stem.replace_extension("");

    auto sdf_meta = stem;
    const fs::path sdf_path = sdf_meta.replace_extension(".sdf.json");
    auto dif_meta = stem;
    const fs::path dif_path = dif_meta.replace_extension(".dif.json");

    auto sdf = load_sdf_volume(sdf_path);
    auto dif = load_dif_materials(dif_path);

    if (!sdf && dif.empty()) {
        return {};
    }

    auto inputs = std::make_shared<precomputed_inputs>();
    inputs->sdf = std::move(sdf);
    inputs->dif_materials = std::move(dif);
    return inputs;
}

void apply_precomputed_inputs(voxels_and_mesh& voxels_and_mesh,
                              const precomputed_inputs& inputs,
                              double speed_of_sound) {
    (void)speed_of_sound;
    if (!inputs.sdf) {
        return;
    }

    auto state = std::make_shared<precomputed_boundary_state>();
    state->volume = inputs.sdf;
    state->default_coefficient = 0;
    for (size_t i = 0; i < inputs.surface_names.size(); ++i) {
        state->label_to_coefficient[inputs.surface_names[i]] =
                static_cast<cl_uint>(i);
    }
    voxels_and_mesh.precomputed = std::move(state);
}

}  // namespace waveguide
}  // namespace wayverb
