#include "raytracer/program.h"

#include "raytracer/cl/brdf.h"
#include "raytracer/cl/structs.h"

#include "core/cl/geometry.h"
#include "core/cl/geometry_structs.h"
#include "core/cl/scene_structs.h"
#include "core/cl/voxel.h"
#include "core/cl/voxel_structs.h"

namespace wayverb {
namespace raytracer {

constexpr auto source=R"(
#define PRINT_INT3(VAR) printf("%v3hld\n", (VAR));
#define PRINT_FLOAT3(VAR) printf("%2.2v3hlf\n", (VAR));

#define PRINT_ULONG(VAR) printf("%ld\n", (VAR));
#define PRINT_FLOAT(VAR) printf("%2.2f\n", (VAR));

float wrap_unit(float x) { return x - floor(x); }
float clamp01(float x) { return fmin(fmax(x, 0.0f), 1.0f); }

float3 build_tangent(float3 n) {
    const float3 up = fabs(n.z) < 0.999f ? (float3)(0.0f, 0.0f, 1.0f)
                                         : (float3)(0.0f, 1.0f, 0.0f);
    return normalize(cross(up, n));
}

float3 cosine_weighted_direction(float3 normal, float u1, float u2) {
    const float phi = 2.0f * (float)M_PI * wrap_unit(u1);
    const float r = sqrt(clamp01(u2));
    const float x = r * cos(phi);
    const float y = r * sin(phi);
    const float z = sqrt(fmax(0.0f, 1.0f - clamp01(u2)));
    const float3 tangent = build_tangent(normal);
    const float3 bitangent = cross(normal, tangent);
    return normalize(tangent * x + bitangent * y + normal * z);
}

float3 mirror_point(float3 p, triangle_verts t);
float3 mirror_point(float3 p, triangle_verts t) {
    const float3 n = triangle_verts_normal(t);
    return p - n * dot(n, p - t.v0) * 2;
}

triangle_verts mirror_verts(triangle_verts in, triangle_verts t);
triangle_verts mirror_verts(triangle_verts in, triangle_verts t) {
    return (triangle_verts){mirror_point(in.v0, t),
                            mirror_point(in.v1, t),
                            mirror_point(in.v2, t)};
}

void reflect_and_add_triangle_to_history(triangle_verts current,
                                         global triangle_verts* history,
                                         size_t iteration);
void reflect_and_add_triangle_to_history(triangle_verts current,
                                         global triangle_verts* history,
                                         size_t iteration) {
    //  repeatedly reflect the intersected triangle in
    //  previously-intersected triangles
    for (size_t k = 0; k != iteration; ++k) {
        current = mirror_verts(current, history[k]);
    }

    //  add the reflected triangle to the prev_primitives array
    history[iteration] = current;
}

kernel void init_reflections(global reflection* reflections) {
    const size_t thread = get_global_id(0);
    reflections[thread] = (reflection){(float3)(0),
                                       ~(uint)0,
                                       0.0f,
                                       (char)true,
                                       (char)0,
                                       (char)0,
                                       (char)0,
                                       0.0f,
                                       0.0f};
}

kernel void reflections(global ray* rays,  //  ray

                        float3 receiver,  //  receiver

                        const global uint* voxel_index,  //  voxel
                        aabb global_aabb,
                        uint side,

                        const global triangle* triangles,  //  scene
                        const global float3* vertices,
                        const global surface* surfaces,

                        const global float* rng,  //  random numbers

                        global reflection* reflections) {  //  output
    //  get thread index
    const size_t thread = get_global_id(0);

    const bool keep_going = reflections[thread].keep_going;
    const uint previous_triangle = reflections[thread].triangle;

    //  zero out result reflection
    reflections[thread] = (reflection){};

    //  if this thread should stop, then stop
    if (!keep_going) {
        return;
    }

    //  find the ray to intersect
    const ray this_ray = rays[thread];

    //  find the intersection between scene geometry and this ray
    const intersection closest_intersection =
            voxel_traversal(this_ray,
                            voxel_index,
                            global_aabb,
                            side,
                            triangles,
                            vertices,
                            previous_triangle);

    //  didn't find an intersection, should halt this thread
    if (!closest_intersection.inter.t) {
        return;
    }

    //  find where the ray intersects with the scene geometry
    const float3 intersection_pt =
            this_ray.position +
            this_ray.direction * closest_intersection.inter.t;

    //  get the normal at the intersection
    const triangle closest_triangle = triangles[closest_intersection.index];
    float3 tnorm = triangle_normal(closest_triangle, vertices);

    //  calculate the new specular direction from this point
    const float3 specular = reflect(tnorm, this_ray.direction);

    //  make sure the normal faces the right direction
    tnorm *= signbit(dot(tnorm, specular));

    //  see whether the receiver is visible from this point
    const bool is_intersection =
            voxel_point_intersection(intersection_pt,
                                     receiver,
                                     voxel_index,
                                     global_aabb,
                                     side,
                                     triangles,
                                     vertices,
                                         closest_intersection.index);

    //  determine scattering behaviour using BRDF sampling
    const float u_component = rng[3 * thread + 0];
    const float u1 = rng[3 * thread + 1];
    const float u2 = rng[3 * thread + 2];
    const surface s = surfaces[closest_triangle.surface];
    const float scatter_prob = clamp(mean(s.scattering), 0.0f, 1.0f);

    float3 scattering = specular;
    float sample_pdf = 1.0f;
    float cos_theta = fabs(dot(tnorm, specular));
    const bool choose_diffuse = (scatter_prob > 0.0f && u_component < scatter_prob);

    if (choose_diffuse) {
        scattering = lambert_sample(tnorm, u1, u2, &cos_theta);
        sample_pdf = fmax(lambert_pdf(cos_theta), 1e-6f);
    }

    //  now we can populate the output
    reflections[thread] = (reflection){intersection_pt,
                                       closest_intersection.index,
                                       scatter_prob,
                                       true,
                                       is_intersection,
                                       (char)choose_diffuse,
                                       (char)0,
                                       sample_pdf,
                                       cos_theta};
=======
                                       choose_diffuse,
                                       0,
                                       sample_pdf,
                                       cos_theta};
>>>>>>> origin/rt/brdf-opencl-helpers

    //  find the next ray to trace
    rays[thread] = (ray){intersection_pt, normalize(outgoing_dir)};
}

)";

program::program(const core::compute_context& cc)
        : program_wrapper_{
                  cc,
                  std::vector<std::string>{
                          core::cl_representation_v<core::bands_type>,
                          core::cl_representation_v<
                                  core::surface<core::simulation_bands>>,
                          core::cl_representation_v<core::triangle>,
                          core::cl_representation_v<core::triangle_verts>,
                          core::cl_representation_v<core::aabb>,
                          core::cl_representation_v<core::ray>,
                          core::cl_representation_v<core::triangle_inter>,
                          core::cl_representation_v<core::intersection>,
                          core::cl_representation_v<reflection>,
                          core::cl_representation_v<impulse<8>>,
                          core::cl_sources::geometry,
                          core::cl_sources::voxel,
                          ::cl_sources::brdf,
                          source}} {}

}  // namespace raytracer
}  // namespace wayverb
