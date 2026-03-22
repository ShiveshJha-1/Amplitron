#include "gui/pedal_widget.h"
#include "gui/theme.h"
#include <cstring>
#include <cmath>

namespace GuitarAmp {

PedalWidget::PedalWidget(std::shared_ptr<Effect> effect, int index)
    : effect_(std::move(effect)), index_(index) {
    assign_colors();
}

void PedalWidget::assign_colors() {
    // SOLID: Open/Closed — color lookup via theme table, no modification needed to add effects
    const auto* entry = get_effect_color(effect_->name());
    pedal_color_ = entry->pedal_color;
    led_color_ = entry->led_color;
}

bool PedalWidget::render() {
    bool should_remove = false;

    ImGui::PushID(index_);

    float pedal_width = Theme::PEDAL_WIDTH;
    float pedal_height = Theme::PEDAL_HEIGHT;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Pedal body
    ImVec2 p0 = cursor;
    ImVec2 p1 = ImVec2(cursor.x + pedal_width, cursor.y + pedal_height);

    // Shadow
    dl->AddRectFilled(
        ImVec2(p0.x + 4, p0.y + 4),
        ImVec2(p1.x + 4, p1.y + 4),
        Theme::PEDAL_SHADOW, Theme::ROUNDING_MD
    );

    // Body
    ImU32 body_color = ImGui::ColorConvertFloat4ToU32(pedal_color_);
    dl->AddRectFilled(p0, p1, body_color, Theme::ROUNDING_MD);

    // Border
    dl->AddRect(p0, p1, Theme::PEDAL_BORDER, Theme::ROUNDING_MD, 0, 2.0f);

    // Metallic top plate
    ImVec2 plate_p0 = ImVec2(p0.x + 8, p0.y + 8);
    ImVec2 plate_p1 = ImVec2(p1.x - 8, p0.y + 45);
    dl->AddRectFilled(plate_p0, plate_p1,
        Theme::PEDAL_PLATE, Theme::ROUNDING_SM);

    // Effect name
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 12, p0.y + 14));
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextPrimary());
    ImGui::Text("%s", effect_->name());
    ImGui::PopStyleColor();

    // LED indicator
    float led_x = p0.x + pedal_width - 25;
    float led_y = p0.y + 20;
    bool enabled = effect_->is_enabled();
    ImU32 led_col = enabled ?
        ImGui::ColorConvertFloat4ToU32(led_color_) :
        Theme::LED_OFF;
    dl->AddCircleFilled(ImVec2(led_x, led_y), 6, led_col);
    if (enabled) {
        // Glow effect
        dl->AddCircleFilled(ImVec2(led_x, led_y), 10,
            IM_COL32(
                static_cast<int>(led_color_.x * 255),
                static_cast<int>(led_color_.y * 255),
                static_cast<int>(led_color_.z * 255),
                40
            ));
    }

    // Knobs area
    float knob_y_start = p0.y + 55;
    auto& params = effect_->params();
    int num_params = static_cast<int>(params.size());

    float knob_radius = 20.0f;
    float knob_spacing_x = 85.0f;
    float knob_spacing_y = 72.0f;
    float knob_hit_size = knob_radius * 2.2f;

    // Knob arc constants: 270° sweep from bottom-left to bottom-right
    constexpr float PI = 3.14159265f;
    constexpr float TWO_PI = 6.28318530f;
    constexpr float ARC_START = 2.356f;   // 135° (7:30 position)
    constexpr float ARC_RANGE = 4.712f;   // 270° sweep clockwise

    for (int i = 0; i < num_params && i < 6; ++i) {
        int col = i % 2;
        int row = i / 2;
        float kx = p0.x + 15 + col * knob_spacing_x;
        float ky = knob_y_start + row * knob_spacing_y;

        ImVec2 knob_center = ImVec2(kx + knob_spacing_x * 0.5f, ky + knob_radius + 2);

        char label[64];
        snprintf(label, sizeof(label), "##knob_%s_%d_%d", effect_->name(), index_, i);

        // Invisible interaction area centered on knob
        ImGui::SetCursorScreenPos(ImVec2(
            knob_center.x - knob_hit_size * 0.5f,
            knob_center.y - knob_hit_size * 0.5f));
        ImGui::InvisibleButton(label, ImVec2(knob_hit_size, knob_hit_size));

        bool is_hovered = ImGui::IsItemHovered();
        bool is_active = ImGui::IsItemActive();

        // --- Interaction ---
        float range = params[i].max_val - params[i].min_val;

        if (is_active) {
            float mdx = ImGui::GetIO().MouseDelta.x;
            float mdy = ImGui::GetIO().MouseDelta.y;

            if (mdx != 0.0f || mdy != 0.0f) {
                ImVec2 mouse = ImGui::GetIO().MousePos;
                float dx = mouse.x - knob_center.x;
                float dy = mouse.y - knob_center.y;
                float dist = std::sqrt(dx * dx + dy * dy);

                float value_delta = 0.0f;

                if (dist > 5.0f && dist < knob_radius * 5.0f) {
                    // ANGULAR MODE: mouse is near the knob — track rotation
                    // Compute angular delta between previous and current mouse
                    float prev_x = mouse.x - mdx;
                    float prev_y = mouse.y - mdy;
                    float curr_angle = std::atan2(
                        mouse.y - knob_center.y, mouse.x - knob_center.x);
                    float prev_angle = std::atan2(
                        prev_y - knob_center.y, prev_x - knob_center.x);

                    float angle_delta = curr_angle - prev_angle;
                    // Unwrap: keep delta in [-PI, PI]
                    if (angle_delta > PI)  angle_delta -= TWO_PI;
                    if (angle_delta < -PI) angle_delta += TWO_PI;

                    // Clockwise on screen (positive atan2 delta) = increase value
                    value_delta = (angle_delta / ARC_RANGE) * range;
                } else {
                    // LINEAR MODE: mouse far from knob — use vertical drag
                    float sensitivity = 0.007f;
                    value_delta = -mdy * sensitivity * range;
                }

                // Modifiers
                if (ImGui::GetIO().KeyShift) value_delta *= 0.2f;
                if (ImGui::GetIO().KeyCtrl)  value_delta *= 3.0f;

                params[i].value = clamp(params[i].value + value_delta,
                                        params[i].min_val, params[i].max_val);
            }
        }

        // Scroll wheel
        if (is_hovered && std::fabs(ImGui::GetIO().MouseWheel) > 0.0f) {
            float step = range * 0.03f;
            if (ImGui::GetIO().KeyShift) step *= 0.2f;
            params[i].value = clamp(params[i].value + ImGui::GetIO().MouseWheel * step,
                                    params[i].min_val, params[i].max_val);
        }

        // Double-click to reset
        if (is_hovered && ImGui::IsMouseDoubleClicked(0)) {
            params[i].value = params[i].default_val;
        }

        // Right-click for direct input
        if (is_hovered && ImGui::IsMouseClicked(1)) {
            ImGui::OpenPopup(label);
        }
        if (ImGui::BeginPopup(label)) {
            ImGui::Text("%s", params[i].name.c_str());
            ImGui::SetNextItemWidth(120);
            ImGui::SliderFloat("##edit", &params[i].value,
                               params[i].min_val, params[i].max_val, "%.2f");
            if (ImGui::Button("Reset")) {
                params[i].value = params[i].default_val;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // --- Drawing ---
        float normalized = (params[i].value - params[i].min_val) / range;

        // Outer ring / track (colored arc)
        float track_radius = knob_radius + 3;
        int segments = 40;
        for (int s = 0; s < segments; ++s) {
            float t0 = static_cast<float>(s) / segments;
            float t1 = static_cast<float>(s + 1) / segments;
            float a0 = ARC_START + t0 * ARC_RANGE;
            float a1 = ARC_START + t1 * ARC_RANGE;

            bool filled = t0 <= normalized;
            ImU32 seg_color = filled ?
                ImGui::ColorConvertFloat4ToU32(led_color_) :
                Theme::KNOB_TRACK_OFF;

            dl->AddLine(
                ImVec2(knob_center.x + std::cos(a0) * track_radius,
                       knob_center.y + std::sin(a0) * track_radius),
                ImVec2(knob_center.x + std::cos(a1) * track_radius,
                       knob_center.y + std::sin(a1) * track_radius),
                seg_color, 3.0f);
        }

        // Knob body (highlights on hover/active)
        ImU32 knob_bg = is_active ? Theme::KNOB_ACTIVE :
                        is_hovered ? Theme::KNOB_HOVER :
                                     Theme::KNOB_FACE;
        dl->AddCircleFilled(knob_center, knob_radius, Theme::KNOB_BG);
        dl->AddCircleFilled(knob_center, knob_radius - 1, knob_bg);

        // Pointer line (rotates with value)
        float pointer_angle = ARC_START + normalized * ARC_RANGE;
        float ptr_inner = knob_radius * 0.25f;
        float ptr_outer = knob_radius - 3.0f;
        ImVec2 ptr_from = ImVec2(
            knob_center.x + std::cos(pointer_angle) * ptr_inner,
            knob_center.y + std::sin(pointer_angle) * ptr_inner);
        ImVec2 ptr_to = ImVec2(
            knob_center.x + std::cos(pointer_angle) * ptr_outer,
            knob_center.y + std::sin(pointer_angle) * ptr_outer);

        ImU32 ptr_color = is_active ?
            Theme::ACCENT_GOLD_HOT :
            Theme::ACCENT_GOLD;
        dl->AddLine(ptr_from, ptr_to, ptr_color, 2.5f);
        dl->AddCircleFilled(ptr_to, 3.0f, ptr_color);

        // Tooltip
        if (is_hovered || is_active) {
            ImGui::SetTooltip("%s: %.2f %s\nRotate or drag to adjust\nScroll wheel also works\nShift=fine  Ctrl=coarse\nDbl-click=reset  Right-click=edit",
                params[i].name.c_str(), params[i].value, params[i].unit.c_str());
        }

        // Parameter name below knob (centered)
        const char* pname = params[i].name.c_str();
        ImVec2 text_size = ImGui::CalcTextSize(pname);
        ImGui::SetCursorScreenPos(ImVec2(
            knob_center.x - text_size.x * 0.5f,
            knob_center.y + knob_radius + 8));
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextSecondary());
        ImGui::TextUnformatted(pname);
        ImGui::PopStyleColor();

        // Value text above knob (centered)
        char val_buf[32];
        snprintf(val_buf, sizeof(val_buf), "%.1f", params[i].value);
        ImVec2 val_size = ImGui::CalcTextSize(val_buf);
        ImGui::SetCursorScreenPos(ImVec2(
            knob_center.x - val_size.x * 0.5f,
            knob_center.y - knob_radius - 14));
        ImGui::PushStyleColor(ImGuiCol_Text,
            is_active ? Theme::GoldHot() :
                        Theme::TextDim());
        ImGui::TextUnformatted(val_buf);
        ImGui::PopStyleColor();
    }

    // Footswitch (toggle on/off)
    float switch_y = p0.y + pedal_height - 55;
    float switch_x = p0.x + (pedal_width - 50) / 2;
    ImGui::SetCursorScreenPos(ImVec2(switch_x, switch_y));

    // Draw footswitch
    ImVec2 sw_center = ImVec2(switch_x + 25, switch_y + 15);
    dl->AddCircleFilled(sw_center, 18, Theme::SWITCH_BODY);
    dl->AddCircle(sw_center, 18, Theme::SWITCH_RING, 0, 2.0f);
    dl->AddCircleFilled(sw_center, 12,
        enabled ? Theme::SWITCH_ACTIVE : Theme::SWITCH_IDLE);

    ImGui::InvisibleButton("##switch", ImVec2(50, 30));
    if (ImGui::IsItemClicked()) {
        effect_->set_enabled(!enabled);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(enabled ? "Click to bypass" : "Click to enable");
    }

    // Remove button (small X at top-right)
    ImGui::SetCursorScreenPos(ImVec2(p1.x - 22, p0.y + 2));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.1f, 0.1f, 0.8f));
    char remove_label[32];
    snprintf(remove_label, sizeof(remove_label), "X##rm%d", index_);
    if (ImGui::SmallButton(remove_label)) {
        should_remove = true;
    }
    ImGui::PopStyleColor(2);

    // Advance cursor for next pedal
    ImGui::SetCursorScreenPos(ImVec2(p0.x + pedal_width + 15, cursor.y));

    ImGui::PopID();
    return should_remove;
}

} // namespace GuitarAmp
