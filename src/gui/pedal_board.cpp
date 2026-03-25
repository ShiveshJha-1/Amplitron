#include "gui/pedal_board.h"
#include "gui/pedal_widget.h"
#include "gui/theme.h"
#include "gui/command.h"

#include "audio/effects/noise_gate.h"
#include "audio/effects/compressor.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/distortion.h"
#include "audio/effects/equalizer.h"
#include "audio/effects/chorus.h"
#include "audio/effects/delay.h"
#include "audio/effects/reverb.h"
#include "audio/effects/cabinet_sim.h"
#include "audio/effects/amp_simulator.h"

#include <imgui.h>
#include <algorithm>

namespace GuitarAmp {

/** @brief Construct PedalBoard and build initial widget list from engine state. */
PedalBoard::PedalBoard(AudioEngine& engine, CommandHistory& history)
    : engine_(engine), history_(history) {
    rebuild_widgets();
}

/** @brief Default destructor. */
PedalBoard::~PedalBoard() = default;

/** @brief Recreate PedalWidget list to match the engine's current effect chain. */
void PedalBoard::rebuild_widgets() {
    widgets_.clear();
    auto& effects = engine_.effects();
    for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
        auto w = std::make_unique<PedalWidget>(effects[i], i);
        w->set_history(&history_);
        widgets_.push_back(std::move(w));
    }
}

/** @brief Render the toolbar (add/reset) and the scrollable signal chain area. */
void PedalBoard::render() {
    ImGui::BeginChild("PedalToolbar", ImVec2(0, 35), true);
    render_add_pedal_menu();
    ImGui::SameLine();

    if (ImGui::Button("Reset All")) {
        for (auto& fx : engine_.effects()) {
            fx->reset();
            auto& p = fx->params();
            for (auto& param : p) {
                param.value = param.default_val;
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextColored(Theme::TextSecondary(),
        "  Signal Chain: %d pedals | Drag knobs to adjust | Double-click to reset knob",
        static_cast<int>(engine_.effects().size()));

    ImGui::EndChild();

    // Pedal board area with horizontal scroll
    ImGui::BeginChild("PedalArea", ImVec2(0, 0), true,
        ImGuiWindowFlags_HorizontalScrollbar);

    render_signal_chain();

    ImGui::EndChild();
}

/** @brief Render the "+ Add Pedal" button and category popup with effect menu items. */
void PedalBoard::render_add_pedal_menu() {
    if (ImGui::Button("+ Add Pedal")) {
        ImGui::OpenPopup("AddPedalPopup");
    }

    if (ImGui::BeginPopup("AddPedalPopup")) {
        ImGui::TextColored(Theme::Gold(), "DRIVE");
        if (ImGui::MenuItem("Overdrive")) {
            history_.execute(std::make_unique<AddEffectCommand>(engine_, std::make_shared<Overdrive>()));
            rebuild_widgets();
        }
        if (ImGui::MenuItem("Distortion")) {
            history_.execute(std::make_unique<AddEffectCommand>(engine_, std::make_shared<Distortion>()));
            rebuild_widgets();
        }

        ImGui::Separator();
        ImGui::TextColored(Theme::Live(), "DYNAMICS");
        if (ImGui::MenuItem("Noise Gate")) {
            history_.execute(std::make_unique<AddEffectCommand>(engine_, std::make_shared<NoiseGate>()));
            rebuild_widgets();
        }
        if (ImGui::MenuItem("Compressor")) {
            history_.execute(std::make_unique<AddEffectCommand>(engine_, std::make_shared<Compressor>()));
            rebuild_widgets();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.35f, 0.60f, 0.95f, 1.0f), "MODULATION");
        if (ImGui::MenuItem("Chorus")) {
            history_.execute(std::make_unique<AddEffectCommand>(engine_, std::make_shared<Chorus>()));
            rebuild_widgets();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.65f, 0.35f, 0.95f, 1.0f), "TIME");
        if (ImGui::MenuItem("Delay")) {
            history_.execute(std::make_unique<AddEffectCommand>(engine_, std::make_shared<Delay>()));
            rebuild_widgets();
        }
        if (ImGui::MenuItem("Reverb")) {
            history_.execute(std::make_unique<AddEffectCommand>(engine_, std::make_shared<Reverb>()));
            rebuild_widgets();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.20f, 1.0f), "AMP MODELS");
        {
            const auto& models = get_amp_models();
            for (int m = 0; m < static_cast<int>(models.size()); ++m) {
                char label[64];
                std::snprintf(label, sizeof(label), "Amp Sim: %s", models[m].name);
                if (ImGui::MenuItem(label)) {
                    auto amp = std::make_shared<AmpSimulator>();
                    amp->params()[0].value = static_cast<float>(m);
                    history_.execute(std::make_unique<AddEffectCommand>(engine_, amp));
                    rebuild_widgets();
                }
            }
        }

        ImGui::Separator();
        ImGui::TextColored(Theme::GoldDim(), "TONE");
        if (ImGui::MenuItem("Equalizer")) {
            history_.execute(std::make_unique<AddEffectCommand>(engine_, std::make_shared<Equalizer>()));
            rebuild_widgets();
        }
        if (ImGui::MenuItem("Cabinet Sim")) {
            history_.execute(std::make_unique<AddEffectCommand>(engine_, std::make_shared<CabinetSim>()));
            rebuild_widgets();
        }

        ImGui::EndPopup();
    }
}

/** @brief Draw the signal flow line, render each pedal widget, and handle drag-and-drop reordering. */
void PedalBoard::render_signal_chain() {
    if (widgets_.empty()) {
        ImGui::SetCursorPos(ImVec2(
            ImGui::GetWindowWidth() / 2 - 150,
            ImGui::GetWindowHeight() / 2 - 30
        ));
        ImGui::TextColored(Theme::TextDim(),
            "No pedals in chain.\nClick '+ Add Pedal' to get started.");
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    // Draw signal flow line
    float line_y = origin.y + 160;
    float total_width = widgets_.size() * 195.0f + 40;
    dl->AddLine(
        ImVec2(origin.x, line_y),
        ImVec2(origin.x + total_width, line_y),
        Theme::CHAIN_LINE, 3.0f
    );

    // Input jack
    dl->AddCircleFilled(ImVec2(origin.x + 5, line_y), 6, Theme::CHAIN_JACK);
    dl->AddCircle(ImVec2(origin.x + 5, line_y), 6, Theme::BORDER_DARK, 0, 1.5f);

    // Render each pedal
    float pedal_x = origin.x + 20;
    int remove_idx = -1;

    for (int i = 0; i < static_cast<int>(widgets_.size()); ++i) {
        ImGui::SetCursorScreenPos(ImVec2(pedal_x, origin.y + 5));

        if (widgets_[i]->render()) {
            remove_idx = i;
        }

        // Drag-and-drop reordering
        // Use the full pedal rect as source/target
        ImVec2 pedal_min = ImVec2(pedal_x, origin.y + 5);
        ImVec2 pedal_max = ImVec2(pedal_x + Theme::PEDAL_WIDTH, origin.y + 5 + Theme::PEDAL_HEIGHT);
        ImGui::SetCursorScreenPos(pedal_min);
        char dnd_id[32];
        snprintf(dnd_id, sizeof(dnd_id), "##dnd_%d", i);
        ImGui::InvisibleButton(dnd_id, ImVec2(Theme::PEDAL_WIDTH, Theme::PEDAL_HEIGHT));

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("PEDAL_REORDER", &i, sizeof(int));
            ImGui::Text("Move %s", widgets_[i]->get_effect()->name());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PEDAL_REORDER")) {
                int source_idx = *static_cast<const int*>(payload->Data);
                if (source_idx != i) {
                    history_.execute(std::make_unique<ReorderEffectCommand>(engine_, source_idx, i));
                    rebuild_widgets();
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Connection dot between pedals
        if (i < static_cast<int>(widgets_.size()) - 1) {
            float dot_x = pedal_x + 190;
            dl->AddCircleFilled(ImVec2(dot_x, line_y), 4, Theme::CHAIN_DOT);
        }

        pedal_x += 195;
    }

    // Output jack
    dl->AddCircleFilled(ImVec2(pedal_x, line_y), 6, Theme::CHAIN_JACK);
    dl->AddCircle(ImVec2(pedal_x, line_y), 6, Theme::BORDER_DARK, 0, 1.5f);

    // Handle removal
    if (remove_idx >= 0) {
        history_.execute(std::make_unique<RemoveEffectCommand>(engine_, remove_idx));
        rebuild_widgets();
    }

    // Reserve space for horizontal scrolling
    ImGui::Dummy(ImVec2(total_width + 20, 340));
}

} // namespace GuitarAmp
