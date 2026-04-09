#pragma once
#include <string>
#include "preset_manager.h"

namespace Amplitron {
    std::string escape_json_string_ext(const std::string& s);
    std::string unescape_json_string_ext(const std::string& s);
    std::string to_json_ext(const PresetData& preset);
    bool from_json_ext(const std::string& json, PresetData& preset);
}
