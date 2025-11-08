#include "core/scene_data_loader.h"
#include "core/geometry_analysis.h"
#include "core/conversions.h"

#include <fstream>
#include <iostream>
#include <string>

using namespace wayverb;

namespace {

std::string usage() { return "Usage: sanitize_mesh <input.obj> <output.obj> [weld_eps]"; }

template <typename Vertex, typename Surface>
void write_obj(const core::generic_scene_data<Vertex, Surface>& scene,
               const std::string& out) {
    std::ofstream os(out);
    if (!os) throw std::runtime_error{"Failed to open output file"};
    // write vertices
    for (const auto& v : scene.get_vertices()) {
        const auto p = core::to_vec3{}(v);
        os << "v " << p.x << ' ' << p.y << ' ' << p.z << '\n';
    }
    // faces are 1-based in OBJ
    for (const auto& t : scene.get_triangles()) {
        os << "f " << (t.v0 + 1) << ' ' << (t.v1 + 1) << ' ' << (t.v2 + 1)
           << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 3) {
            std::cerr << usage() << "\n";
            return EXIT_FAILURE;
        }
        const std::string in = argv[1];
        const std::string out = argv[2];
        const float eps = argc >= 4 ? static_cast<float>(std::atof(argv[3]))
                                    : 1.0e-6f;

        core::scene_data_loader loader{in};
        const auto data_opt = loader.get_scene_data();
        if (!data_opt) throw std::runtime_error{"Failed to load scene"};
        const auto& data = *data_opt;

        auto rep0 = core::analyze_geometry(data, eps);
        std::cout << "before: watertight=" << rep0.watertight
                  << " boundary_edges=" << rep0.boundary_edges
                  << " non_manifold_edges=" << rep0.non_manifold_edges
                  << " zero_area=" << rep0.zero_area
                  << " duplicate_vertices=" << rep0.duplicate_vertices
                  << "\n";

        auto san = core::sanitize_geometry(data, eps);

        auto rep1 = core::analyze_geometry(san, eps);
        std::cout << "after : watertight=" << rep1.watertight
                  << " boundary_edges=" << rep1.boundary_edges
                  << " non_manifold_edges=" << rep1.non_manifold_edges
                  << " zero_area=" << rep1.zero_area
                  << " duplicate_vertices=" << rep1.duplicate_vertices
                  << "\n";

        write_obj(san, out);
        std::cout << "wrote sanitized OBJ: " << out << "\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "sanitize_mesh error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}

