#include "audio/audio_engine.h"

namespace Amplitron {

void AudioEngine::add_effect(std::shared_ptr<Effect> effect) {
    std::lock_guard<std::mutex> lock(effect_mutex_);
    effect->set_sample_rate(sample_rate_);
    effects_.push_back(std::move(effect));
    topology_dirty_.store(true, std::memory_order_release);
}

void AudioEngine::insert_effect(int index, std::shared_ptr<Effect> effect) {
    std::lock_guard<std::mutex> lock(effect_mutex_);
    effect->set_sample_rate(sample_rate_);
    if (index >= 0 && index < static_cast<int>(effects_.size())) {
        effects_.insert(effects_.begin() + index, std::move(effect));
    } else {
        effects_.push_back(std::move(effect));
    }
    topology_dirty_.store(true, std::memory_order_release);
}

void AudioEngine::remove_effect(int index) {
    std::lock_guard<std::mutex> lock(effect_mutex_);
    if (index >= 0 && index < static_cast<int>(effects_.size())) {
        effects_.erase(effects_.begin() + index);
        topology_dirty_.store(true, std::memory_order_release);
    }
}

void AudioEngine::restore_effects_state(std::vector<std::shared_ptr<Effect>> new_effects) {
    std::lock_guard<std::mutex> lock(effect_mutex_);
    effects_.clear();
    for (auto& fx : new_effects) {
        fx->set_sample_rate(sample_rate_);
        effects_.push_back(std::move(fx));
    }
    topology_dirty_.store(true, std::memory_order_release);
}

void AudioEngine::set_tuner_tap(std::shared_ptr<Effect> tap) {
    std::lock_guard<std::mutex> lock(effect_mutex_);
    tuner_tap_ = std::move(tap);
    if (tuner_tap_) {
        tuner_tap_->set_sample_rate(sample_rate_);
        tuner_tap_->reset();
    }
    topology_dirty_.store(true, std::memory_order_release);
}

void AudioEngine::clear_tuner_tap() {
    std::lock_guard<std::mutex> lock(effect_mutex_);
    tuner_tap_.reset();
    topology_dirty_.store(true, std::memory_order_release);
}

bool AudioEngine::has_tuner_tap() const {
    return tuner_tap_ != nullptr;
}

void AudioEngine::move_effect(int from, int to) {
    std::lock_guard<std::mutex> lock(effect_mutex_);
    int n = static_cast<int>(effects_.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) return;
    auto effect = effects_[from];
    effects_.erase(effects_.begin() + from);
    effects_.insert(effects_.begin() + to, effect);
    topology_dirty_.store(true, std::memory_order_release);
}

} // namespace Amplitron
