#include "audio/effects/ir_cabinet.h"
#include "audio/effect_factory.h"
#include "audio/dsp/wav_loader.h"
#include <cstring>
#include <iostream>
#include <algorithm>

namespace Amplitron {

static EffectRegistrar<IRCabinet> reg("IR Cabinet");

IRCabinet::IRCabinet() {
    params_.push_back({"Level", 1.0f, 0.0f, 2.0f, 1.0f, "", "Output level"});
}

IRCabinet::~IRCabinet() {
    // Clean up any unconsumed pending kernel
    ConvolutionKernel* pending = pending_kernel_.exchange(nullptr);
    delete pending;
}

int IRCabinet::max_ir_samples() const {
    // 500ms at current sample rate
    return sample_rate_ / 2;
}

bool IRCabinet::load_ir(const std::string& filepath) {
    WavData wav = load_wav_file(filepath, sample_rate_, max_ir_samples());
    if (wav.samples.empty()) return false;

    raw_ir_samples_ = wav.samples;
    ir_path_ = filepath;

    // Extract filename from path
    size_t sep = filepath.find_last_of("/\\");
    ir_name_ = (sep != std::string::npos) ? filepath.substr(sep + 1) : filepath;
    ir_duration_ms_ = static_cast<float>(raw_ir_samples_.size()) /
                      static_cast<float>(sample_rate_) * 1000.0f;

    // Build kernel with current expected block size, or a reasonable default
    int bs = expected_block_size_ > 0 ? expected_block_size_ : 256;
    build_kernel(bs);

    std::cout << "IR Cabinet: loaded \"" << ir_name_ << "\" ("
              << raw_ir_samples_.size() << " samples, "
              << ir_duration_ms_ << " ms)" << std::endl;
    return true;
}

void IRCabinet::clear_ir() {
    raw_ir_samples_.clear();
    ir_path_.clear();
    ir_name_.clear();
    ir_duration_ms_ = 0.0f;

    // Store a null kernel to signal clearing
    ConvolutionKernel* old = pending_kernel_.exchange(nullptr);
    delete old;

    // Directly clear the engine (safe if called from GUI thread when audio
    // thread is not actively processing — structural changes hold effect_mutex_)
    conv_engine_.set_kernel(nullptr);
}

bool IRCabinet::has_ir() const {
    return !raw_ir_samples_.empty();
}

void IRCabinet::build_kernel(int block_size) {
    if (raw_ir_samples_.empty() || block_size <= 0) return;

    auto* kernel = new ConvolutionKernel(raw_ir_samples_, block_size);
    kernel->source_path = ir_path_;
    kernel->source_name = ir_name_;
    kernel->duration_ms = ir_duration_ms_;

    expected_block_size_ = block_size;

    // Store for audio thread to pick up (delete any unconsumed previous kernel)
    ConvolutionKernel* old = pending_kernel_.exchange(kernel);
    delete old;
}

void IRCabinet::check_pending_kernel() {
    ConvolutionKernel* pending = pending_kernel_.exchange(nullptr,
                                                          std::memory_order_acquire);
    if (pending) {
        conv_engine_.set_kernel(
            std::shared_ptr<const ConvolutionKernel>(pending));
    }

    // Handle deferred block-size-triggered kernel rebuild
    // (avoids allocation in the real-time callback loop)
    int new_block_size = pending_block_size_.exchange(0);
    if (new_block_size > 0 && new_block_size != expected_block_size_ &&
        !raw_ir_samples_.empty()) {
        build_kernel(new_block_size);
    }
}

void IRCabinet::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);

    // Reload IR at new sample rate if one is loaded
    if (!ir_path_.empty()) {
        load_ir(ir_path_);
    }
}

void IRCabinet::reset() {
    conv_engine_.reset();
}

void IRCabinet::process(float* buffer, int num_samples) {
    // Check for new kernel from GUI thread and deferred rebuilds
    check_pending_kernel();

    if (!conv_engine_.has_kernel()) return;  // passthrough when no IR loaded

    // If block size changed, defer kernel rebuild to avoid allocation in callback
    if (num_samples != expected_block_size_ && num_samples > 0 &&
        !raw_ir_samples_.empty()) {
        // The convolution engine's process() will fall back to direct convolution
        // when num_samples != block_size. Request kernel rebuild for next process call.
        // By deferring via pending_block_size_, we avoid allocations in the real-time callback.
        pending_block_size_.store(num_samples, std::memory_order_release);
    }

    float level = params_[0].value;

    // Save dry signal for mix blending (resize only when block size changes)
    if (dry_buffer_.size() != static_cast<size_t>(num_samples)) {
        dry_buffer_.resize(num_samples);
    }
    std::copy(buffer, buffer + num_samples, dry_buffer_.begin());

    // Apply convolution
    conv_engine_.process(buffer, num_samples);

    // Apply level
    if (level != 1.0f) {
        for (int i = 0; i < num_samples; ++i) {
            buffer[i] *= level;
        }
    }

    // Apply dry/wet mix (using base class mix_)
    if (mix_ < 1.0f) {
        float wet = mix_;
        float dry_amt = 1.0f - wet;
        for (int i = 0; i < num_samples; ++i) {
            buffer[i] = dry_buffer_[static_cast<size_t>(i)] * dry_amt + buffer[i] * wet;
        }
    }
}

} // namespace Amplitron
