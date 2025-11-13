#include "raytracer/stochastic/program.h"
#include "raytracer/cl/brdf.h"
#include "raytracer/cl/structs.h"

#include "core/cl/geometry.h"
#include "core/cl/geometry_structs.h"
#include "core/cl/scene_structs.h"
#include "core/cl/voxel.h"
#include "core/cl/voxel_structs.h"

namespace wayverb {
namespace raytracer {
namespace stochastic {

constexpr auto source = R"(

//  These functions replicate functionality from common/surfaces.h

bands_type absorption_to_energy_reflectance(bands_type t);
bands_type absorption_to_energy_reflectance(bands_type t) { return 1 - t; }

bands_type absorption_to_pressure_reflectance(bands_type t);
bands_type absorption_to_pressure_reflectance(bands_type t) {
    return sqrt(absorption_to_energy_reflectance(t));
}

bands_type pressure_reflectance_to_average_wall_impedance(bands_type t);
bands_type pressure_reflectance_to_average_wall_impedance(bands_type t) {
    return (1 + t) / (1 - t);
}

bands_type average_wall_impedance_to_pressure_reflectance(bands_type t,
                                                           float cos_angle);
bands_type average_wall_impedance_to_pressure_reflectance(bands_type t,
                                                           float cos_angle) {
    const bands_type tmp = t * cos_angle;
    const bands_type ret = (tmp - 1) / (tmp + 1);
    return ret;
}

bands_type scattered(bands_type total_reflected, bands_type scattering);
bands_type scattered(bands_type total_reflected, bands_type scattering) {
    return total_reflected * scattering;
}

bands_type specular(bands_type total_reflected, bands_type scattering);
bands_type specular(bands_type total_reflected, bands_type scattering) {
    return total_reflected * (1 - scattering);
}

kernel void init_stochastic_path_info(global stochastic_path_info* info,
                                      bands_type volume,
                                      float3 position) {
    const size_t thread = get_global_id(0);
    info[thread] =
            (stochastic_path_info){volume, volume, position, 0.0f};
}

kernel void stochastic(const global reflection* reflections,
                    float3 receiver,
                    float receiver_radius,

                    const global triangle* triangles,
                    const global float3* vertices,
                    const global surface* surfaces,

                    global stochastic_path_info* stochastic_path,

                    global impulse* stochastic_output,
                    global impulse* intersected_output) {
    const size_t thread = get_global_id(0);

    //  zero out output
    stochastic_output[thread] = (impulse){};
    intersected_output[thread] = (impulse){};

    //  if this thread doesn't have anything to do, stop now
    if (!reflections[thread].keep_going) {
        return;
    }

    //  find the new volume
    const size_t triangle_index = reflections[thread].triangle;
    const triangle reflective_triangle = triangles[triangle_index];
    const size_t surface_index = reflective_triangle.surface;
    const surface reflective_surface = surfaces[surface_index];

    const bands_type reflectance =
            absorption_to_energy_reflectance(reflective_surface.absorption);

    // Get incoming energy from path accumulators
    const bands_type throughput = stochastic_path[thread].throughput;
    const bands_type deterministic = stochastic_path[thread].deterministic;
    const bands_type outgoing_throughput = throughput * reflectance;
    const bands_type outgoing_specular = deterministic * reflectance;

    // Scattering probabilities and sample info
    const float scatter_probability = reflections[thread].scatter_probability;
    const bool sampled_diffuse = reflections[thread].sampled_diffuse;
    const float diffuse_prob = fmax(scatter_probability, 1.0e-4f);
    const float specular_prob = fmax(1.0f - scatter_probability, 1.0e-4f);

    // Split reflected energy into diffuse and specular components
    const bands_type diffuse_component =
            scattered(outgoing_throughput, reflective_surface.scattering);
    const bands_type specular_component =
            specular(outgoing_throughput, reflective_surface.scattering);
    const bands_type rain_energy =
            scattered(outgoing_specular, reflective_surface.scattering);
    const bands_type specular_chain =
            specular(outgoing_specular, reflective_surface.scattering);

    // Calculate throughput for path continuation with BRDF weighting for diffuse samples
    bands_type diffuse_throughput;
    if (sampled_diffuse) {
        const float cos_theta = fmax(reflections[thread].cos_theta, 0.0f);
        const float sample_pdf = fmax(reflections[thread].sample_pdf, 1e-6f);
        const bands_type diffuse_brdf =
                reflectance * reflective_surface.scattering * (1.0f / M_PI_F);
        diffuse_throughput = throughput * diffuse_brdf * (cos_theta / sample_pdf);
    } else {
        diffuse_throughput = diffuse_component;
    }
    const bands_type specular_throughput = specular_component;


    const float3 last_position = stochastic_path[thread].position;
    const float3 this_position = reflections[thread].position;

    //  find the new distance to this reflection
    const float last_distance = stochastic_path[thread].distance;
    const float this_distance =
            last_distance + distance(last_position, this_position);

    const bands_type propagated_throughput =
            sampled_diffuse ? (diffuse_throughput / diffuse_prob)
                            : (specular_throughput / specular_prob);
    const bands_type propagated_deterministic = specular_chain;

    //  set accumulator
    stochastic_path[thread] = (stochastic_path_info){propagated_throughput,
                                                     propagated_deterministic,
                                                     this_position,
                                                     this_distance};

    //  compute output

    //  stochastic output (diffuse rain per Schroeder 5.20)
    //  Only output when receiver is visible AND path was sampled diffusely
    if (reflections[thread].receiver_visible && sampled_diffuse) {
        const float3 to_receiver = receiver - this_position;
        const float to_receiver_distance = length(to_receiver);
        const float total_distance = this_distance + to_receiver_distance;

        //  This implements diffusion according to Lambert's cosine law.
        //  i.e. The intensity is proportional to the cosine of the angle
        //  between the surface normal and the outgoing vector.
        const float3 tnorm = triangle_normal(reflective_triangle, vertices);
        const float cos_angle = fabs(dot(tnorm, normalize(to_receiver)));

        //  Scattered energy equation:
        //  schroder2011 5.20
        //  detected scattered energy =
        //      incident energy * (1 - a) * s * (1 - cos(y/2)) * 2 * cos(theta)
        //  where
        //      y = opening angle
        //      theta = angle between receiver centre and surface normal

        const float sin_y =
                receiver_radius / max(receiver_radius, to_receiver_distance);
        const float angle_correction = 1 - sqrt(1 - sin_y * sin_y);

        // Apply distance attenuation for diffuse rain
        const float inv_distance_sq =
                1.0f / fmax(to_receiver_distance * to_receiver_distance,
                            1.0e-6f);
        const bands_type diffuse_output = rain_energy * angle_correction *
                                          (2 * cos_angle) * inv_distance_sq;

        //  set output
        stochastic_output[thread] =
                (impulse){diffuse_output, this_position, total_distance};
    }
}

)";

program::program(const core::compute_context& cc)
        : program_wrapper_(
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
                          core::cl_representation_v<
                                  impulse<core::simulation_bands>>,
                          core::cl_representation_v<reflection>,
                          core::cl_representation_v<stochastic_path_info>,
                          core::cl_sources::geometry,
                          core::cl_sources::voxel,
                          ::cl_sources::brdf,
                          source}) {}

}  // namespace stochastic
}  // namespace raytracer
}  // namespace wayverb
