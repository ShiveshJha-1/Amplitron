#include "midi/midi_manager.h"

#include <fstream>
#include <sstream>
#include <filesystem>

namespace Amplitron {

// ---------------------------------------------------------------------------
// Persistence — midi_config.json
// ---------------------------------------------------------------------------

std::string MidiManager::get_config_path() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) return "midi_config.json";
    std::string dir = std::string(appdata) + "\\Amplitron";
    try {
        std::filesystem::create_directories(dir);
    } catch (...) {
        // Ignore errors — will fall back to local path if creation fails
    }
    return dir + "\\midi_config.json";
#else
    const char* home = std::getenv("HOME");
    if (!home) return "midi_config.json";
    std::string config_dir = std::string(home) + "/.config/amplitron";
    try {
        std::filesystem::create_directories(config_dir);
    } catch (...) {
        // Ignore errors — will fall back to local path if creation fails
    }
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

    return true;  // Successfully parsed (even if no mappings)
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
