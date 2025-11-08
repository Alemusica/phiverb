#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string read_file(const std::string& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Unable to open scene file: " + path);
    }
    std::string content((std::istreambuf_iterator<char>(stream)),
                        std::istreambuf_iterator<char>());
    return content;
}

bool extract_number(std::string_view text,
                    std::string_view key,
                    double& out) {
    const auto pos = text.find(key);
    if (pos == std::string_view::npos) {
        return false;
    }
    auto colon = text.find(':', pos);
    if (colon == std::string_view::npos) {
        return false;
    }
    colon += 1;
    while (colon < text.size() && std::isspace(text[colon])) {
        ++colon;
    }
    auto end = colon;
    while (end < text.size()) {
        const char c = text[end];
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' ||
            c == 'e' || c == 'E') {
            ++end;
        } else {
            break;
        }
    }
    try {
        out = std::stod(std::string{text.substr(colon, end - colon)});
    } catch (...) {
        return false;
    }
    return true;
}

struct SceneInfo {
    double volume = 0;
    double surface = 0;
    double alpha = 0;
    double air_m = 0;
};

SceneInfo parse_scene(const std::string& path) {
    const auto content = read_file(path);
    SceneInfo info{};
    if (!extract_number(content, "\"volume_m3\"", info.volume)) {
        throw std::runtime_error("Scene missing 'volume_m3'.");
    }
    if (!extract_number(content, "\"surface_m2\"", info.surface)) {
        throw std::runtime_error("Scene missing 'surface_m2'.");
    }
    if (!extract_number(content, "\"alpha_bar_1k\"", info.alpha)) {
        throw std::runtime_error("Scene missing 'alpha_bar_1k'.");
    }
    extract_number(content, "\"air_m_nepers_per_m\"", info.air_m);
    return info;
}

double sabine(double V, double S, double alpha) {
    const double A = std::max(alpha * S, 1e-6);
    return 0.161 * V / A;
}

double eyring(double V, double S, double alpha) {
    const double a = std::clamp(alpha, 1e-6, 1.0 - 1e-6);
    return 0.161 * V / (-S * std::log(1.0 - a));
}

double norris_eyring(double V, double S, double alpha, double m_air) {
    const double a = std::clamp(alpha, 1e-6, 1.0 - 1e-6);
    const double denom = -S * std::log(1.0 - a) + 4.0 * m_air * V;
    return 0.161 * V / std::max(denom, 1e-6);
}

std::vector<float> synthesize_ir(double fs,
                                 double target_rt,
                                 double duration_seconds) {
    const size_t samples = static_cast<size_t>(duration_seconds * fs);
    std::vector<float> data(samples, 0.0f);
    std::mt19937 rng(1337);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    const double log_decay = std::log(1000.0);  // 60 dB.
    const double denom = std::max(target_rt, 0.1);
    const double factor = log_decay / denom;

    for (size_t i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / fs;
        const double env = std::exp(-factor * t);
        data[i] = static_cast<float>(env * dist(rng));
    }
    return data;
}

void write_wav(const std::string& path,
               const std::vector<float>& data,
               double sample_rate) {
    const int32_t sr = static_cast<int32_t>(sample_rate);
    const uint16_t channels = 1;
    const uint16_t bits_per_sample = 16;
    const uint32_t byte_rate = sr * channels * bits_per_sample / 8;
    const uint16_t block_align = channels * bits_per_sample / 8;

    const uint32_t data_size = static_cast<uint32_t>(data.size()) * block_align;
    const uint32_t chunk_size = 36 + data_size;

#pragma pack(push, 1)
    struct WavHeader {
        char riff[4] = {'R', 'I', 'F', 'F'};
        uint32_t chunk_size;
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4] = {'f', 'm', 't', ' '};
        uint32_t subchunk1_size = 16;
        uint16_t audio_format = 1;
        uint16_t num_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
        char data_id[4] = {'d', 'a', 't', 'a'};
        uint32_t data_size;
    } header;
#pragma pack(pop)

    header.chunk_size = chunk_size;
    header.num_channels = channels;
    header.sample_rate = sr;
    header.byte_rate = byte_rate;
    header.block_align = block_align;
    header.bits_per_sample = bits_per_sample;
    header.data_size = data_size;

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Unable to write output file: " + path);
    }
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    for (float sample : data) {
        sample = std::clamp(sample, -1.0f, 1.0f);
        const int16_t pcm = static_cast<int16_t>(sample * 32767.0f);
        out.write(reinterpret_cast<const char*>(&pcm), sizeof(pcm));
    }
}

struct Args {
    std::string scene_path;
    std::string out_path;
    double sample_rate = 48000.0;
};

Args parse_args(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--scene" && i + 1 < argc) {
            args.scene_path = argv[++i];
        } else if (arg == "--out" && i + 1 < argc) {
            args.out_path = argv[++i];
        } else if (arg == "--sample-rate" && i + 1 < argc) {
            args.sample_rate = std::stod(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: wayverb_cli --scene file.json --out output.wav"
                      << " [--sample-rate SR]\n";
            std::exit(0);
        }
    }
    if (args.scene_path.empty() || args.out_path.empty()) {
        throw std::runtime_error("Missing --scene or --out argument.");
    }
    return args;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto args = parse_args(argc, argv);
        const auto scene = parse_scene(args.scene_path);

        const double rt_sabine = sabine(scene.volume, scene.surface, scene.alpha);
        const double rt_eyring = eyring(scene.volume, scene.surface, scene.alpha);
        const double rt_norris = norris_eyring(
                scene.volume, scene.surface, scene.alpha, scene.air_m);
        const double target_rt = std::max({rt_sabine, rt_eyring, rt_norris});

        const double min_duration = std::max(1.0, target_rt * 3.0);
        auto ir = synthesize_ir(args.sample_rate, target_rt, min_duration);
        write_wav(args.out_path, ir, args.sample_rate);
        std::cout << "Generated IR (RT≈" << target_rt << " s) -> "
                  << args.out_path << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "wayverb_cli error: " << e.what() << "\n";
        return 1;
    }
}
