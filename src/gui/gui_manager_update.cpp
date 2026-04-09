#include "gui/gui_manager.h"
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>

namespace Amplitron {

void GuiManager::check_for_updates() {
#ifndef AMPLITRON_NO_DESKTOP_SHELL
    FILE* pipe = nullptr;
#ifdef _WIN32
    pipe = _popen("curl -s --fail --connect-timeout 5 --max-time 10 https://api.github.com/repos/sudip-mondal-2002/Amplitron/releases", "r");
#else
    pipe = popen("curl -s --fail --connect-timeout 5 --max-time 10 https://api.github.com/repos/sudip-mondal-2002/Amplitron/releases", "r");
#endif

    if (!pipe) return;

    std::string result = "";
    char buffer[256];
    while (!feof(pipe)) {
        if (fgets(buffer, 256, pipe) != nullptr)
            result += buffer;
    }
#ifdef _WIN32
    int ret = _pclose(pipe);
#else
    int ret = pclose(pipe);
#endif

    // If curl failed, discard the result and return
    if (ret != 0) return;

    std::string search_str = "\"tag_name\": \"";
    size_t pos = result.find(search_str);
    if (pos != std::string::npos) {
        pos += search_str.length();
        size_t end_pos = result.find("\"", pos);
        if (end_pos != std::string::npos) {
            std::string latest_version = result.substr(pos, end_pos - pos);

            std::string html_url = "";
            std::string url_search_str = "\"html_url\": \"";
            size_t url_pos = result.find(url_search_str);
            if (url_pos != std::string::npos) {
                url_pos += url_search_str.length();
                size_t url_end_pos = result.find("\"", url_pos);
                if (url_end_pos != std::string::npos) {
                    html_url = result.substr(url_pos, url_end_pos - url_pos);
                }
            }

            auto parse_version = [](const std::string& v) -> std::vector<int> {
                std::vector<int> parts;
                std::string s = v;
                if (!s.empty() && s[0] == 'v') s = s.substr(1);
                size_t pos = 0;
                while (pos < s.size()) {
                    size_t dot = s.find('.', pos);
                    if (dot == std::string::npos) dot = s.size();
                    try { parts.push_back(std::stoi(s.substr(pos, dot - pos))); }
                    catch (...) { parts.push_back(0); }
                    pos = dot + 1;
                }
                return parts;
            };

            std::string current_version = "v" AMPLITRON_VERSION;
            if (!latest_version.empty()) {
                auto latest_parts = parse_version(latest_version);
                auto current_parts = parse_version(current_version);
                bool is_newer = false;
                size_t max_len = std::max(latest_parts.size(), current_parts.size());
                for (size_t i = 0; i < max_len; ++i) {
                    int lv = (i < latest_parts.size()) ? latest_parts[i] : 0;
                    int cv = (i < current_parts.size()) ? current_parts[i] : 0;
                    if (lv > cv) { is_newer = true; break; }
                    if (lv < cv) { break; }
                }
                if (is_newer) {
                    std::lock_guard<std::mutex> lock(update_mutex_);
                    new_release_version_ = latest_version;
                    new_release_url_ = html_url;
                    has_new_release_ = true;
                }
            }
        }
    }
#endif
}

} // namespace Amplitron
