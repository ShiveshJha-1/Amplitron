#pragma once

#include "audio/effect.h"
#include "audio/dsp/convolution_engine.h"
#include <atomic>
#include <string>
#include <vector>
#include <memory>

namespace Amplitron {

class IRCabinet : public Effect {
public:
    IRCabinet();
    ~IRCabinet() override;

    void process(float* buffer, int num_samples) override;
    void set_sample_rate(int sample_rate) override;
    void reset() override;
    const char* name() const override { return "IR Cabinet"; }
    std::vector<EffectParam>& params() override { return params_; }

    // --- IR management (called from GUI thread) ---

    // Load an IR file. Returns true on success.
    bool load_ir(const std::string& filepath);

    // Clear the loaded IR.
    void clear_ir();

    // Query state (safe from GUI thread)
    bool has_ir() const;
    const std::string& ir_path() const { return ir_path_; }
    const std::string& ir_name() const { return ir_name_; }
    float ir_duration_ms() const { return ir_duration_ms_; }

private:
    std::vector<EffectParam> params_;

    // Atomic kernel swap: GUI thread stores, audio thread consumes
    std::atomic<ConvolutionKernel*> pending_kernel_{nullptr};

    ConvolutionEngine conv_engine_;

    // Raw IR samples for rebuilding kernel on sample rate / block size changes
    std::vector<float> raw_ir_samples_;

    // IR file metadata
    std::string ir_path_;
    std::string ir_name_;
    float ir_duration_ms_ = 0.0f;

    // Expected block size for the current kernel
    int expected_block_size_ = 0;

    // Max IR length: 500ms worth of samples at current sample rate
    int max_ir_samples() const;

    // Check and consume pending kernel (called at start of process())
    void check_pending_kernel();

    // Build kernel from raw_ir_samples_ for a given block size
    void build_kernel(int block_size);
};

} // namespace Amplitron
