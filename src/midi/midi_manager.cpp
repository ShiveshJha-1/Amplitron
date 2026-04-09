#include "midi/midi_manager.h"
#include "audio/audio_engine.h"
#include <rtmidi/RtMidi.h>

#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace Amplitron {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

MidiManager::MidiManager() = default;

MidiManager::~MidiManager() {
    shutdown();
}

// ---------------------------------------------------------------------------
// RtMidi callback — runs on RtMidi's internal thread, must be lock-free
// ---------------------------------------------------------------------------

void MidiManager::midi_callback(double /*timestamp*/,
                                std::vector<unsigned char>* message,
                                void* user_data) {
    if (!message || message->size() < 3) return;

    auto* self = static_cast<MidiManager*>(user_data);
    uint8_t status = (*message)[0];

    // Only handle Control Change messages (0xB0 .. 0xBF)
    if ((status & 0xF0) != 0xB0) return;

    MidiEvent event{};
    event.status = status;
    event.data1  = (*message)[1];  // CC number
    event.data2  = (*message)[2];  // CC value
    self->midi_queue_.try_push(event);  // Drop if full — acceptable for CC
}

// ---------------------------------------------------------------------------
// Port management
// ---------------------------------------------------------------------------

bool MidiManager::initialize() {
    if (midi_in_) return true;  // Already initialized

    try {
        auto* rt = new RtMidiIn(RtMidi::UNSPECIFIED, "Amplitron MIDI");
        rt->ignoreTypes(true, true, true);  // Ignore SysEx, timing, active sensing
        midi_in_ = rt;
    } catch (const RtMidiError& e) {
        std::cerr << "[MidiManager] RtMidi init failed: " << e.getMessage() << "\n";
        return false;
    }

    // Auto-open the first available port (if any)
    auto ports = get_available_ports();
    if (!ports.empty()) {
        open_port(0);
    }
    return true;
}

void MidiManager::shutdown() {
    close_port();
    if (midi_in_) {
        delete static_cast<RtMidiIn*>(midi_in_);
        midi_in_ = nullptr;
    }
}

std::vector<std::string> MidiManager::get_available_ports() const {
    std::vector<std::string> result;
    if (!midi_in_) return result;

    auto* rt = static_cast<RtMidiIn*>(midi_in_);
    unsigned int count = rt->getPortCount();
    result.reserve(count);
    for (unsigned int i = 0; i < count; ++i) {
        result.push_back(rt->getPortName(i));
    }
    return result;
}

bool MidiManager::open_port(int port_index) {
    if (!midi_in_) return false;

    close_port();

    auto* rt = static_cast<RtMidiIn*>(midi_in_);
    unsigned int count = rt->getPortCount();
    if (port_index < 0 || static_cast<unsigned int>(port_index) >= count) return false;

    try {
        rt->setCallback(&MidiManager::midi_callback, this);
        rt->openPort(static_cast<unsigned int>(port_index), "Amplitron In");
        current_port_ = port_index;
        current_port_name_ = rt->getPortName(static_cast<unsigned int>(port_index));
        return true;
    } catch (const RtMidiError& e) {
        std::cerr << "[MidiManager] Failed to open port " << port_index
                  << ": " << e.getMessage() << "\n";
        current_port_ = -1;
        current_port_name_.clear();
        return false;
    }
}

void MidiManager::close_port() {
    if (!midi_in_ || current_port_ < 0) return;

    auto* rt = static_cast<RtMidiIn*>(midi_in_);
    try {
        rt->cancelCallback();
        rt->closePort();
    } catch (...) {}
    current_port_ = -1;
    current_port_name_.clear();
}

// ---------------------------------------------------------------------------
// Mapping management
// ---------------------------------------------------------------------------

void MidiManager::add_mapping(const MidiMapping& mapping) {
    // Remove any existing mapping with the same CC + channel
    for (auto it = mappings_.begin(); it != mappings_.end(); ++it) {
        if (it->cc_number == mapping.cc_number &&
            it->midi_channel == mapping.midi_channel) {
            mappings_.erase(it);
            break;
        }
    }
    mappings_.push_back(mapping);
}

void MidiManager::remove_mapping(int index) {
    if (index >= 0 && index < static_cast<int>(mappings_.size())) {
        mappings_.erase(mappings_.begin() + index);
    }
}

void MidiManager::clear_mappings() {
    mappings_.clear();
}

void MidiManager::install_default_mappings() {
    MidiMapping cc7;
    cc7.cc_number = 7;
    cc7.midi_channel = -1;
    cc7.target_type = MidiTargetType::OutputGain;
    cc7.mode = MidiMappingMode::Continuous;
    add_mapping(cc7);

    MidiMapping cc11;
    cc11.cc_number = 11;
    cc11.midi_channel = -1;
    cc11.target_type = MidiTargetType::InputGain;
    cc11.mode = MidiMappingMode::Continuous;
    add_mapping(cc11);

    MidiMapping cc64;
    cc64.cc_number = 64;
    cc64.midi_channel = -1;
    cc64.target_type = MidiTargetType::EffectBypass;
    cc64.mode = MidiMappingMode::Toggle;
    cc64.effect_name = "AmpSimulator";
    add_mapping(cc64);

    MidiMapping cc74;
    cc74.cc_number = 74;
    cc74.midi_channel = -1;
    cc74.target_type = MidiTargetType::EffectParam;
    cc74.mode = MidiMappingMode::Continuous;
    cc74.effect_name = "WahPedal";
    cc74.param_name = "Sweep";
    add_mapping(cc74);
}

// ---------------------------------------------------------------------------
// MIDI Learn
// ---------------------------------------------------------------------------

void MidiManager::start_learn(MidiTargetType type,
                              const std::string& effect_name,
                              const std::string& param_name) {
    learn_active_ = true;
    learn_target_type_ = type;
    learn_effect_name_ = effect_name;
    learn_param_name_ = param_name;
}

void MidiManager::cancel_learn() {
    learn_active_ = false;
    learn_effect_name_.clear();
    learn_param_name_.clear();
}

std::string MidiManager::learn_status() const {
    if (!learn_active_) return "";

    std::string target;
    switch (learn_target_type_) {
        case MidiTargetType::EffectParam:
            target = learn_effect_name_ + " > " + learn_param_name_;
            break;
        case MidiTargetType::EffectBypass:
            target = learn_effect_name_ + " (bypass)";
            break;
        case MidiTargetType::InputGain:
            target = "Input Gain";
            break;
        case MidiTargetType::OutputGain:
            target = "Output Gain";
            break;
    }
    return "MIDI Learn: move a CC for \"" + target + "\"...";
}

// ---------------------------------------------------------------------------
// Poll — called from GUI thread each frame
// ---------------------------------------------------------------------------

void MidiManager::inject_event(const MidiEvent& event) {
    midi_queue_.try_push(event);
}

void MidiManager::poll(AudioEngine& engine) {
    MidiEvent event{};
    while (midi_queue_.try_pop(event)) {
        uint8_t cc_number = event.data1;
        uint8_t cc_value  = event.data2;
        int channel = event.status & 0x0F;

        // MIDI Learn: capture the first CC and create a mapping
        if (learn_active_) {
            MidiMapping mapping;
            mapping.cc_number = cc_number;
            mapping.midi_channel = channel;
            mapping.target_type = learn_target_type_;
            mapping.mode = (learn_target_type_ == MidiTargetType::EffectBypass)
                             ? MidiMappingMode::Toggle
                             : MidiMappingMode::Continuous;
            mapping.effect_name = learn_effect_name_;
            mapping.param_name  = learn_param_name_;
            add_mapping(mapping);
            learn_active_ = false;
            continue;
        }

        // Normal mode: resolve mapping and apply
        for (const auto& m : mappings_) {
            if (m.cc_number != cc_number) continue;
            if (m.midi_channel >= 0 && m.midi_channel != channel) continue;
            apply_mapping(m, cc_value, engine);
        }
    }
}

void MidiManager::apply_mapping(const MidiMapping& mapping, int cc_value,
                                AudioEngine& engine) {
    float normalized = static_cast<float>(cc_value) / 127.0f;

    switch (mapping.target_type) {
        case MidiTargetType::InputGain: {
            // Map 0-127 to 0.0-2.0 (same range as GUI gain knob)
            float gain = normalized * 2.0f;
            engine.set_input_gain(gain);
            break;
        }
        case MidiTargetType::OutputGain: {
            float gain = normalized * 2.0f;
            engine.set_output_gain(gain);
            break;
        }
        case MidiTargetType::EffectBypass: {
            // Find the effect by name
            auto& effects = engine.effects();
            for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
                if (effects[i]->name() == mapping.effect_name) {
                    bool enabled = (mapping.mode == MidiMappingMode::Toggle)
                                     ? (cc_value >= 64)
                                     : (normalized > 0.5f);
                    effects[i]->set_enabled(enabled);
                    engine.push_effect_enabled(i, enabled ? 1.0f : 0.0f);
                    break;
                }
            }
            break;
        }
        case MidiTargetType::EffectParam: {
            // Find the effect by name, then the param by name
            auto& effects = engine.effects();
            for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
                if (effects[i]->name() != mapping.effect_name) continue;

                auto& params = effects[i]->params();
                for (int p = 0; p < static_cast<int>(params.size()); ++p) {
                    if (params[p].name != mapping.param_name) continue;

                    float value = params[p].min_val +
                                  normalized * (params[p].max_val - params[p].min_val);
                    params[p].value = value;  // GUI sync
                    engine.push_param_change(i, p, value);  // Audio sync
                    break;
                }
                break;  // Only map to the first matching effect
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Persistence — midi_config.json
// ---------------------------------------------------------------------------

std::string MidiManager::get_config_path() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) return "midi_config.json";
    std::string dir = std::string(appdata) + "\\Amplitron";
    std::filesystem::create_directories(dir);
    return dir + "\\midi_config.json";
#else
    const char* home = std::getenv("HOME");
    if (!home) return "midi_config.json";
    std::string config_dir = std::string(home) + "/.config/amplitron";
    std::filesystem::create_directories(config_dir);
    return config_dir + "/midi_config.json";
#endif
}

static void escape_json(std::ostream& os, const std::string& s) {
    for (char c : s) {
        if (c == '\\') os << "\\\\";
        else if (c == '"') os << "\\\"";
        else if (c == '\n') os << "\\n";
        else os << c;
    }
}

std::string MidiManager::mappings_to_json() const {
    std::ostringstream os;
    os << "{\n  \"mappings\": [\n";

    for (size_t i = 0; i < mappings_.size(); ++i) {
        const auto& m = mappings_[i];
        os << "    {\n";
        os << "      \"cc\": " << m.cc_number << ",\n";
        os << "      \"channel\": " << m.midi_channel << ",\n";
        os << "      \"target\": " << static_cast<int>(m.target_type) << ",\n";
        os << "      \"mode\": " << static_cast<int>(m.mode) << ",\n";
        os << "      \"effect\": \""; escape_json(os, m.effect_name); os << "\",\n";
        os << "      \"param\": \""; escape_json(os, m.param_name); os << "\"\n";
        os << "    }";
        if (i + 1 < mappings_.size()) os << ",";
        os << "\n";
    }

    os << "  ]\n}\n";
    return os.str();
}

// Minimal JSON parsing helpers (matches project convention — no external JSON lib)
static std::string extract_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    size_t colon = json.find(':', pos + search.size());
    if (colon == std::string::npos) return "";

    size_t q1 = json.find('"', colon + 1);
    if (q1 == std::string::npos) return "";

    std::string result;
    for (size_t i = q1 + 1; i < json.size() && json[i] != '"'; ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            ++i;
            if (json[i] == '\\') result += '\\';
            else if (json[i] == '"') result += '"';
            else if (json[i] == 'n') result += '\n';
            else { result += '\\'; result += json[i]; }
        } else {
            result += json[i];
        }
    }
    return result;
}

static int extract_int(const std::string& json, const std::string& key, int def = 0) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return def;

    size_t colon = json.find(':', pos + search.size());
    if (colon == std::string::npos) return def;

    size_t start = colon + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) ++start;

    try {
        return std::stoi(json.substr(start));
    } catch (...) {
        return def;
    }
}

bool MidiManager::mappings_from_json(const std::string& json) {
    mappings_.clear();

    // Find "mappings" array
    size_t arr_start = json.find("\"mappings\"");
    if (arr_start == std::string::npos) return false;

    size_t bracket = json.find('[', arr_start);
    if (bracket == std::string::npos) return false;

    // Parse each object in the array
    size_t pos = bracket + 1;
    while (pos < json.size()) {
        size_t obj_start = json.find('{', pos);
        if (obj_start == std::string::npos) break;

        size_t obj_end = json.find('}', obj_start);
        if (obj_end == std::string::npos) break;

        std::string obj = json.substr(obj_start, obj_end - obj_start + 1);

        MidiMapping m;
        m.cc_number = extract_int(obj, "cc", 0);
        m.midi_channel = extract_int(obj, "channel", -1);
        m.target_type = static_cast<MidiTargetType>(extract_int(obj, "target", 0));
        m.mode = static_cast<MidiMappingMode>(extract_int(obj, "mode", 0));
        m.effect_name = extract_string(obj, "effect");
        m.param_name = extract_string(obj, "param");
        mappings_.push_back(m);

        pos = obj_end + 1;
    }

    return !mappings_.empty();
}

void MidiManager::save_config() const {
    std::string path = get_config_path();
    std::ofstream f(path);
    if (!f.is_open()) return;
    f << mappings_to_json();
}

void MidiManager::load_config() {
    std::string path = get_config_path();
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    mappings_from_json(content);
}

} // namespace Amplitron
