#pragma once
#include <string_view>

namespace wayverb { namespace metal {
inline std::string_view embedded_waveguide_kernels_source() {
    return std::string_view{R"__WAYVERB__(
#include <metal_stdlib>
using namespace metal;

#include <metal_stdlib>
using namespace metal;

namespace wayverb_metal {

constexpr constant uint BIQUAD_ORDER = 2;
constexpr constant uint BIQUAD_SECTIONS = 3;
constexpr constant uint CANONICAL_FILTER_ORDER = BIQUAD_ORDER * BIQUAD_SECTIONS;
constexpr constant uint CANONICAL_FILTER_STORAGE = CANONICAL_FILTER_ORDER + 2;

enum boundary_type : int {
    id_none = 0,
    id_inside = 1 << 0,
    id_nx = 1 << 1,
    id_px = 1 << 2,
    id_ny = 1 << 3,
    id_py = 1 << 4,
    id_nz = 1 << 5,
    id_pz = 1 << 6,
    id_reentrant = 1 << 7,
};

enum PortDirection : uint {
    id_port_nx = 0,
    id_port_px = 1,
    id_port_ny = 2,
    id_port_py = 3,
    id_port_nz = 4,
    id_port_pz = 5,
};

struct condensed_node {
    int boundary_type;
    uint boundary_index;
};

struct mesh_descriptor {
    float3 min_corner;
    int3 dimensions;
    float spacing;
};

struct memory_canonical {
    float array[CANONICAL_FILTER_STORAGE];
};

struct coefficients_canonical {
    float b[CANONICAL_FILTER_STORAGE];
    float a[CANONICAL_FILTER_STORAGE];
};

struct boundary_data {
    memory_canonical filter_memory;
    uint coefficient_index;
    uint guard_tag;
    uint _pad[6];
};

struct boundary_data_array_1 {
    boundary_data array[1];
};

struct boundary_data_array_2 {
    boundary_data array[2];
};

struct boundary_data_array_3 {
    boundary_data array[3];
};

struct trace_record {
    uint kind;
    uint step;
    uint global_index;
    uint face;
    float prev_pressure;
    float current_pressure;
    float next_pressure;
    float fs0_in;
    float fs0_out;
    float diff;
    float a0;
    float b0;
};

struct layout_info {
    uint sz_memory_canonical;
    uint sz_coefficients_canonical;
    uint sz_boundary_data;
    uint sz_boundary_data_array_3;
    uint off_bd_filter_memory;
    uint off_bd_coefficient_index;
    uint off_bd_guard_tag;
    uint off_b3_data0;
    uint off_b3_data1;
    uint off_b3_data2;
};

} // namespace wayverb_metal



namespace wayverb_metal {

inline int metal_atomic_cmpxchg(device atomic_int* ptr, int cmp, int val) {
    int expected = cmp;
    atomic_compare_exchange_strong_explicit(ptr, &expected, val,
                                            memory_order_relaxed,
                                            memory_order_relaxed);
    return expected;
}

inline uint metal_atomic_inc(device atomic_uint* ptr) {
    return atomic_fetch_add_explicit(ptr, 1u, memory_order_relaxed);
}

inline void metal_atomic_or(device atomic_int* ptr, int val) {
    atomic_fetch_or_explicit(ptr, val, memory_order_relaxed);
}

#define atomic_cmpxchg(ptr, cmp, val) \
    metal_atomic_cmpxchg(reinterpret_cast<device atomic_int*>(ptr), cmp, val)
#define atomic_inc(ptr) \
    metal_atomic_inc(reinterpret_cast<device atomic_uint*>(ptr))
#define atomic_or(ptr, val) \
    metal_atomic_or(reinterpret_cast<device atomic_int*>(ptr), val)

#define courant (1.0f / sqrt(3.0f))
#define courant_sq (1.0f / 3.0f)
#define WAYVERB_OFFSET_OF(type, member) \
    ((uint)((constant char*)&(((constant type*)0)->member) - \
            (constant char*)((constant type*)0)))

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

inline void record_nan(device int* debug_info,
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
     device int* flag = ( device int*)debug_info;
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

inline void record_pressure_nan(device int* debug_info,
                                int code,
                                uint global_index,
                                float prev_bits,
                                float next_bits) {
    if (debug_info == 0) {
        return;
    }
     device int* flag = ( device int*)debug_info;
    if (atomic_cmpxchg(flag, 0, 1) == 0) {
        debug_info[0] = code;
        debug_info[1] = (int)global_index;
        debug_info[2] = 0;
        debug_info[3] = 0;
        debug_info[4] = 0;
        debug_info[5] = (int)as_uint(prev_bits);
        debug_info[6] = (int)as_uint(next_bits);
        for (int i = 7; i < 12; ++i) {
            debug_info[i] = 0;
        }
    }
}

typedef struct {
    uint kind;
    uint step;
    uint global_index;
    uint face;
    float prev_pressure;
    float current_pressure;
    float next_pressure;
    float fs0_in;
    float fs0_out;
    float diff;
    float a0;
    float b0;
} trace_record_t;

#define TRACE_KIND_PRESSURE 1u
#define TRACE_KIND_BOUNDARY_1 2u
#define TRACE_KIND_BOUNDARY_2 3u
#define TRACE_KIND_BOUNDARY_3 4u

inline void trace_write(device trace_record_t* records,
                        device uint* head,
                        uint capacity,
                        uint enabled,
                        uint kind,
                        uint step,
                        uint global_index,
                        uint face,
                        float prev_p,
                        float curr_p,
                        float next_p,
                        float fs0_in,
                        float fs0_out,
                        float diff,
                        float a0,
                        float b0) {
    if (!enabled || records == 0 || head == 0 || capacity == 0) {
        return;
    }
    uint slot = atomic_inc(( device uint*)head);
    slot = slot % capacity;
    trace_record_t rec;
    rec.kind = kind;
    rec.step = step;
    rec.global_index = global_index;
    rec.face = face;
    rec.prev_pressure = prev_p;
    rec.current_pressure = curr_p;
    rec.next_pressure = next_p;
    rec.fs0_in = fs0_in;
    rec.fs0_out = fs0_out;
    rec.diff = diff;
    rec.a0 = a0;
    rec.b0 = b0;
    records[slot] = rec;
}

float filter_step_canonical_private(
        float input,
        memory_canonical* m,
        const device coefficients_canonical* c) {
    const float a0 = c->a[0];
    const float b0 = c->b[0];
    const float denom0 =
            fabs(a0) > (float)1e-12 ? a0 : (float)1;
    const float output = (input * b0 + m->array[0]) / denom0;
    for (int i = 0; i != CANONICAL_FILTER_ORDER - 1; ++i) {
        const float b =
                c->b[i + 1] == 0 ? 0 : c->b[i + 1] * input;
        const float a =
                c->a[i + 1] == 0 ? 0 : c->a[i + 1] * output;
        m->array[i] = b - a + m->array[i + 1];
    }
    const float b_last = c->b[CANONICAL_FILTER_ORDER] == 0
            ? 0
            : c->b[CANONICAL_FILTER_ORDER] * input;
    const float a_last = c->a[CANONICAL_FILTER_ORDER] == 0
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

// call with the index of the BOUNDARY node, and the relative direction of the
// ghost point
//
// we don't actually care about the pressure at the ghost point other than to
// calculate the boundary filter input
void ghost_point_pressure_update(
        uint cookie_a,
        float next_pressure,
        float prev_pressure,
        float inner_pressure,
        device boundary_data* bd,
        const device coefficients_canonical* boundary,
         device int* error_flag,
        device int* debug_info,
        uint global_index,
        uint boundary_index,
        int local_idx,
        uint cookie_b);
void ghost_point_pressure_update(
        uint cookie_a,
        float next_pressure,
        float prev_pressure,
        float inner_pressure,
        device boundary_data* bd,
        const device coefficients_canonical* boundary,
         device int* error_flag,
        device int* debug_info,
        uint global_index,
        uint boundary_index,
        int local_idx,
        uint cookie_b) {
    (void)inner_pressure;
    if (cookie_a != 0x12345678u || cookie_b != 0xA5A5A5A5u) {
        atomic_or(error_flag, id_outside_range_error);
        if (debug_info != 0) {
            debug_info[0] = -1;
            debug_info[1] = (int)cookie_a;
            debug_info[2] = (int)cookie_b;
            debug_info[3] = (int)global_index;
        }
        return;
    }
    float a0 = boundary->a[0];
    float b0 = boundary->b[0];
    const float kFilterMemoryLimit = 1.0e30f;
    if (!isfinite(a0)) {
        a0 = (float)1;
    }
    if (!isfinite(b0)) {
        b0 = (float)1;
    }
    if (fabs((float)b0) < 1.0e-12f && fabs((float)a0) < 1.0e-12f) {
        return;
    }
    float filt_state = bd->filter_memory.array[0];
    if (!isfinite(filt_state)) {
        filt_state = 0;
    }
    bool reset_memory = false;
#pragma unroll
    for (int k = 0; k != CANONICAL_FILTER_ORDER; ++k) {
        const float value = (float)bd->filter_memory.array[k];
        if (!isfinite(value) || fabs(value) > kFilterMemoryLimit) {
            reset_memory = true;
            break;
        }
    }
    if (reset_memory) {
        const float filt_state_bits = (float)filt_state;
        record_nan(debug_info,
                   10,
                   global_index,
                   boundary_index,
                   local_idx,
                   bd->coefficient_index,
                   filt_state_bits,
                   (float)a0,
                   (float)b0,
                   0.0f,
                   0.0f,
                   prev_pressure,
                   next_pressure);
#pragma unroll
        for (int k = 0; k != CANONICAL_FILTER_ORDER; ++k) {
            bd->filter_memory.array[k] = (float)0;
        }
        filt_state = 0;
    }

    const float delta = prev_pressure - next_pressure;
    if (delta == 0.0f && (float)filt_state == 0.0f) {
        bd->filter_memory.array[0] = 0;
        return;
    }

    const float safe_b0 = fabs((float)b0) > 1.0e-12f ? (float)b0 : 1.0f;
    const float denom = fmax(safe_b0 * (float)courant, 1.0e-12f);
    const float inv_denom = 1.0f / denom;
    const float diff = fma((float)a0 * delta, inv_denom, (float)filt_state / safe_b0);
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
        bd->filter_memory.array[0] = (float)nan(0u);
        return;
    }
    const float filter_input = -diff;
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
        bd->filter_memory.array[0] = (float)nan(0u);
        return;
    }
    memory_canonical local_memory;
#pragma unroll
    for (int k = 0; k != CANONICAL_FILTER_ORDER; ++k) {
        local_memory.array[k] = bd->filter_memory.array[k];
    }

    const float output =
            filter_step_canonical_private((float)filter_input, &local_memory, boundary);
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
    bool local_clamped = false;
#pragma unroll
    for (int k = 0; k != CANONICAL_FILTER_ORDER; ++k) {
        const float value = (float)local_memory.array[k];
        if (!isfinite(value) || fabs(value) > kFilterMemoryLimit) {
            local_memory.array[k] = (float)0;
            local_clamped = true;
        }
    }
    if (local_clamped) {
        record_nan(debug_info,
                   11,
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
#pragma unroll
    for (int k = 0; k != CANONICAL_FILTER_ORDER; ++k) {
        bd->filter_memory.array[k] = local_memory.array[k];
    }
}

////////////////////////////////////////////////////////////////////////////////

#define TEMPLATE_SUM_SURROUNDING_PORTS(dimensions) \
    float CAT(get_summed_surrounding_, dimensions)( \
            const device condensed_node* nodes, \
            CAT(InnerNodeDirections, dimensions) pd, \
            const device float* current, \
            int3 locator, \
            int3 dim, \
             device int* error_flag); \
    float CAT(get_summed_surrounding_, dimensions)( \
            const device condensed_node* nodes, \
            CAT(InnerNodeDirections, dimensions) pd, \
            const device float* current, \
            int3 locator, \
            int3 dim, \
             device int* error_flag) { \
        float ret = 0; \
        CAT(SurroundingPorts, dimensions) \
        on_boundary = CAT(on_boundary_, dimensions)(pd); \
        for (int i = 0; i != CAT(NUM_SURROUNDING_PORTS_, dimensions); ++i) { \
            uint index = neighbor_index(locator, dim, on_boundary.array[i]); \
            if (index == no_neighbor) { \
                atomic_or(error_flag, id_outside_mesh_error); \
                return 0; \
            } \
            int boundary_type = nodes[index].boundary_type; \
            if (boundary_type == id_none || boundary_type == id_inside) { \
                atomic_or(error_flag, id_suspicious_boundary_error); \
            } \
            ret += current[index]; \
        } \
        return ret; \
    }

TEMPLATE_SUM_SURROUNDING_PORTS(1);
TEMPLATE_SUM_SURROUNDING_PORTS(2);

float get_summed_surrounding_3(const device condensed_node* nodes,
                               InnerNodeDirections3 i,
                               const device float* current,
                               int3 locator,
                               int3 dimensions,
                                device int* error_flag);
float get_summed_surrounding_3(const device condensed_node* nodes,
                               InnerNodeDirections3 i,
                               const device float* current,
                               int3 locator,
                               int3 dimensions,
                                device int* error_flag) {
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

float get_inner_pressure(const device condensed_node* nodes,
                         const device float* current,
                         int3 locator,
                         int3 dim,
                         PortDirection bt,
                          device int* error_flag);
float get_inner_pressure(const device condensed_node* nodes,
                         const device float* current,
                         int3 locator,
                         int3 dim,
                         PortDirection bt,
                          device int* error_flag) {
    uint neighbor = neighbor_index(locator, dim, bt);
    if (neighbor == no_neighbor) {
        atomic_or(error_flag, id_outside_mesh_error);
        return 0;
    }
    return current[neighbor];
}

#define GET_CURRENT_SURROUNDING_WEIGHTING_TEMPLATE(dimensions) \
    float CAT(get_current_surrounding_weighting_, dimensions)( \
            const device condensed_node* nodes, \
            const device float* current, \
            int3 locator, \
            int3 dim, \
            CAT(InnerNodeDirections, dimensions) ind, \
             device int* error_flag); \
    float CAT(get_current_surrounding_weighting_, dimensions)( \
            const device condensed_node* nodes, \
            const device float* current, \
            int3 locator, \
            int3 dim, \
            CAT(InnerNodeDirections, dimensions) ind, \
             device int* error_flag) { \
        float sum = 0; \
        for (int i = 0; i != dimensions; ++i) { \
            sum += 2 * get_inner_pressure(nodes, \
                                          current, \
                                          locator, \
                                          dim, \
                                          ind.array[i], \
                                          error_flag); \
        } \
        return courant_sq * \
               (sum + CAT(get_summed_surrounding_, dimensions)( \
                              nodes, ind, current, locator, dim, error_flag)); \
    }

GET_CURRENT_SURROUNDING_WEIGHTING_TEMPLATE(1);
GET_CURRENT_SURROUNDING_WEIGHTING_TEMPLATE(2);
GET_CURRENT_SURROUNDING_WEIGHTING_TEMPLATE(3);

////////////////////////////////////////////////////////////////////////////////

#define GET_FILTER_WEIGHTING_TEMPLATE(dimensions) \
    float CAT(get_filter_weighting_, dimensions)( \
            device CAT(boundary_data_array_, dimensions) * bda, \
            const device coefficients_canonical* boundary_coefficients); \
    float CAT(get_filter_weighting_, dimensions)( \
            device CAT(boundary_data_array_, dimensions) * bda, \
            const device coefficients_canonical* boundary_coefficients) { \
        float sum = 0; \
        for (int i = 0; i != dimensions; ++i) { \
            const device boundary_data* bd_ptr = bda->array + i; \
            const float filt_state = bd_ptr->filter_memory.array[0]; \
            const float b0 = \
                    boundary_coefficients[bd_ptr->coefficient_index].b[0];\
            if (fabs((float)b0) > 1.0e-12f) { \
                sum += filt_state / b0; \
            } \
        } \
        return courant_sq * sum; \
    }

GET_FILTER_WEIGHTING_TEMPLATE(1);
GET_FILTER_WEIGHTING_TEMPLATE(2);
GET_FILTER_WEIGHTING_TEMPLATE(3);

////////////////////////////////////////////////////////////////////////////////

#define GET_COEFF_WEIGHTING_TEMPLATE(dimensions) \
    float CAT(get_coeff_weighting_, dimensions)( \
            device CAT(boundary_data_array_, dimensions) * bda, \
            const device coefficients_canonical* boundary_coefficients); \
    float CAT(get_coeff_weighting_, dimensions)( \
            device CAT(boundary_data_array_, dimensions) * bda, \
            const device coefficients_canonical* boundary_coefficients) { \
        float sum = 0; \
        for (int i = 0; i != dimensions; ++i) { \
            const device coefficients_canonical* boundary = \
                    boundary_coefficients + bda->array[i].coefficient_index; \
            const float a0 = (float)boundary->a[0]; \
            const float b0 = (float)boundary->b[0]; \
            if (fabs(b0) > 1.0e-12f) { \
                sum += a0 / b0; \
            } \
        } \
        return sum * courant; \
    }

GET_COEFF_WEIGHTING_TEMPLATE(1);
GET_COEFF_WEIGHTING_TEMPLATE(2);
GET_COEFF_WEIGHTING_TEMPLATE(3);

////////////////////////////////////////////////////////////////////////////////

#define BOUNDARY_TEMPLATE(dimensions) \
    float CAT(boundary_, dimensions)( \
            const device float* current, \
            float prev_pressure, \
            condensed_node node, \
            const device condensed_node* nodes, \
            int3 locator, \
            int3 dim, \
            device CAT(boundary_data_array_, dimensions) * bdat, \
            const device coefficients_canonical* boundary_coefficients, \
             device int* error_flag, \
            device int* debug_info, \
            uint global_index); \
    float CAT(boundary_, dimensions)( \
            const device float* current, \
            float prev_pressure, \
            condensed_node node, \
            const device condensed_node* nodes, \
            int3 locator, \
            int3 dim, \
            device CAT(boundary_data_array_, dimensions) * bdat, \
            const device coefficients_canonical* boundary_coefficients, \
             device int* error_flag, \
            device int* debug_info, \
            uint global_index) { \
        CAT(InnerNodeDirections, dimensions) \
        ind = CAT(get_inner_node_directions_, dimensions)(node.boundary_type); \
        float current_surrounding_weighting = \
                CAT(get_current_surrounding_weighting_, dimensions)( \
                        nodes, current, locator, dim, ind, error_flag); \
        device CAT(boundary_data_array_, dimensions)* bda = \
                bdat + node.boundary_index; \
        const float filter_weighting = CAT(get_filter_weighting_, dimensions)( \
                bda, boundary_coefficients); \
        const float coeff_weighting = CAT(get_coeff_weighting_, dimensions)( \
                bda, boundary_coefficients); \
        const float prev_weighting = (coeff_weighting - 1) * prev_pressure; \
        const float numerator = current_surrounding_weighting + \
                                filter_weighting + prev_weighting; \
        float denom = 1 + coeff_weighting; \
        if (!isfinite(denom) || fabs(denom) < 1.0e-12f) { \
            atomic_or(error_flag, id_suspicious_boundary_error); \
            denom = denom >= 0 ? 1.0f : -1.0f; \
        } \
        float ret = numerator / denom; \
        if (!isfinite(ret)) { \
            record_nan(debug_info, \
                       200 + dimensions, \
                       global_index, \
                       node.boundary_index, \
                       -1, \
                       0, \
                       filter_weighting, \
                       coeff_weighting, \
                       denom, \
                       numerator, \
                       denom, \
                       prev_pressure, \
                       ret); \
            atomic_or(error_flag, id_nan_error); \
            ret = 0.0f; \
        } \
        return ret; \
    }

BOUNDARY_TEMPLATE(1);
BOUNDARY_TEMPLATE(2);
BOUNDARY_TEMPLATE(3);

////////////////////////////////////////////////////////////////////////////////

#define UPDATE_BOUNDARY_KERNEL(dims_count) \
    kernel void CAT(update_boundary_, dims_count)( \
            const device float* previous_history, \
            const device float* current, \
            device float* next, \
            const device condensed_node* nodes, \
            int3 grid_dimensions, \
            const device uint* boundary_nodes, \
            device CAT(boundary_data_array_, dims_count) * boundary_storage, \
            const device coefficients_canonical* boundary_coefficients, \
             device int* error_flag, \
            device int* debug_info, \
            const uint trace_target, \
            device trace_record_t* trace_records, \
            device uint* trace_head, \
            const uint trace_capacity, \
            const uint trace_enabled, \
            const uint step_index, \
            uint gid [[thread_position_in_grid]]) { \
        const uint work_index = (uint)gid; \
        const uint global_index = boundary_nodes[work_index]; \
        const condensed_node node = nodes[global_index]; \
        const uint boundary_index = node.boundary_index; \
        if (boundary_index != work_index) { \
            atomic_or(error_flag, id_outside_range_error); \
            if (debug_info != 0) { \
                debug_info[0] = id_outside_range_error; \
                debug_info[1] = (int)boundary_index; \
                debug_info[2] = (int)work_index; \
            } \
            return; \
        } \
        device CAT(boundary_data_array_, dims_count)* bdat = \
                boundary_storage + boundary_index; \
        const float next_pressure = next[global_index]; \
        const float prev_pressure = previous_history[global_index]; \
        const float current_pressure = current[global_index]; \
        const int3 locator = to_locator(global_index, grid_dimensions); \
        CAT(InnerNodeDirections, dims_count) ind = \
                CAT(get_inner_node_directions_, dims_count)(node.boundary_type); \
        const int faces = (dims_count); \
        for (int face = 0; face != faces; ++face) { \
            const PortDirection pd = ind.array[face]; \
            if (pd == (PortDirection)(-1)) { \
                continue; \
            } \
            const uint boundary_bit = boundary_bit_from_port(pd); \
            const int local_idx = \
                    boundary_local_index(node.boundary_type, boundary_bit); \
            if (local_idx < 0 || local_idx >= faces) { \
                atomic_or(error_flag, id_suspicious_boundary_error); \
                continue; \
            } \
            device boundary_data* bd = bdat->array + local_idx; \
            const device coefficients_canonical* boundary = \
                    boundary_coefficients + bd->coefficient_index; \
            const float fs0_in = bd->filter_memory.array[0]; \
            const float inner_pressure = \
                    get_inner_pressure(nodes, \
                                       current, \
                                       locator, \
                                       grid_dimensions, \
                                       pd, \
                                       error_flag); \
            if (trace_enabled && trace_target == global_index) { \
                trace_write(trace_records, \
                            trace_head, \
                            trace_capacity, \
                            trace_enabled, \
                            CAT(TRACE_KIND_BOUNDARY_, dims_count), \
                            step_index, \
                            global_index, \
                            (uint)local_idx, \
                            prev_pressure, \
                            current_pressure, \
                            next_pressure, \
                            fs0_in, \
                            fs0_in, \
                            0.0f, \
                            boundary->a[0], \
                            boundary->b[0]); \
            } \
            ghost_point_pressure_update(0x12345678u, \
                                        next_pressure, \
                                        prev_pressure, \
                                        inner_pressure, \
                                        bd, \
                                        boundary, \
                                        error_flag, \
                                        debug_info, \
                                        global_index, \
                                        boundary_index, \
                                        local_idx, \
                                        0xA5A5A5A5u); \
            if (trace_enabled && trace_target == global_index) { \
                const float fs0_out = bd->filter_memory.array[0]; \
                trace_write(trace_records, \
                            trace_head, \
                            trace_capacity, \
                            trace_enabled, \
                            CAT(TRACE_KIND_BOUNDARY_, dims_count), \
                            step_index, \
                            global_index, \
                            (uint)local_idx, \
                            prev_pressure, \
                            current_pressure, \
                            next_pressure, \
                            fs0_in, \
                            fs0_out, \
                            0.0f, \
                            boundary->a[0], \
                            boundary->b[0]); \
            } \
        } \
    }

UPDATE_BOUNDARY_KERNEL(1);
UPDATE_BOUNDARY_KERNEL(2);
UPDATE_BOUNDARY_KERNEL(3);

////////////////////////////////////////////////////////////////////////////////

#define ENABLE_BOUNDARIES (1)

float normal_waveguide_update(float prev_pressure,
                              const device float* current,
                              int3 dimensions,
                              int3 locator);
float normal_waveguide_update(float prev_pressure,
                              const device float* current,
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
        const device condensed_node* nodes,
        float prev_pressure,
        const device float* current,
        int3 dimensions,
        int3 locator,
        device boundary_data_array_1* boundary_data_1,
        device boundary_data_array_2* boundary_data_2,
        device boundary_data_array_3* boundary_data_3,
        const device coefficients_canonical* boundary_coefficients,
         device int* error_flag,
        device int* debug_info,
        uint global_index);
float next_waveguide_pressure(
        const condensed_node node,
        const device condensed_node* nodes,
        float prev_pressure,
        const device float* current,
        int3 dimensions,
        int3 locator,
        device boundary_data_array_1* boundary_data_1,
        device boundary_data_array_2* boundary_data_2,
        device boundary_data_array_3* boundary_data_3,
        const device coefficients_canonical* boundary_coefficients,
         device int* error_flag,
        device int* debug_info,
        uint global_index) {
    // find the next pressure at this node, assign it to next_pressure
    switch (popcount(node.boundary_type)) {
        // this is inside or outside, not a boundary
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
        // this is an edge where two boundaries meet
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
        // this is a corner where three boundaries meet
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

kernel void zero_buffer(device float* buffer,
                        uint gid [[thread_position_in_grid]]) {
    buffer[gid] = 0.0f;
}

kernel void condensed_waveguide(
        device float* previous,
        const device float* current,
        const device condensed_node* nodes,
        int3 dimensions,
        device boundary_data_array_1* boundary_data_1,
        device boundary_data_array_2* boundary_data_2,
        device boundary_data_array_3* boundary_data_3,
        const device coefficients_canonical* boundary_coefficients,
         device int* error_flag,
        device int* debug_info,
        const uint num_prev,
        const uint trace_target,
        device trace_record_t* trace_records,
        device uint* trace_head,
        const uint trace_capacity,
        const uint trace_enabled,
        const uint step_index,
        uint gid [[thread_position_in_grid]]) {
    const uint index = gid;

    if (index >= num_prev) {
        atomic_or(error_flag, id_outside_range_error);
        if (debug_info != 0) {
            debug_info[0] = id_outside_range_error;
            debug_info[1] = (int)index;
            debug_info[2] = (int)num_prev;
        }
        return;
    }

    const condensed_node node = nodes[index];
    const int3 locator = to_locator(index, dimensions);

    const float prev_pressure = previous[index];
    const float current_pressure = current[index];
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
    if (trace_enabled && trace_target == (uint)index) {
        trace_write(trace_records,
                    trace_head,
                    trace_capacity,
                    trace_enabled,
                    TRACE_KIND_PRESSURE,
                    step_index,
                    (uint)index,
                    0u,
                    prev_pressure,
                    current_pressure,
                    next_pressure,
                    0.0f,
                    0.0f,
                    0.0f,
                    0.0f,
                    0.0f);
    }

    if (isnan(next_pressure)) {
        record_pressure_nan(debug_info,
                            100,
                            (uint)index,
                            prev_pressure,
                            next_pressure);
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
    uint off_bd_guard_tag;
    uint off_b3_data0;
    uint off_b3_data1;
    uint off_b3_data2;
} layout_info_t;

kernel void layout_probe(device layout_info_t* out_info,
                         uint gid [[thread_position_in_grid]]) {
    if (gid != 0) {
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
    info.off_bd_guard_tag = WAYVERB_OFFSET_OF(boundary_data, guard_tag);
    info.off_b3_data0 = WAYVERB_OFFSET_OF(boundary_data_array_3, array[0]);
    info.off_b3_data1 = WAYVERB_OFFSET_OF(boundary_data_array_3, array[1]);
    info.off_b3_data2 = WAYVERB_OFFSET_OF(boundary_data_array_3, array[2]);

    *out_info = info;
}

kernel void probe_previous(
        const device float* previous, uint probe_index, device float* out,
        uint gid [[thread_position_in_grid]]) {
    if (gid == 0) {
        out[0] = previous[probe_index];
    }
}

} // namespace wayverb_metal

)__WAYVERB__"};
}
} }  // namespace wayverb::metal
