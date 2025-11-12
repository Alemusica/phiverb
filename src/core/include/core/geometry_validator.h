#pragma once

#include "core/geo/triangle_vec.h"
#include "core/cl/triangle.h"
#include "core/scene_data.h"

#include <vector>
#include <string>
#include <optional>

namespace wayverb {
namespace core {

/// Geometry validation and repair utilities.
class geometry_validator final {
public:
    /// Report of geometry validation results.
    struct validation_report final {
        bool is_valid{true};
        size_t degenerate_triangles{0};
        size_t self_intersections{0};
        size_t inconsistent_normals{0};
        size_t non_manifold_edges{0};
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        
        void add_error(const std::string& msg) {
            errors.push_back(msg);
            is_valid = false;
        }
        
        void add_warning(const std::string& msg) {
            warnings.push_back(msg);
        }
    };
    
    static constexpr float epsilon() { return 1e-6f; }
    static constexpr size_t max_triangles() { return 100000; }
    
    /// Validate a mesh by checking for degenerate triangles and other issues.
    /// This is a basic validation that checks triangle areas.
    template <typename Vertex, typename Surface>
    static validation_report validate(
            const generic_scene_data<Vertex, Surface>& scene) {
        validation_report report;
        
        const auto& triangles = scene.get_triangles();
        const auto& vertices = scene.get_vertices();
        
        // Check for too many triangles
        if (triangles.size() > max_triangles()) {
            report.add_warning("Mesh has " + std::to_string(triangles.size()) +
                             " triangles, which may cause performance issues.");
        }
        
        // Check each triangle for degeneracy
        for (size_t i = 0; i < triangles.size(); ++i) {
            const auto& tri = triangles[i];
            const auto tri_vec = geo::get_triangle_vec3(tri, vertices.data());
            
            if (is_degenerate(tri_vec)) {
                ++report.degenerate_triangles;
                if (report.degenerate_triangles <= 10) {  // Limit error messages
                    report.add_error("Triangle " + std::to_string(i) +
                                   " is degenerate (zero or near-zero area).");
                }
            }
        }
        
        if (report.degenerate_triangles > 10) {
            report.add_error("Found " + std::to_string(report.degenerate_triangles) +
                           " total degenerate triangles.");
        }
        
        return report;
    }
    
private:
    /// Check if a triangle is degenerate (has near-zero area).
    static bool is_degenerate(const geo::triangle_vec3& tri) {
        return triangle_area(tri) < epsilon();
    }
    
    /// Calculate the area of a triangle.
    static float triangle_area(const geo::triangle_vec3& tri) {
        return geo::area(tri);
    }
};

}  // namespace core
}  // namespace wayverb
