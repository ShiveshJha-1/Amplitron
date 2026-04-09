#include "test_framework.h"
#include "audio/dsp/convolution_engine.h"
#include "audio/dsp/wav_loader.h"
#include "audio/effects/ir_cabinet.h"
#include "audio/effect_factory.h"
#include <cmath>
#include <cstring>
#include <vector>
#include <numeric>

using namespace Amplitron;
using namespace TestFramework;

// ============================================================================
// Convolution engine tests
// ============================================================================

TEST(ConvolutionKernel_UnitImpulse) {
    // A single-sample IR [1.0] should produce 1 partition
    std::vector<float> ir = {1.0f};
    ConvolutionKernel kernel(ir, 64);
    ASSERT_EQ(kernel.num_partitions(), 1);
    ASSERT_EQ(kernel.block_size(), 64);
    ASSERT_EQ(kernel.fft_size(), 128);
    ASSERT_EQ(kernel.ir_length(), 1);
}

TEST(ConvolutionEngine_UnitImpulse_Identity) {
    // Convolving with a unit impulse [1.0] should reproduce the input
    const int block_size = 64;
    std::vector<float> ir = {1.0f};
    auto kernel = std::make_shared<ConvolutionKernel>(ir, block_size);

    ConvolutionEngine engine;
    engine.set_kernel(kernel);

    // Generate a sine wave input
    std::vector<float> input(block_size);
    for (int i = 0; i < block_size; ++i) {
        input[i] = std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 48000.0f);
    }

    std::vector<float> expected(input);
    engine.process(input.data(), block_size);

    for (int i = 0; i < block_size; ++i) {
        ASSERT_NEAR(input[i], expected[i], 1e-4f);
    }
}

TEST(ConvolutionEngine_DelayedImpulse) {
    // IR = [0, 0, 0, 0, 1.0] should delay the signal by 4 samples
    const int block_size = 64;
    const int delay = 4;
    std::vector<float> ir(delay + 1, 0.0f);
    ir[delay] = 1.0f;

    auto kernel = std::make_shared<ConvolutionKernel>(ir, block_size);

    ConvolutionEngine engine;
    engine.set_kernel(kernel);

    // Input: impulse at sample 0
    std::vector<float> input(block_size, 0.0f);
    input[0] = 1.0f;

    engine.process(input.data(), block_size);

    // Output should have the impulse at position 'delay'
    for (int i = 0; i < block_size; ++i) {
        float expected = (i == delay) ? 1.0f : 0.0f;
        ASSERT_NEAR(input[i], expected, 1e-4f);
    }
}

TEST(ConvolutionEngine_KnownFIR) {
    // IR = [0.25, 0.5, 0.25] — simple FIR averaging filter
    const int block_size = 64;
    std::vector<float> ir = {0.25f, 0.5f, 0.25f};
    auto kernel = std::make_shared<ConvolutionKernel>(ir, block_size);

    ConvolutionEngine engine;
    engine.set_kernel(kernel);

    // Input: step function [1, 1, 1, ...]
    std::vector<float> input(block_size, 1.0f);
    // Make the first few samples distinct
    input[0] = 0.0f;

    // Manual convolution for comparison
    std::vector<float> expected(block_size, 0.0f);
    for (int i = 0; i < block_size; ++i) {
        for (int j = 0; j < 3; ++j) {
            int src = i - j;
            if (src >= 0 && src < block_size) {
                expected[i] += input[src] * ir[j];
            }
        }
    }

    // Save input copy before processing (process modifies in-place)
    // We already computed expected from the original input, so just process
    std::vector<float> process_buf(input);
    engine.process(process_buf.data(), block_size);

    for (int i = 0; i < block_size; ++i) {
        ASSERT_NEAR(process_buf[i], expected[i], 1e-4f);
    }
}

TEST(ConvolutionEngine_MultiBlock_Continuity) {
    // A long IR spanning multiple blocks should produce continuous output
    const int block_size = 32;
    const int ir_len = 128;  // 4 blocks
    std::vector<float> ir(ir_len, 0.0f);
    // Simple exponential decay IR
    for (int i = 0; i < ir_len; ++i) {
        ir[i] = std::exp(-static_cast<float>(i) * 0.05f) * 0.1f;
    }

    auto kernel = std::make_shared<ConvolutionKernel>(ir, block_size);

    ConvolutionEngine engine;
    engine.set_kernel(kernel);

    // Process several blocks with an impulse in the first block
    const int num_blocks = 8;
    std::vector<float> all_output(num_blocks * block_size, 0.0f);

    for (int b = 0; b < num_blocks; ++b) {
        std::vector<float> block(block_size, 0.0f);
        if (b == 0) block[0] = 1.0f;  // impulse in first block

        engine.process(block.data(), block_size);
        std::memcpy(&all_output[b * block_size], block.data(),
                     sizeof(float) * block_size);
    }

    // The output should match the IR for the first ir_len samples
    for (int i = 0; i < ir_len; ++i) {
        ASSERT_NEAR(all_output[i], ir[i], 1e-3f);
    }

    // After the IR length, output should be near zero
    for (int i = ir_len; i < num_blocks * block_size; ++i) {
        ASSERT_NEAR(all_output[i], 0.0f, 1e-4f);
    }
}

TEST(ConvolutionEngine_LongIR_Ringout) {
    // Verify that a multi-partition IR rings out correctly
    const int block_size = 16;
    const int ir_len = 80;  // 5 blocks
    std::vector<float> ir(ir_len);
    for (int i = 0; i < ir_len; ++i) {
        ir[i] = 1.0f / static_cast<float>(ir_len);  // flat IR (average)
    }

    auto kernel = std::make_shared<ConvolutionKernel>(ir, block_size);

    ConvolutionEngine engine;
    engine.set_kernel(kernel);

    // Send impulse
    std::vector<float> block(block_size, 0.0f);
    block[0] = 1.0f;
    engine.process(block.data(), block_size);

    // Collect output from multiple blocks
    float total_energy = 0.0f;
    for (int i = 0; i < block_size; ++i) total_energy += block[i] * block[i];

    for (int b = 1; b < 10; ++b) {
        std::vector<float> buf(block_size, 0.0f);
        engine.process(buf.data(), block_size);
        for (int i = 0; i < block_size; ++i) total_energy += buf[i] * buf[i];
    }

    // Total energy should match IR energy (Parseval's theorem)
    float ir_energy = 0.0f;
    for (float s : ir) ir_energy += s * s;

    ASSERT_NEAR(total_energy, ir_energy, 1e-3f);
}

// ============================================================================
// Resampler tests
// ============================================================================

TEST(Resampler_OutputLength) {
    // Resampling from 44100 to 48000 should produce the correct output length
    std::vector<float> input(4410, 0.5f);  // 100ms at 44100
    auto output = resample_linear(input, 44100, 48000);
    // Expected length: ceil(4410 * 48000 / 44100) = 4800
    ASSERT_EQ(static_cast<int>(output.size()), 4800);
}

TEST(Resampler_SameRate_Passthrough) {
    std::vector<float> input = {1.0f, 2.0f, 3.0f, 4.0f};
    auto output = resample_linear(input, 48000, 48000);
    ASSERT_EQ(output.size(), input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        ASSERT_NEAR(output[i], input[i], 1e-6f);
    }
}

// ============================================================================
// IRCabinet effect tests
// ============================================================================

TEST(IRCabinet_Interface) {
    IRCabinet cab;
    ASSERT_TRUE(std::strcmp(cab.name(), "IR Cabinet") == 0);
    ASSERT_EQ(static_cast<int>(cab.params().size()), 1);  // Level
    ASSERT_FALSE(cab.has_ir());

    // Process without IR loaded — should be passthrough (no crash)
    std::vector<float> buf(64, 0.5f);
    std::vector<float> expected(buf);
    cab.process(buf.data(), 64);
    for (int i = 0; i < 64; ++i) {
        ASSERT_NEAR(buf[i], expected[i], 1e-6f);
    }
}

TEST(IRCabinet_FactoryRegistration) {
    auto fx = EffectFactory::instance().create("IR Cabinet");
    ASSERT_TRUE(fx != nullptr);
    ASSERT_TRUE(std::strcmp(fx->name(), "IR Cabinet") == 0);
}
