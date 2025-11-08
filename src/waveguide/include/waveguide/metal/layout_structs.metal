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
    uint _pad[7];
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
    uint off_b3_data0;
    uint off_b3_data1;
    uint off_b3_data2;
};

} // namespace wayverb_metal

