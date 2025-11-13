#include "raytracer/cl/brdf.h"

namespace cl_sources {
const char* brdf{R"(
static inline float3 cosine_sample_hemisphere(float u1, float u2) {
    const float u1_clamped = clamp(u1, 0.0f, 0.9999999f);
    const float u2_wrapped = u2 - floor(u2);
    const float r = sqrt(u1_clamped);
    const float phi = (float)(2.0f * M_PI_F) * u2_wrapped;
    float s_phi;
    float c_phi;
    s_phi = sin(phi);
    c_phi = cos(phi);
    const float z = sqrt(fmax(0.0f, 1.0f - u1_clamped));
    return (float3)(r * c_phi, r * s_phi, z);
}

static inline void onb_frisvad(float3 n, float3* tangent, float3* bitangent) {
    const float len_sq = dot(n, n);
    if (!(len_sq > 0.0f)) {
        *tangent = (float3)(1.0f, 0.0f, 0.0f);
        *bitangent = (float3)(0.0f, 1.0f, 0.0f);
        return;
    }

    const float inv_len = rsqrt(len_sq);
    const float3 nn = n * inv_len;
    const float abs_z = fabs(nn.z);
    if (abs_z > 0.999999f) {
        const float sign_z = copysign(1.0f, nn.z);
        *tangent = (float3)(1.0f, 0.0f, 0.0f);
        *bitangent = (float3)(0.0f, sign_z, 0.0f);
        return;
    }

    const float sign_z = copysign(1.0f, nn.z);
    const float a = -1.0f / (sign_z + nn.z);
    const float b = a * nn.x * nn.y;
    *tangent = normalize((float3)(1.0f + sign_z * nn.x * nn.x * a,
                                  sign_z * b,
                                  -sign_z * nn.x));
    *bitangent = normalize((float3)(b,
                                    sign_z + nn.y * nn.y * a,
                                    -nn.y));
}

static inline float3 lambert_sample(float3 surface_normal,
                                    float u1,
                                    float u2,
                                    float* cos_theta) {
    const float normal_len_sq = dot(surface_normal, surface_normal);
    float3 shading_normal = surface_normal;
    if (normal_len_sq > 0.0f) {
        shading_normal *= rsqrt(normal_len_sq);
    } else {
        shading_normal = (float3)(0.0f, 1.0f, 0.0f);
    }

    float3 tangent;
    float3 bitangent;
    onb_frisvad(shading_normal, &tangent, &bitangent);
    const float3 local = cosine_sample_hemisphere(u1, u2);
    float3 world = local.x * tangent +
                   local.y * bitangent +
                   local.z * shading_normal;
    world = normalize(world);
    const float cos_theta_local = fmax(0.0f, dot(world, shading_normal));
    if (cos_theta) {
        *cos_theta = cos_theta_local;
    }
    return world;
}

static inline float lambert_pdf(float cos_theta) {
    return cos_theta > 0.0f ? cos_theta * (1.0f / M_PI_F) : 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

/*
//  Taken from
//  http://file.scirp.org/pdf/OJA_2015122513452619.pdf
//  but with some modifications so that rays only radiate out in a hemisphere
//  instead of a sphere.

//  y: angle between specular and outgoing vectors (radians)
//  d: the directionality coefficient of the surface
//
//  TODO we might need to adjust the magnitude to correct for not-radiating-in-
//  all-directions
float get_frac(float numerator, float denominator);
float get_frac(float numerator, float denominator) {
    return denominator ? numerator / denominator : 0;
}

float brdf_mag(float y, float d);
float brdf_mag(float y, float d) {
    const float y_sq = y * y;
    const float one_minus_d_sq = pow(1 - d, 2);
    const float numerator = 2 * one_minus_d_sq * y_sq + 2 * d - 1;

    const float to_sqrt = max(one_minus_d_sq * y_sq + 2 * d - 1, 0.0f);

    if (0.5 <= d) {
        const float denominator = 4 * M_PI * d * sqrt(to_sqrt);
        const float extra = ((1 - d) * y) / (2 * M_PI * d);
        return get_frac(numerator, denominator) + extra;
    }

    const float denominator = 2 * M_PI * d * sqrt(to_sqrt);
    return get_frac(numerator, denominator);
}

//  specular: the specular reflection direction (unit vector)
//  outgoing: the actual direction of the outgoing reflection (unit vector)
//  d: the directionality coefficient of the surface
float brdf_mag_for_outgoing(float3 specular, float3 outgoing, float d);
float brdf_mag_for_outgoing(float3 specular, float3 outgoing, float d) {
    return brdf_mag(dot(specular, outgoing), d);
}

bands_type brdf_mags_for_outgoing(float3 specular,
                                   float3 outgoing,
                                   bands_type d);
bands_type brdf_mags_for_outgoing(float3 specular,
                                   float3 outgoing,
                                   bands_type d) {
    return (bands_type)(brdf_mag_for_outgoing(specular, outgoing, d.s0),
                         brdf_mag_for_outgoing(specular, outgoing, d.s1),
                         brdf_mag_for_outgoing(specular, outgoing, d.s2),
                         brdf_mag_for_outgoing(specular, outgoing, d.s3),
                         brdf_mag_for_outgoing(specular, outgoing, d.s4),
                         brdf_mag_for_outgoing(specular, outgoing, d.s5),
                         brdf_mag_for_outgoing(specular, outgoing, d.s6),
                         brdf_mag_for_outgoing(specular, outgoing, d.s7));
}
*/

////////////////////////////////////////////////////////////////////////////////

float mean(bands_type v);
float mean(bands_type v) {
    return (v.s0 + v.s1 + v.s2 + v.s3 + v.s4 + v.s5 + v.s6 + v.s7) / 8;
}
)"};
}  // namespace cl_sources
