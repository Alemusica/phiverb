#include <metal_stdlib>
using namespace metal;

#include "waveguide/metal/layout_structs.metal"

namespace wayverb_metal {

constexpr constant float COURANT = 1.0f / sqrt(3.0f);
constexpr constant float COURANT_SQ = 1.0f / 3.0f;
constexpr constant uint NUM_PORTS = 6;
constexpr constant uint TRACE_KIND_PRESSURE = 1u;
constexpr constant uint TRACE_KIND_BOUNDARY_1 = 2u;
constexpr constant uint TRACE_KIND_BOUNDARY_2 = 3u;
constexpr constant uint TRACE_KIND_BOUNDARY_3 = 4u;
constexpr constant uint NO_NEIGHBOR = ~0u;

struct InnerNodeDirections1 {
    PortDirection array[1];
};

struct InnerNodeDirections2 {
    PortDirection array[2];
};

struct InnerNodeDirections3 {
    PortDirection array[3];
};

struct SurroundingPorts1 {
    PortDirection array[4];
};

struct SurroundingPorts2 {
    PortDirection array[2];
};

inline bool locator_outside(int3 locator, int3 dim) {
    return any(locator < int3(0)) || any(locator >= dim);
}

inline int3 to_locator(uint index, int3 dim) {
    const int xrem = index % dim.x;
    const int xquot = index / dim.x;
    const int yrem = xquot % dim.y;
    const int yquot = xquot / dim.y;
    const int zrem = yquot % dim.z;
    return int3(xrem, yrem, zrem);
}

inline uint to_index(int3 locator, int3 dim) {
    return uint(locator.x + locator.y * dim.x + locator.z * dim.x * dim.y);
}

inline uint neighbor_index(int3 locator, int3 dim, PortDirection pd) {
    switch (pd) {
        case id_port_nx: locator += int3(-1, 0, 0); break;
        case id_port_px: locator += int3(1, 0, 0); break;
        case id_port_ny: locator += int3(0, -1, 0); break;
        case id_port_py: locator += int3(0, 1, 0); break;
        case id_port_nz: locator += int3(0, 0, -1); break;
        case id_port_pz: locator += int3(0, 0, 1); break;
    }
    if (locator_outside(locator, dim)) return NO_NEIGHBOR;
    return to_index(locator, dim);
}

inline float3 compute_node_position(const mesh_descriptor descriptor,
                                    int3 locator) {
    return descriptor.min_corner + float3(locator) * descriptor.spacing;
}

inline PortDirection opposite(PortDirection pd) {
    switch (pd) {
        case id_port_nx: return id_port_px;
        case id_port_px: return id_port_nx;
        case id_port_ny: return id_port_py;
        case id_port_py: return id_port_ny;
        case id_port_nz: return id_port_pz;
        case id_port_pz: return id_port_nz;
    }
    return id_port_nx;
}

inline uint boundary_bit_from_port(PortDirection pd) {
    switch (pd) {
        case id_port_nx: return id_nx;
        case id_port_px: return id_px;
        case id_port_ny: return id_ny;
        case id_port_py: return id_py;
        case id_port_nz: return id_nz;
        case id_port_pz: return id_pz;
    }
    return 0u;
}

inline InnerNodeDirections1 get_inner_node_directions_1(uint b) {
    switch (b) {
        case id_nx: return InnerNodeDirections1{{id_port_nx}};
        case id_px: return InnerNodeDirections1{{id_port_px}};
        case id_ny: return InnerNodeDirections1{{id_port_ny}};
        case id_py: return InnerNodeDirections1{{id_port_py}};
        case id_nz: return InnerNodeDirections1{{id_port_nz}};
        case id_pz: return InnerNodeDirections1{{id_port_pz}};
        default: return InnerNodeDirections1{{(PortDirection)(-1)}};
    }
}

inline InnerNodeDirections2 get_inner_node_directions_2(uint bt) {
    switch (bt) {
        case id_nx | id_ny: return InnerNodeDirections2{{id_port_nx, id_port_ny}};
        case id_nx | id_py: return InnerNodeDirections2{{id_port_nx, id_port_py}};
        case id_px | id_ny: return InnerNodeDirections2{{id_port_px, id_port_ny}};
        case id_px | id_py: return InnerNodeDirections2{{id_port_px, id_port_py}};
        case id_nx | id_nz: return InnerNodeDirections2{{id_port_nx, id_port_nz}};
        case id_nx | id_pz: return InnerNodeDirections2{{id_port_nx, id_port_pz}};
        case id_px | id_nz: return InnerNodeDirections2{{id_port_px, id_port_nz}};
        case id_px | id_pz: return InnerNodeDirections2{{id_port_px, id_port_pz}};
        case id_ny | id_nz: return InnerNodeDirections2{{id_port_ny, id_port_nz}};
        case id_ny | id_pz: return InnerNodeDirections2{{id_port_ny, id_port_pz}};
        case id_py | id_nz: return InnerNodeDirections2{{id_port_py, id_port_nz}};
        case id_py | id_pz: return InnerNodeDirections2{{id_port_py, id_port_pz}};
        default: return InnerNodeDirections2{{(PortDirection)(-1), (PortDirection)(-1)}};
    }
}

inline InnerNodeDirections3 get_inner_node_directions_3(uint bt) {
    switch (bt) {
        case id_nx | id_ny | id_nz:
            return InnerNodeDirections3{{id_port_nx, id_port_ny, id_port_nz}};
        case id_nx | id_ny | id_pz:
            return InnerNodeDirections3{{id_port_nx, id_port_ny, id_port_pz}};
        case id_nx | id_py | id_nz:
            return InnerNodeDirections3{{id_port_nx, id_port_py, id_port_nz}};
        case id_nx | id_py | id_pz:
            return InnerNodeDirections3{{id_port_nx, id_port_py, id_port_pz}};
        case id_px | id_ny | id_nz:
            return InnerNodeDirections3{{id_port_px, id_port_ny, id_port_nz}};
        case id_px | id_ny | id_pz:
            return InnerNodeDirections3{{id_port_px, id_port_ny, id_port_pz}};
        case id_px | id_py | id_nz:
            return InnerNodeDirections3{{id_port_px, id_port_py, id_port_nz}};
        case id_px | id_py | id_pz:
            return InnerNodeDirections3{{id_port_px, id_port_py, id_port_pz}};
        default:
            return InnerNodeDirections3{{(PortDirection)(-1), (PortDirection)(-1), (PortDirection)(-1)}};
    }
}

inline SurroundingPorts1 on_boundary_1(InnerNodeDirections1 pd) {
    switch (pd.array[0]) {
        case id_port_nx:
        case id_port_px:
            return SurroundingPorts1{{id_port_ny, id_port_py, id_port_nz, id_port_pz}};
        case id_port_ny:
        case id_port_py:
            return SurroundingPorts1{{id_port_nx, id_port_px, id_port_nz, id_port_pz}};
        case id_port_nz:
        case id_port_pz:
            return SurroundingPorts1{{id_port_nx, id_port_px, id_port_ny, id_port_py}};
        default:
            return SurroundingPorts1{{(PortDirection)(-1), (PortDirection)(-1), (PortDirection)(-1), (PortDirection)(-1)}};
    }
}

inline SurroundingPorts2 on_boundary_2(InnerNodeDirections2 ind) {
    if (ind.array[0] == id_port_nx || ind.array[0] == id_port_px ||
        ind.array[1] == id_port_nx || ind.array[1] == id_port_px) {
        if (ind.array[0] == id_port_ny || ind.array[0] == id_port_py ||
            ind.array[1] == id_port_ny || ind.array[1] == id_port_py) {
            return SurroundingPorts2{{id_port_nz, id_port_pz}};
        }
        return SurroundingPorts2{{id_port_ny, id_port_py}};
    }
    return SurroundingPorts2{{id_port_nx, id_port_px}};
}

inline int boundary_local_index(uint boundary_type_value, uint boundary_bit) {
    uint mask = boundary_type_value;
    mask &= ~(uint)id_inside;
    mask &= ~(uint)id_reentrant;
    if ((mask & boundary_bit) == 0u || boundary_bit == 0u) return -1;
    uint lower = mask & (boundary_bit - 1u);
    return (int)popcount(lower);
}

inline bool try_claim_debug(device int* debug_info) {
    if (!debug_info) return false;
    device atomic_int* flag = reinterpret_cast<device atomic_int*>(debug_info);
    int expected = 0;
    return atomic_compare_exchange_strong_explicit(
            flag, &expected, 1, memory_order_relaxed, memory_order_relaxed);
}

inline void flag_error(device atomic_int* error_flag, int code) {
    if (!error_flag) return;
    atomic_fetch_or_explicit(error_flag, code, memory_order_relaxed);
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
    if (!try_claim_debug(debug_info)) return;
    debug_info[0] = code;
    debug_info[1] = int(global_index);
    debug_info[2] = int(boundary_index);
    debug_info[3] = local_idx;
    debug_info[4] = int(coeff_index);
    debug_info[5] = int(as_type<uint>(filt_state_bits));
    debug_info[6] = int(as_type<uint>(a0_bits));
    debug_info[7] = int(as_type<uint>(b0_bits));
    debug_info[8] = int(as_type<uint>(diff_bits));
    debug_info[9] = int(as_type<uint>(filter_input_bits));
    debug_info[10] = int(as_type<uint>(prev_bits));
    debug_info[11] = int(as_type<uint>(next_bits));
}

inline void record_pressure_nan(device int* debug_info,
                                int code,
                                uint global_index,
                                float prev_bits,
                                float next_bits) {
    if (!try_claim_debug(debug_info)) return;
    debug_info[0] = code;
    debug_info[1] = int(global_index);
    debug_info[2] = 0;
    debug_info[3] = 0;
    debug_info[4] = 0;
    debug_info[5] = int(as_type<uint>(prev_bits));
    debug_info[6] = int(as_type<uint>(next_bits));
    for (int i = 7; i < 12; ++i) debug_info[i] = 0;
}

inline void trace_write(device trace_record* records,
                        device atomic_uint* head,
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
    if (!enabled || records == nullptr || head == nullptr || capacity == 0u)
        return;
    uint slot = atomic_fetch_add_explicit(
            head, 1u, memory_order_relaxed);
    slot %= capacity;
    trace_record rec;
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

inline float filter_step_canonical_private(
        float input,
        thread memory_canonical& m,
        const device coefficients_canonical* c) {
    const float a0 = c->a[0];
    const float b0 = c->b[0];
    const float denom0 = fabs(a0) > 1.0e-12f ? a0 : 1.0f;
    const float output = (input * b0 + m.array[0]) / denom0;
    for (int i = 0; i != int(CANONICAL_FILTER_ORDER) - 1; ++i) {
        const float b = c->b[i + 1] * input;
        const float a = c->a[i + 1] * output;
        m.array[i] = b - a + m.array[i + 1];
    }
    const float b_last = c->b[CANONICAL_FILTER_ORDER] * input;
    const float a_last = c->a[CANONICAL_FILTER_ORDER] * output;
    m.array[CANONICAL_FILTER_ORDER - 1] = b_last - a_last;
    return output;
}

inline void ghost_point_pressure_update(uint cookie_a,
                                        float next_pressure,
                                        float prev_pressure,
                                        float inner_pressure,
                                        device boundary_data* bd,
                                        const device coefficients_canonical* boundary,
                                        device atomic_int* error_flag,
                                        device int* debug_info,
                                        uint global_index,
                                        uint boundary_index,
                                        int local_idx,
                                        uint cookie_b) {
    (void)inner_pressure;
    if (cookie_a != 0x12345678u || cookie_b != 0xA5A5A5A5u) {
        flag_error(error_flag, id_outside_range_error);
        if (debug_info) {
            debug_info[0] = -1;
            debug_info[1] = int(cookie_a);
            debug_info[2] = int(cookie_b);
            debug_info[3] = int(global_index);
        }
        return;
    }
    float a0 = boundary->a[0];
    float b0 = boundary->b[0];
    constexpr float kFilterMemoryLimit = 1.0e30f;
    if (!isfinite(a0)) a0 = 1.0f;
    if (!isfinite(b0)) b0 = 1.0f;
    if (fabs(b0) < 1.0e-12f && fabs(a0) < 1.0e-12f) return;
    float filt_state = bd->filter_memory.array[0];
    if (!isfinite(filt_state)) filt_state = 0.0f;
    bool reset_memory = false;
    for (uint k = 0; k != CANONICAL_FILTER_ORDER; ++k) {
        const float value = bd->filter_memory.array[k];
        if (!isfinite(value) || fabs(value) > kFilterMemoryLimit) {
            reset_memory = true;
            break;
        }
    }
    if (reset_memory) {
        record_nan(debug_info,
                   10,
                   global_index,
                   boundary_index,
                   local_idx,
                   bd->coefficient_index,
                   filt_state,
                   a0,
                   b0,
                   0.0f,
                   0.0f,
                   prev_pressure,
                   next_pressure);
        for (uint k = 0; k != CANONICAL_FILTER_ORDER; ++k) {
            bd->filter_memory.array[k] = 0.0f;
        }
        filt_state = 0.0f;
    }

    const float delta = prev_pressure - next_pressure;
    if (delta == 0.0f && filt_state == 0.0f) {
        bd->filter_memory.array[0] = 0.0f;
        return;
    }

    const float safe_b0 = fabs(b0) > 1.0e-12f ? b0 : 1.0f;
    const float denom = fmax(safe_b0 * COURANT, 1.0e-12f);
    const float inv_denom = 1.0f / denom;
    const float diff = fma(a0 * delta, inv_denom, filt_state / safe_b0);
    if (!isfinite(diff)) {
        flag_error(error_flag, id_nan_error);
        record_nan(debug_info,
                   1,
                   global_index,
                   boundary_index,
                   local_idx,
                   bd->coefficient_index,
                   filt_state,
                   a0,
                   b0,
                   diff,
                   0.0f,
                   prev_pressure,
                   next_pressure);
        bd->filter_memory.array[0] = nan(0u);
        return;
    }
    const float filter_input = -diff;
    if (!isfinite(filter_input)) {
        flag_error(error_flag, id_nan_error);
        record_nan(debug_info,
                   2,
                   global_index,
                   boundary_index,
                   local_idx,
                   bd->coefficient_index,
                   filt_state,
                   a0,
                   b0,
                   diff,
                   filter_input,
                   prev_pressure,
                   next_pressure);
        bd->filter_memory.array[0] = nan(0u);
        return;
    }

    memory_canonical mem = bd->filter_memory;
    const float output = filter_step_canonical_private(filter_input, mem, boundary);
    for (uint k = 0; k != CANONICAL_FILTER_ORDER; ++k) {
        bd->filter_memory.array[k] = mem.array[k];
    }
    bd->filter_memory.array[0] = output * safe_b0;
}

} // namespace wayverb_metal
