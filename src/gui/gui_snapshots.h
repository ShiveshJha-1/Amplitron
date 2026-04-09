#pragma once

#include "common.h"
#include "audio/audio_engine.h"
#include "gui/command_history.h"
#include "gui/snapshot_manager.h"

namespace Amplitron {

class PedalBoard;

/**
 * @brief GUI module for the in-session snapshot (A/B/C/D) toolbar row.
 *
 * Renders four labeled slot buttons with visual state indicators and handles
 * save/recall interactions:
 *   - Left-click a filled slot  → recall (undoable via RecallSnapshotCommand)
 *   - Right-click any slot      → context menu (Save / Clear)
 *   - Keyboard Cmd/Ctrl+1–4    → save current board to slot A–D (handled by GuiManager)
 *
 * Recall operations are routed through CommandHistory so they appear in the
 * Edit > Undo/Redo menu.
 */
class GuiSnapshots {
public:
    GuiSnapshots(AudioEngine& engine, CommandHistory& history);

    /** @brief Render the snapshot toolbar row (call once per frame). */
    void render();

    /**
     * @brief Save the current engine state to the given slot (0–3).
     *
     * Does not go through command history — saving a slot is not undoable.
     * The active slot indicator is updated to the saved slot.
     */
    void save_to_slot(int slot);

    /**
     * @brief Recall the snapshot from the given slot via command history.
     *
     * Records a RecallSnapshotCommand so the operation can be undone with
     * Ctrl+Z. No-op if the slot is empty.
     */
    void recall_slot(int slot);

    /** @brief Set the pedal board pointer so rebuild_widgets() is called after recall. */
    void set_pedal_board(PedalBoard* pb) { pedal_board_ = pb; }

    /** @brief Access the underlying SnapshotManager. */
    SnapshotManager& manager() { return manager_; }
    const SnapshotManager& manager() const { return manager_; }

private:
    static constexpr float STATUS_DISPLAY_SECONDS = 2.0f;

    AudioEngine& engine_;
    CommandHistory& history_;
    PedalBoard* pedal_board_ = nullptr;
    SnapshotManager manager_;
    char status_msg_[64] = {};
    float status_timer_ = 0.0f;
};

} // namespace Amplitron
