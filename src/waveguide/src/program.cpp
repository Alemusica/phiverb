#include "waveguide/program.h"

#include "waveguide/cl/filters.h"
#include "waveguide/cl/structs.h"
#include "waveguide/cl/utils.h"
#include "waveguide/mesh_descriptor.h"

namespace wayverb {
namespace waveguide {

constexpr auto source = R"(
#define courant (1.0f / sqrt(3.0f))
#define courant_sq (1.0f / 3.0f)
#define WAYVERB_OFFSET_OF(type, member)                                        \
    ((uint)((__constant char*)&(((__constant type*)0)->member) -               \
            (__constant char*)((__constant type*)0)))

typedef struct { PortDirection array[1]; } InnerNodeDirections1;
typedef struct { PortDirection array[2]; } InnerNodeDirections2;
typedef struct { PortDirection array[3]; } InnerNodeDirections3;

InnerNodeDirections1 get_inner_node_directions_1(boundary_type b);
InnerNodeDirections1 get_inner_node_directions_1(boundary_type b) {
    switch (b) {
        case id_nx: return (InnerNodeDirections1){{id_port_nx}};
        case id_px: return (InnerNodeDirections1){{id_port_px}};
        case id_ny: return (InnerNodeDirections1){{id_port_ny}};
        case id_py: return (InnerNodeDirections1){{id_port_py}};
        case id_nz: return (InnerNodeDirections1){{id_port_nz}};
        case id_pz: return (InnerNodeDirections1){{id_port_pz}};

        default: return (InnerNodeDirections1){{-1}};
    }
}

InnerNodeDirections2 get_inner_node_directions_2(int boundary_type);
InnerNodeDirections2 get_inner_node_directions_2(int boundary_type) {
    switch (boundary_type) {
        case id_nx | id_ny:
            return (InnerNodeDirections2){{id_port_nx, id_port_ny}};
        case id_nx | id_py:
            return (InnerNodeDirections2){{id_port_nx, id_port_py}};
        case id_px | id_ny:
            return (InnerNodeDirections2){{id_port_px, id_port_ny}};
        case id_px | id_py:
            return (InnerNodeDirections2){{id_port_px, id_port_py}};
        case id_nx | id_nz:
            return (InnerNodeDirections2){{id_port_nx, id_port_nz}};
        case id_nx | id_pz:
            return (InnerNodeDirections2){{id_port_nx, id_port_pz}};
        case id_px | id_nz:
            return (InnerNodeDirections2){{id_port_px, id_port_nz}};
        case id_px | id_pz:
            return (InnerNodeDirections2){{id_port_px, id_port_pz}};
        case id_ny | id_nz:
            return (InnerNodeDirections2){{id_port_ny, id_port_nz}};
        case id_ny | id_pz:
            return (InnerNodeDirections2){{id_port_ny, id_port_pz}};
        case id_py | id_nz:
            return (InnerNodeDirections2){{id_port_py, id_port_nz}};
        case id_py | id_pz:
            return (InnerNodeDirections2){{id_port_py, id_port_pz}};

        default: return (InnerNodeDirections2){{-1, -1}};
    }
}

InnerNodeDirections3 get_inner_node_directions_3(int boundary_type);
InnerNodeDirections3 get_inner_node_directions_3(int boundary_type) {
    switch (boundary_type) {
        case id_nx | id_ny | id_nz:
            return (InnerNodeDirections3){{id_port_nx, id_port_ny, id_port_nz}};
        case id_nx | id_ny | id_pz:
            return (InnerNodeDirections3){{id_port_nx, id_port_ny, id_port_pz}};
        case id_nx | id_py | id_nz:
            return (InnerNodeDirections3){{id_port_nx, id_port_py, id_port_nz}};
        case id_nx | id_py | id_pz:
            return (InnerNodeDirections3){{id_port_nx, id_port_py, id_port_pz}};
        case id_px | id_ny | id_nz:
            return (InnerNodeDirections3){{id_port_px, id_port_ny, id_port_nz}};
        case id_px | id_ny | id_pz:
            return (InnerNodeDirections3){{id_port_px, id_port_ny, id_port_pz}};
        case id_px | id_py | id_nz:
            return (InnerNodeDirections3){{id_port_px, id_port_py, id_port_nz}};
        case id_px | id_py | id_pz:
            return (InnerNodeDirections3){{id_port_px, id_port_py, id_port_pz}};

        default: return (InnerNodeDirections3){{-1, -1, -1}};
    }
}

PortDirection opposite(PortDirection pd);
PortDirection opposite(PortDirection pd) {
    switch (pd) {
        case id_port_nx: return id_port_px;
        case id_port_px: return id_port_nx;
        case id_port_ny: return id_port_py;
        case id_port_py: return id_port_ny;
        case id_port_nz: return id_port_pz;
        case id_port_pz: return id_port_nz;

        default: return -1;
    }
}

uint boundary_bit_from_port(PortDirection pd);
uint boundary_bit_from_port(PortDirection pd) {
    switch (pd) {
        case id_port_nx: return id_nx;
        case id_port_px: return id_px;
        case id_port_ny: return id_ny;
        case id_port_py: return id_py;
        case id_port_nz: return id_nz;
        case id_port_pz: return id_pz;
        default: return 0;
    }
}

int boundary_local_index(uint boundary_type, uint boundary_bit);
int boundary_local_index(uint boundary_type, uint boundary_bit) {
    uint mask = boundary_type;
    mask &= ~(uint)id_inside;
    mask &= ~(uint)id_reentrant;
    if ((mask & boundary_bit) == 0u || boundary_bit == 0u) {
        return -1;
    }
    uint lower = mask & (boundary_bit - 1u);
    return (int)popcount(lower);
}

inline void record_nan(global int* debug_info,
                       int code,
                       uint global_index,
                       uint boundary_index,
                       int local_idx,
                       uint coeff_index,
                       float filt_state_bits,
                       float a0_bits,
                       float b0_bits,
                       float diff_bits,
                       float filter_input_bits,
                       float prev_bits,
                       float next_bits) {
    if (debug_info == 0) {
        return;
    }
    volatile global int* flag = (volatile global int*)debug_info;
    if (atomic_cmpxchg(flag, 0, 1) == 0) {
        debug_info[0] = code;
        debug_info[1] = (int)global_index;
        debug_info[2] = (int)boundary_index;
        debug_info[3] = local_idx;
        debug_info[4] = (int)coeff_index;
        debug_info[5] = (int)as_uint(filt_state_bits);
        debug_info[6] = (int)as_uint(a0_bits);
        debug_info[7] = (int)as_uint(b0_bits);
        debug_info[8] = (int)as_uint(diff_bits);
        debug_info[9] = (int)as_uint(filter_input_bits);
        debug_info[10] = (int)as_uint(prev_bits);
        debug_info[11] = (int)as_uint(next_bits);
    }
}

filt_real filter_step_canonical_private(
        filt_real input,
        memory_canonical* m,
        const global coefficients_canonical* c) {
    const filt_real a0 = c->a[0];
    const filt_real b0 = c->b[0];
    const filt_real denom0 =
            fabs(a0) > (filt_real)1e-12 ? a0 : (filt_real)1;
    const filt_real output = (input * b0 + m->array[0]) / denom0;
    for (int i = 0; i != CANONICAL_FILTER_ORDER - 1; ++i) {
        const filt_real b =
                c->b[i + 1] == 0 ? 0 : c->b[i + 1] * input;
        const filt_real a =
                c->a[i + 1] == 0 ? 0 : c->a[i + 1] * output;
        m->array[i] = b - a + m->array[i + 1];
    }
    const filt_real b_last = c->b[CANONICAL_FILTER_ORDER] == 0
            ? 0
            : c->b[CANONICAL_FILTER_ORDER] * input;
    const filt_real a_last = c->a[CANONICAL_FILTER_ORDER] == 0
            ? 0
            : c->a[CANONICAL_FILTER_ORDER] * output;
    m->array[CANONICAL_FILTER_ORDER - 1] = b_last - a_last;
    return output;
}
#define NUM_SURROUNDING_PORTS_1 4
typedef struct {
    PortDirection array[NUM_SURROUNDING_PORTS_1];
} SurroundingPorts1;

#define NUM_SURROUNDING_PORTS_2 2
typedef struct {
    PortDirection array[NUM_SURROUNDING_PORTS_2];
} SurroundingPorts2;

SurroundingPorts1 on_boundary_1(InnerNodeDirections1 pd);
SurroundingPorts1 on_boundary_1(InnerNodeDirections1 pd) {
    switch (pd.array[0]) {
        case id_port_nx:
        case id_port_px:
            return (SurroundingPorts1){
                    {id_port_ny, id_port_py, id_port_nz, id_port_pz}};
        case id_port_ny:
        case id_port_py:
            return (SurroundingPorts1){
                    {id_port_nx, id_port_px, id_port_nz, id_port_pz}};
        case id_port_nz:
        case id_port_pz:
            return (SurroundingPorts1){
                    {id_port_nx, id_port_px, id_port_ny, id_port_py}};

        default: return (SurroundingPorts1){{-1, -1, -1, -1}};
    }
}

SurroundingPorts2 on_boundary_2(InnerNodeDirections2 ind);
SurroundingPorts2 on_boundary_2(InnerNodeDirections2 ind) {
    if (ind.array[0] == id_port_nx || ind.array[0] == id_port_px ||
        ind.array[1] == id_port_nx || ind.array[1] == id_port_px) {
        if (ind.array[0] == id_port_ny || ind.array[0] == id_port_py ||
            ind.array[1] == id_port_ny || ind.array[1] == id_port_py) {
            return (SurroundingPorts2){{id_port_nz, id_port_pz}};
        }
        return (SurroundingPorts2){{id_port_ny, id_port_py}};
    }
    return (SurroundingPorts2){{id_port_nx, id_port_px}};
}

//  call with the index of the BOUNDARY node, and the relative direction of the
//  ghost point
//
//  we don't actually care about the pressure at the ghost point other than to
//  calculate the boundary filter input
void ghost_point_pressure_update(
        float next_pressure,
        float prev_pressure,
        float inner_pressure,
        global boundary_data* bd,
        const global coefficients_canonical* boundary,
        volatile global int* error_flag,
        global int* debug_info,
        uint global_index,
        uint boundary_index,
        int local_idx);
void ghost_point_pressure_update(
        float next_pressure,
        float prev_pressure,
        float inner_pressure,
        global boundary_data* bd,
        const global coefficients_canonical* boundary,
        volatile global int* error_flag,
        global int* debug_info,
        uint global_index,
        uint boundary_index,
        int local_idx) {
    (void)inner_pressure;
    memory_canonical local_memory = bd->filter_memory;
    filt_real filt_state = local_memory.array[0];
    filt_real b0 = boundary->b[0];
    filt_real a0 = boundary->a[0];

    if (!isfinite(filt_state)) {
        filt_state = 0;
    }
    if (!isfinite(b0)) {
        b0 = (filt_real)1;
    }
    if (!isfinite(a0)) {
        a0 = (filt_real)1;
    }

    const float delta = prev_pressure - next_pressure;
    if (delta == 0.0f && (float)filt_state == 0.0f) {
        local_memory.array[0] = 0;
        bd->filter_memory = local_memory;
        return;
    }

    const filt_real safe_b0 = fabs(b0) > (filt_real)1e-12 ? b0 : (filt_real)1;
    const filt_real safe_denom = safe_b0 * (filt_real)courant;
    filt_real diff = (safe_denom != 0)
            ? (a0 * (prev_pressure - next_pressure)) / safe_denom +
                      (filt_state / safe_b0)
            : (filt_state / safe_b0);
    if (!isfinite(diff)) {
        atomic_or(error_flag, id_nan_error);
        record_nan(debug_info,
                   1,
                   global_index,
                   boundary_index,
                   local_idx,
                   bd->coefficient_index,
                   (float)filt_state,
                   (float)a0,
                   (float)b0,
                   (float)diff,
                   0.0f,
                   prev_pressure,
                   next_pressure);
        local_memory.array[0] = (filt_real)nan((uint)0);
        bd->filter_memory = local_memory;
        return;
    }
    filt_real filter_input = -diff;
    if (!isfinite(filter_input)) {
        atomic_or(error_flag, id_nan_error);
        record_nan(debug_info,
                   2,
                   global_index,
                   boundary_index,
                   local_idx,
                   bd->coefficient_index,
                   (float)filt_state,
                   (float)a0,
                   (float)b0,
                   (float)diff,
                   (float)filter_input,
                   prev_pressure,
                   next_pressure);
        local_memory.array[0] = (filt_real)nan((uint)0);
        bd->filter_memory = local_memory;
        return;
    }
    const filt_real output =
            filter_step_canonical_private(filter_input, &local_memory, boundary);
    if (!isfinite(output)) {
        atomic_or(error_flag, id_nan_error);
        record_nan(debug_info,
                   3,
                   global_index,
                   boundary_index,
                   local_idx,
                   bd->coefficient_index,
                   (float)filt_state,
                   (float)a0,
                   (float)b0,
                   (float)diff,
                   (float)filter_input,
                   prev_pressure,
                   next_pressure);
    }
    bd->filter_memory = local_memory;
}

////////////////////////////////////////////////////////////////////////////////

#define TEMPLATE_SUM_SURROUNDING_PORTS(dimensions)                           \
    float CAT(get_summed_surrounding_, dimensions)(                          \
            const global condensed_node* nodes,                              \
            CAT(InnerNodeDirections, dimensions) pd,                         \
            const global float* current,                                     \
            int3 locator,                                                    \
            int3 dim,                                                        \
            volatile global int* error_flag);                                \
    float CAT(get_summed_surrounding_, dimensions)(                          \
            const global condensed_node* nodes,                              \
            CAT(InnerNodeDirections, dimensions) pd,                         \
            const global float* current,                                     \
            int3 locator,                                                    \
            int3 dim,                                                        \
            volatile global int* error_flag) {                               \
        float ret = 0;                                                       \
        CAT(SurroundingPorts, dimensions)                                    \
        on_boundary = CAT(on_boundary_, dimensions)(pd);                     \
        for (int i = 0; i != CAT(NUM_SURROUNDING_PORTS_, dimensions); ++i) { \
            uint index = neighbor_index(locator, dim, on_boundary.array[i]); \
            if (index == no_neighbor) {                                      \
                atomic_or(error_flag, id_outside_mesh_error);                \
                return 0;                                                    \
            }                                                                \
            int boundary_type = nodes[index].boundary_type;                  \
            if (boundary_type == id_none || boundary_type == id_inside) {    \
                atomic_or(error_flag, id_suspicious_boundary_error);         \
            }                                                                \
            ret += current[index];                                           \
        }                                                                    \
        return ret;                                                          \
    }

TEMPLATE_SUM_SURROUNDING_PORTS(1);
TEMPLATE_SUM_SURROUNDING_PORTS(2);

float get_summed_surrounding_3(const global condensed_node* nodes,
                               InnerNodeDirections3 i,
                               const global float* current,
                               int3 locator,
                               int3 dimensions,
                               volatile global int* error_flag);
float get_summed_surrounding_3(const global condensed_node* nodes,
                               InnerNodeDirections3 i,
                               const global float* current,
                               int3 locator,
                               int3 dimensions,
                               volatile global int* error_flag) {
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

float get_inner_pressure(const global condensed_node* nodes,
                         const global float* current,
                         int3 locator,
                         int3 dim,
                         PortDirection bt,
                         volatile global int* error_flag);
float get_inner_pressure(const global condensed_node* nodes,
                         const global float* current,
                         int3 locator,
                         int3 dim,
                         PortDirection bt,
                         volatile global int* error_flag) {
    uint neighbor = neighbor_index(locator, dim, bt);
    if (neighbor == no_neighbor) {
        atomic_or(error_flag, id_outside_mesh_error);
        return 0;
    }
    return current[neighbor];
}

#define GET_CURRENT_SURROUNDING_WEIGHTING_TEMPLATE(dimensions)                 \
    float CAT(get_current_surrounding_weighting_, dimensions)(                 \
            const global condensed_node* nodes,                                \
            const global float* current,                                       \
            int3 locator,                                                      \
            int3 dim,                                                          \
            CAT(InnerNodeDirections, dimensions) ind,                          \
            volatile global int* error_flag);                                  \
    float CAT(get_current_surrounding_weighting_, dimensions)(                 \
            const global condensed_node* nodes,                                \
            const global float* current,                                       \
            int3 locator,                                                      \
            int3 dim,                                                          \
            CAT(InnerNodeDirections, dimensions) ind,                          \
            volatile global int* error_flag) {                                 \
        float sum = 0;                                                         \
        for (int i = 0; i != dimensions; ++i) {                                \
            sum += 2 * get_inner_pressure(nodes,                               \
                                          current,                             \
                                          locator,                             \
                                          dim,                                 \
                                          ind.array[i],                        \
                                          error_flag);                         \
        }                                                                      \
        return courant_sq *                                                    \
               (sum + CAT(get_summed_surrounding_, dimensions)(                \
                              nodes, ind, current, locator, dim, error_flag)); \
    }

GET_CURRENT_SURROUNDING_WEIGHTING_TEMPLATE(1);
GET_CURRENT_SURROUNDING_WEIGHTING_TEMPLATE(2);
GET_CURRENT_SURROUNDING_WEIGHTING_TEMPLATE(3);

////////////////////////////////////////////////////////////////////////////////

#define GET_FILTER_WEIGHTING_TEMPLATE(dimensions)                         \
    float CAT(get_filter_weighting_, dimensions)(                         \
            global CAT(boundary_data_array_, dimensions) * bda,           \
            const global coefficients_canonical* boundary_coefficients);  \
    float CAT(get_filter_weighting_, dimensions)(                         \
            global CAT(boundary_data_array_, dimensions) * bda,           \
            const global coefficients_canonical* boundary_coefficients) { \
        float sum = 0;                                                    \
        for (int i = 0; i != dimensions; ++i) {                           \
            boundary_data bd = bda->array[i];                             \
            const filt_real filt_state = bd.filter_memory.array[0];            \
            sum += filt_state /                                           \
                   boundary_coefficients[bd.coefficient_index].b[0];      \
        }                                                                 \
        return courant_sq * sum;                                          \
    }

GET_FILTER_WEIGHTING_TEMPLATE(1);
GET_FILTER_WEIGHTING_TEMPLATE(2);
GET_FILTER_WEIGHTING_TEMPLATE(3);

////////////////////////////////////////////////////////////////////////////////

#define GET_COEFF_WEIGHTING_TEMPLATE(dimensions)                             \
    float CAT(get_coeff_weighting_, dimensions)(                             \
            global CAT(boundary_data_array_, dimensions) * bda,              \
            const global coefficients_canonical* boundary_coefficients);     \
    float CAT(get_coeff_weighting_, dimensions)(                             \
            global CAT(boundary_data_array_, dimensions) * bda,              \
            const global coefficients_canonical* boundary_coefficients) {    \
        float sum = 0;                                                       \
        for (int i = 0; i != dimensions; ++i) {                              \
            const global coefficients_canonical* boundary =                  \
                    boundary_coefficients + bda->array[i].coefficient_index; \
            sum += boundary->a[0] / boundary->b[0];                          \
        }                                                                    \
        return sum * courant;                                                \
    }

GET_COEFF_WEIGHTING_TEMPLATE(1);
GET_COEFF_WEIGHTING_TEMPLATE(2);
GET_COEFF_WEIGHTING_TEMPLATE(3);

////////////////////////////////////////////////////////////////////////////////

#define BOUNDARY_TEMPLATE(dimensions)                                          \
    float CAT(boundary_, dimensions)(                                          \
            const global float* current,                                       \
            float prev_pressure,                                               \
            condensed_node node,                                               \
            const global condensed_node* nodes,                                \
            int3 locator,                                                      \
            int3 dim,                                                          \
            global CAT(boundary_data_array_, dimensions) * bdat,               \
            const global coefficients_canonical* boundary_coefficients,        \
            volatile global int* error_flag,                                   \
            global int* debug_info,                                            \
            uint global_index);                                                \
    float CAT(boundary_, dimensions)(                                          \
            const global float* current,                                       \
            float prev_pressure,                                               \
            condensed_node node,                                               \
            const global condensed_node* nodes,                                \
            int3 locator,                                                      \
            int3 dim,                                                          \
            global CAT(boundary_data_array_, dimensions) * bdat,               \
            const global coefficients_canonical* boundary_coefficients,        \
            volatile global int* error_flag,                                   \
            global int* debug_info,                                            \
            uint global_index) {                                               \
        CAT(InnerNodeDirections, dimensions)                                   \
        ind = CAT(get_inner_node_directions_, dimensions)(node.boundary_type); \
        float current_surrounding_weighting =                                  \
                CAT(get_current_surrounding_weighting_, dimensions)(           \
                        nodes, current, locator, dim, ind, error_flag);        \
        global CAT(boundary_data_array_, dimensions)* bda =                    \
                bdat + node.boundary_index;                                    \
        const float filter_weighting = CAT(get_filter_weighting_, dimensions)( \
                bda, boundary_coefficients);                                   \
        const float coeff_weighting = CAT(get_coeff_weighting_, dimensions)(   \
                bda, boundary_coefficients);                                   \
        const float prev_weighting = (coeff_weighting - 1) * prev_pressure;    \
        const float numerator = current_surrounding_weighting +                \
                                filter_weighting + prev_weighting;             \
        float denom = 1 + coeff_weighting;                                     \
        if (!isfinite(denom) || fabs(denom) < 1.0e-12f) {                      \
            atomic_or(error_flag, id_suspicious_boundary_error);               \
            denom = denom >= 0 ? 1.0f : -1.0f;                                 \
        }                                                                      \
        float ret = numerator / denom;                                         \
        if (!isfinite(ret)) {                                                  \
            atomic_or(error_flag, id_nan_error);                               \
            ret = 0.0f;                                                        \
        }                                                                      \
        for (int i = 0; i != dimensions; ++i) {                                \
            const uint boundary_bit = boundary_bit_from_port(ind.array[i]);    \
            const int local_idx = boundary_local_index(node.boundary_type,     \
                                                       boundary_bit);          \
            if (local_idx < 0 || local_idx >= dimensions) {                    \
                atomic_or(error_flag, id_suspicious_boundary_error);           \
                continue;                                                      \
            }                                                                  \
            global boundary_data* bd = bda->array + local_idx;                 \
            const global coefficients_canonical* boundary =                    \
                    boundary_coefficients + bd->coefficient_index;             \
            ghost_point_pressure_update(ret,                                   \
                                        prev_pressure,                         \
                                        get_inner_pressure(nodes,              \
                                                           current,            \
                                                           locator,            \
                                                           dim,                \
                                                           ind.array[i],       \
                                                           error_flag),        \
                                        bd,                                    \
                                        boundary,                              \
                                        error_flag,                            \
                                        debug_info,                            \
                                        global_index,                          \
                                        node.boundary_index,                   \
                                        local_idx);                            \
        }                                                                      \
        return ret;                                                            \
    }

BOUNDARY_TEMPLATE(1);
BOUNDARY_TEMPLATE(2);
BOUNDARY_TEMPLATE(3);

////////////////////////////////////////////////////////////////////////////////

#define ENABLE_BOUNDARIES (1)

float normal_waveguide_update(float prev_pressure,
                              const global float* current,
                              int3 dimensions,
                              int3 locator);
float normal_waveguide_update(float prev_pressure,
                              const global float* current,
                              int3 dimensions,
                              int3 locator) {
    float ret = 0;
    for (int i = 0; i != PORTS; ++i) {
        uint port_index = neighbor_index(locator, dimensions, i);
        if (port_index != no_neighbor) {
            ret += current[port_index];
        }
    }

    ret /= (PORTS / 2);
    ret -= prev_pressure;
    return ret;
}

float next_waveguide_pressure(
        const condensed_node node,
        const global condensed_node* nodes,
        float prev_pressure,
        const global float* current,
        int3 dimensions,
        int3 locator,
        global boundary_data_array_1* boundary_data_1,
        global boundary_data_array_2* boundary_data_2,
        global boundary_data_array_3* boundary_data_3,
        const global coefficients_canonical* boundary_coefficients,
        volatile global int* error_flag,
        global int* debug_info,
        uint global_index);
float next_waveguide_pressure(
        const condensed_node node,
        const global condensed_node* nodes,
        float prev_pressure,
        const global float* current,
        int3 dimensions,
        int3 locator,
        global boundary_data_array_1* boundary_data_1,
        global boundary_data_array_2* boundary_data_2,
        global boundary_data_array_3* boundary_data_3,
        const global coefficients_canonical* boundary_coefficients,
        volatile global int* error_flag,
        global int* debug_info,
        uint global_index) {
    //  find the next pressure at this node, assign it to next_pressure
    switch (popcount(node.boundary_type)) {
        //  this is inside or outside, not a boundary
        case 1:
            if (node.boundary_type & id_inside ||
                node.boundary_type & id_reentrant) {
                return normal_waveguide_update(
                        prev_pressure, current, dimensions, locator);
            } else {
#if ENABLE_BOUNDARIES
                return boundary_1(current,
                                  prev_pressure,
                                  node,
                                  nodes,
                                  locator,
                                  dimensions,
                                  boundary_data_1,
                                  boundary_coefficients,
                                  error_flag,
                                  debug_info,
                                  global_index);
#endif
            }
        //  this is an edge where two boundaries meet
        case 2:
#if ENABLE_BOUNDARIES
            return boundary_2(current,
                              prev_pressure,
                              node,
                              nodes,
                              locator,
                              dimensions,
                              boundary_data_2,
                              boundary_coefficients,
                              error_flag,
                              debug_info,
                              global_index);
#endif
        //  this is a corner where three boundaries meet
        case 3:
#if ENABLE_BOUNDARIES
            return boundary_3(current,
                              prev_pressure,
                              node,
                              nodes,
                              locator,
                              dimensions,
                              boundary_data_3,
                              boundary_coefficients,
                              error_flag,
                              debug_info,
                              global_index);
#endif
        default: return 0;
    }
}

kernel void zero_buffer(global float* buffer) {
    const size_t thread = get_global_id(0);
    buffer[thread] = 0.0f;
}

kernel void condensed_waveguide(
        global float* previous,
        const global float* current,
        const global condensed_node* nodes,
        int3 dimensions,
        global boundary_data_array_1* boundary_data_1,
        global boundary_data_array_2* boundary_data_2,
        global boundary_data_array_3* boundary_data_3,
        const global coefficients_canonical* boundary_coefficients,
        volatile global int* error_flag,
        global int* debug_info) {
    const size_t index = get_global_id(0);

    const condensed_node node = nodes[index];
    const int3 locator = to_locator(index, dimensions);

    const float prev_pressure = previous[index];
    const float next_pressure = next_waveguide_pressure(node,
                                                        nodes,
                                                        prev_pressure,
                                                        current,
                                                        dimensions,
                                                        locator,
                                                        boundary_data_1,
                                                        boundary_data_2,
                                                        boundary_data_3,
                                                        boundary_coefficients,
                                                        error_flag,
                                                        debug_info,
                                                        (uint)index);

    if (isinf(next_pressure)) {
        atomic_or(error_flag, id_inf_error);
    }
    if (isnan(next_pressure)) {
        atomic_or(error_flag, id_nan_error);
    }

    previous[index] = next_pressure;
}

typedef struct {
    uint sz_memory_canonical;
    uint sz_coefficients_canonical;
    uint sz_boundary_data;
    uint sz_boundary_data_array_3;
    uint off_bd_filter_memory;
    uint off_bd_coefficient_index;
    uint off_b3_data0;
    uint off_b3_data1;
    uint off_b3_data2;
} layout_info_t;

kernel void layout_probe(__global layout_info_t* out_info) {
    if (get_global_id(0) != 0) {
        return;
    }

    layout_info_t info;
    info.sz_memory_canonical = (uint)sizeof(memory_canonical);
    info.sz_coefficients_canonical = (uint)sizeof(coefficients_canonical);
    info.sz_boundary_data = (uint)sizeof(boundary_data);
    info.sz_boundary_data_array_3 = (uint)sizeof(boundary_data_array_3);
    info.off_bd_filter_memory =
            WAYVERB_OFFSET_OF(boundary_data, filter_memory);
    info.off_bd_coefficient_index =
            WAYVERB_OFFSET_OF(boundary_data, coefficient_index);
    info.off_b3_data0 = WAYVERB_OFFSET_OF(boundary_data_array_3, array[0]);
    info.off_b3_data1 = WAYVERB_OFFSET_OF(boundary_data_array_3, array[1]);
    info.off_b3_data2 = WAYVERB_OFFSET_OF(boundary_data_array_3, array[2]);

    *out_info = info;
}

)";

program::program(const core::compute_context& cc)
        : program_wrapper_{
                  cc,
                  std::vector<std::string>{
                          cl_sources::filter_constants,
                          core::cl_representation_v<filt_real>,
                          core::cl_representation_v<memory_biquad>,
                          core::cl_representation_v<coefficients_biquad>,
                          core::cl_representation_v<memory_canonical>,
                          core::cl_representation_v<coefficients_canonical>,
                          core::cl_representation_v<biquad_memory_array>,
                          core::cl_representation_v<biquad_coefficients_array>,
                          core::cl_representation_v<mesh_descriptor>,
                          core::cl_representation_v<error_code>,
                          core::cl_representation_v<condensed_node>,
                          core::cl_representation_v<boundary_data>,
                          core::cl_representation_v<boundary_data_array_1>,
                          core::cl_representation_v<boundary_data_array_2>,
                          core::cl_representation_v<boundary_data_array_3>,
                          core::cl_representation_v<boundary_type>,
                          cl_sources::filters,
                          cl_sources::utils,
                          source}} {}

}  // namespace waveguide
}  // namespace wayverb
