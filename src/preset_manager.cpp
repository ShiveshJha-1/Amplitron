#include "preset_manager.h"
#include "preset_manager_impl.h"

#include <iostream>
#include <ctime>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define MKDIR(path) _mkdir(path)
#define STAT_STRUCT struct _stat
#define STAT_FN     _stat
#elif defined(__APPLE__)
#include <dirent.h>
#include <mach-o/dyld.h>
#define MKDIR(path) mkdir(path, 0755)
#define STAT_STRUCT struct stat
#define STAT_FN     stat
#else
#include <dirent.h>
#define MKDIR(path) mkdir(path, 0755)
#define STAT_STRUCT struct stat
#define STAT_FN     stat
#endif

namespace Amplitron {

std::string PresetManager::last_error_;
std::string PresetManager::custom_presets_dir_;

bool dir_exists(const std::string& path) {
    STAT_STRUCT st;
    return STAT_FN(path.c_str(), &st) == 0 &&
#ifdef _WIN32
           (st.st_mode & _S_IFDIR);
#else
           S_ISDIR(st.st_mode);
#endif
}

std::string PresetManager::get_system_presets_dir() {
#ifdef _WIN32
    const char* pd = std::getenv("ProgramData");
    return pd ? std::string(pd) + "\\Amplitron\\presets" : "";
#elif defined(__APPLE__)
    return "/Library/Application Support/Amplitron/presets";
#else
    return "/usr/share/amplitron/presets";
#endif
}

std::string PresetManager::get_config_path() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) return "amplitron_config.json";
    std::string dir = std::string(appdata) + "\\Amplitron";
    MKDIR(dir.c_str());
    return dir + "\\config.json";
#else
    const char* home = std::getenv("HOME");
    if (!home) return "amplitron_config.json";
    std::string config_dir = std::string(home) + "/.config/amplitron";
    std::filesystem::create_directories(config_dir);
    return config_dir + "/config.json";
#endif
}

std::string get_user_presets_dir() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) return "";
    return std::string(appdata) + "\\Amplitron\\presets";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/Library/Application Support/Amplitron/presets";
#else
    const char* home = std::getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/.config/amplitron/presets";
#endif
}

} // namespace Amplitron
