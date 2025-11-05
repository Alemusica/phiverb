#include "core/geometry_analysis.h"
#include "core/cl/triangle.h"

#include <unordered_map>
#include <vector>
#include <cmath>

namespace wayverb {
namespace core {

namespace {
struct EdgeKey {
    cl_uint a;
    cl_uint b;
    bool operator==(const EdgeKey& o) const { return a == o.a && b == o.b; }
};
struct EdgeHash {
    std::size_t operator()(const EdgeKey& k) const noexcept {
        return (static_cast<std::size_t>(k.a) << 32) ^ k.b;
    }
};

inline float tri_area2(const cl_float3& v0,
                       const cl_float3& v1,
                       const cl_float3& v2) {
    const float x1 = v1.s[0] - v0.s[0];
    const float y1 = v1.s[1] - v0.s[1];
    const float z1 = v1.s[2] - v0.s[2];
    const float x2 = v2.s[0] - v0.s[0];
    const float y2 = v2.s[1] - v0.s[1];
    const float z2 = v2.s[2] - v0.s[2];
    // squared norm of cross product
    const float cx = y1 * z2 - z1 * y2;
    const float cy = z1 * x2 - x1 * z2;
    const float cz = x1 * y2 - y1 * x2;
    return cx * cx + cy * cy + cz * cz;
}

inline cl_uint quant_key(float v, float eps) {
    return static_cast<cl_uint>(std::llround(v / eps));
}
struct QKey3 {
    cl_uint x, y, z;
    bool operator==(const QKey3& o) const { return x == o.x && y == o.y && z == o.z; }
};
struct QKeyHash {
    std::size_t operator()(const QKey3& k) const noexcept {
        return (static_cast<std::size_t>(k.x) * 73856093) ^
               (static_cast<std::size_t>(k.y) * 19349663) ^
               (static_cast<std::size_t>(k.z) * 83492791);
    }
};
}  // namespace

template <typename Vertex, typename Surface>
geometry_report analyze_geometry(const generic_scene_data<Vertex, Surface>& scene,
                                 float weld_epsilon) {
    geometry_report rep{};
    const auto& tris = scene.get_triangles();
    const auto& verts = scene.get_vertices();
    rep.vertices = verts.size();
    rep.triangles = tris.size();

    // zero-area
    std::size_t zero = 0;
    for (const auto& t : tris) {
        const auto& v0 = reinterpret_cast<const cl_float3&>(verts[t.v0]);
        const auto& v1 = reinterpret_cast<const cl_float3&>(verts[t.v1]);
        const auto& v2 = reinterpret_cast<const cl_float3&>(verts[t.v2]);
        if (tri_area2(v0, v1, v2) < 1.0e-20f) ++zero;
    }
    rep.zero_area = zero;

    // edge manifoldness
    std::unordered_map<EdgeKey, int, EdgeHash> edge_count;
    edge_count.reserve(tris.size() * 3);
    for (const auto& t : tris) {
        cl_uint e[3][2] = {{t.v0, t.v1}, {t.v1, t.v2}, {t.v2, t.v0}};
        for (auto& p : e) {
            EdgeKey k{std::min(p[0], p[1]), std::max(p[0], p[1])};
            edge_count[k] += 1;
        }
    }
    for (const auto& kv : edge_count) {
        if (kv.second == 1) ++rep.boundary_edges;
        else if (kv.second > 2) ++rep.non_manifold_edges;
    }
    rep.watertight = (rep.boundary_edges == 0) && (rep.non_manifold_edges == 0);

    // duplicates (approx via quantization)
    std::unordered_map<QKey3, cl_uint, QKeyHash> qmap;
    qmap.reserve(verts.size());
    std::size_t dups = 0;
    for (cl_uint i = 0; i < verts.size(); ++i) {
        const auto& v = reinterpret_cast<const cl_float3&>(verts[i]);
        QKey3 k{quant_key(v.s[0], weld_epsilon),
                quant_key(v.s[1], weld_epsilon),
                quant_key(v.s[2], weld_epsilon)};
        auto it = qmap.find(k);
        if (it == qmap.end()) qmap.emplace(k, i);
        else ++dups;
    }
    rep.duplicate_vertices = dups;
    return rep;
}

template <typename Vertex, typename Surface>
generic_scene_data<Vertex, Surface> sanitize_geometry(
        const generic_scene_data<Vertex, Surface>& scene,
        float weld_epsilon) {
    const auto& tris = scene.get_triangles();
    const auto& verts = scene.get_vertices();
    const auto& surfs = scene.get_surfaces();

    // weld vertices
    std::unordered_map<QKey3, cl_uint, QKeyHash> qmap;
    qmap.reserve(verts.size());
    util::aligned::vector<Vertex> new_verts;
    new_verts.reserve(verts.size());
    std::vector<cl_uint> remap(verts.size());
    for (cl_uint i = 0; i < verts.size(); ++i) {
        const auto& vr = reinterpret_cast<const cl_float3&>(verts[i]);
        QKey3 k{quant_key(vr.s[0], weld_epsilon),
                quant_key(vr.s[1], weld_epsilon),
                quant_key(vr.s[2], weld_epsilon)};
        auto it = qmap.find(k);
        if (it == qmap.end()) {
            const cl_uint new_index = static_cast<cl_uint>(new_verts.size());
            qmap.emplace(k, new_index);
            new_verts.emplace_back(verts[i]);
            remap[i] = new_index;
        } else {
            remap[i] = it->second;
        }
    }

    // rebuild triangles and drop degenerates/zero-area
    util::aligned::vector<triangle> new_tris;
    new_tris.reserve(tris.size());
    for (const auto& t : tris) {
        triangle nt{t.surface, remap[t.v0], remap[t.v1], remap[t.v2]};
        if (nt.v0 == nt.v1 || nt.v1 == nt.v2 || nt.v2 == nt.v0) continue;
        const auto& v0 = reinterpret_cast<const cl_float3&>(new_verts[nt.v0]);
        const auto& v1 = reinterpret_cast<const cl_float3&>(new_verts[nt.v1]);
        const auto& v2 = reinterpret_cast<const cl_float3&>(new_verts[nt.v2]);
        if (tri_area2(v0, v1, v2) < 1.0e-20f) continue;
        new_tris.emplace_back(nt);
    }

    util::aligned::vector<Surface> new_surfs = surfs;
    return make_scene_data(std::move(new_tris), std::move(new_verts), std::move(new_surfs));
}

// Explicit instantiations for the common types used by the app.
template geometry_report analyze_geometry<cl_float3, std::string>(
        const generic_scene_data<cl_float3, std::string>&, float);
template generic_scene_data<cl_float3, std::string> sanitize_geometry(
        const generic_scene_data<cl_float3, std::string>&, float);

}  // namespace core
}  // namespace wayverb

