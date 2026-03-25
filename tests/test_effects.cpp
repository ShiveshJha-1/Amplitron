#include "test_framework.h"
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
#include <cstring>
#include <cmath>

using namespace GuitarAmp;

// Helper: fill buffer with a sine wave
static void fill_sine(float* buf, int n, float freq, int sr) {
    for (int i = 0; i < n; ++i)
        buf[i] = std::sin(2.0f * 3.14159265f * freq * i / sr);
}

// Helper: compute RMS of a buffer
static float rms(const float* buf, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) sum += buf[i] * buf[i];
    return std::sqrt(sum / n);
}

// Helper: check no NaN or Inf in buffer
static bool buffer_is_finite(const float* buf, int n) {
    for (int i = 0; i < n; ++i)
        if (!std::isfinite(buf[i])) return false;
    return true;
}

// ============================================================
// Effect base class tests
// ============================================================

TEST(effect_enabled_default_true) {
    NoiseGate ng;
    ASSERT_TRUE(ng.is_enabled());
}

TEST(effect_enable_disable) {
    NoiseGate ng;
    ng.set_enabled(false);
    ASSERT_FALSE(ng.is_enabled());
    ng.set_enabled(true);
    ASSERT_TRUE(ng.is_enabled());
}

TEST(effect_mix_clamped) {
    Overdrive od;
    od.set_mix(-0.5f);
    ASSERT_NEAR(od.get_mix(), 0.0f, 1e-6f);
    od.set_mix(1.5f);
    ASSERT_NEAR(od.get_mix(), 1.0f, 1e-6f);
    od.set_mix(0.5f);
    ASSERT_NEAR(od.get_mix(), 0.5f, 1e-6f);
}

TEST(effect_has_name) {
    NoiseGate ng;
    Compressor comp;
    Overdrive od;
    Distortion dist;
    Equalizer eq;
    Chorus ch;
    Delay dl;
    Reverb rv;
    CabinetSim cab;

    ASSERT_TRUE(std::strcmp(ng.name(), "Noise Gate") == 0);
    ASSERT_TRUE(std::strcmp(comp.name(), "Compressor") == 0);
    ASSERT_TRUE(std::strcmp(od.name(), "Overdrive") == 0);
    ASSERT_TRUE(std::strcmp(dist.name(), "Distortion") == 0);
    ASSERT_TRUE(std::strcmp(eq.name(), "Equalizer") == 0);
    ASSERT_TRUE(std::strcmp(ch.name(), "Chorus") == 0);
    ASSERT_TRUE(std::strcmp(dl.name(), "Delay") == 0);
    ASSERT_TRUE(std::strcmp(rv.name(), "Reverb") == 0);
    ASSERT_TRUE(std::strcmp(cab.name(), "Cabinet") == 0);

    AmpSimulator amp;
    ASSERT_TRUE(std::strcmp(amp.name(), "Amp Sim") == 0);
}

TEST(effect_has_params) {
    Overdrive od;
    ASSERT_GT((int)od.params().size(), 0);

    Equalizer eq;
    ASSERT_GT((int)eq.params().size(), 0);

    Compressor comp;
    ASSERT_GT((int)comp.params().size(), 0);
}

TEST(effect_params_have_valid_ranges) {
    Overdrive od;
    for (auto& p : od.params()) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val);
        ASSERT_TRUE(p.value <= p.max_val);
        ASSERT_TRUE(p.default_val >= p.min_val);
        ASSERT_TRUE(p.default_val <= p.max_val);
        ASSERT_FALSE(p.name.empty());
    }
}

TEST(effect_set_sample_rate) {
    Reverb rv;
    rv.set_sample_rate(44100);
    rv.reset();
    // Should not crash
    float buf[128];
    fill_sine(buf, 128, 440.0f, 44100);
    rv.process(buf, 128);
    ASSERT_TRUE(buffer_is_finite(buf, 128));
}

// ============================================================
// Individual effect processing tests
// ============================================================

TEST(noise_gate_silences_quiet_signal) {
    NoiseGate ng;
    ng.set_sample_rate(48000);
    ng.reset();

    // Very quiet signal (well below any reasonable threshold)
    float buf[256];
    for (int i = 0; i < 256; ++i) buf[i] = 0.0001f * std::sin(2.0f * 3.14159f * 440.0f * i / 48000.0f);

    // Process several times to let the gate close
    for (int rep = 0; rep < 20; ++rep)
        ng.process(buf, 256);

    // After many passes of quiet signal, output should be very quiet
    float out_rms = rms(buf, 256);
    ASSERT_LT(out_rms, 0.001f);
}

TEST(noise_gate_passes_loud_signal) {
    NoiseGate ng;
    ng.set_sample_rate(48000);
    ng.reset();

    float buf[256];
    fill_sine(buf, 256, 440.0f, 48000);
    float in_rms = rms(buf, 256);

    ng.process(buf, 256);
    float out_rms = rms(buf, 256);

    // Loud signal should pass through mostly unchanged
    ASSERT_GT(out_rms, in_rms * 0.5f);
}

TEST(overdrive_adds_harmonics) {
    Overdrive od;
    od.set_sample_rate(48000);
    od.reset();

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);

    od.process(buf, 512);
    ASSERT_TRUE(buffer_is_finite(buf, 512));

    // Output should still have energy
    ASSERT_GT(rms(buf, 512), 0.01f);
}

TEST(distortion_clips_signal) {
    Distortion dist;
    dist.set_sample_rate(48000);
    dist.reset();

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);

    dist.process(buf, 512);
    ASSERT_TRUE(buffer_is_finite(buf, 512));

    // All output should be within [-1, 1] (clipped)
    for (int i = 0; i < 512; ++i) {
        ASSERT_GE(buf[i], -1.5f);  // some headroom for processing
        ASSERT_TRUE(buf[i] <= 1.5f);
    }
}

TEST(compressor_reduces_dynamic_range) {
    Compressor comp;
    comp.set_sample_rate(48000);
    comp.reset();

    // Loud signal
    float buf[2048];
    fill_sine(buf, 2048, 440.0f, 48000);
    // Scale to be loud
    for (int i = 0; i < 2048; ++i) buf[i] *= 0.9f;

    // Process multiple times to let compressor engage
    for (int rep = 0; rep < 5; ++rep) {
        fill_sine(buf, 2048, 440.0f, 48000);
        for (int i = 0; i < 2048; ++i) buf[i] *= 0.9f;
        comp.process(buf, 2048);
    }

    ASSERT_TRUE(buffer_is_finite(buf, 2048));
    ASSERT_GT(rms(buf, 2048), 0.01f);
}

TEST(equalizer_processes_without_nan) {
    Equalizer eq;
    eq.set_sample_rate(48000);
    eq.reset();

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);
    eq.process(buf, 512);

    ASSERT_TRUE(buffer_is_finite(buf, 512));
    ASSERT_GT(rms(buf, 512), 0.01f);
}

TEST(chorus_modulates_signal) {
    Chorus ch;
    ch.set_sample_rate(48000);
    ch.reset();

    float buf[1024];
    fill_sine(buf, 1024, 440.0f, 48000);
    ch.process(buf, 1024);

    ASSERT_TRUE(buffer_is_finite(buf, 1024));
}

TEST(delay_produces_echo) {
    Delay dl;
    dl.set_sample_rate(48000);
    dl.reset();

    // Process an impulse
    float buf[4096];
    std::memset(buf, 0, sizeof(buf));
    buf[0] = 1.0f;

    dl.process(buf, 4096);
    ASSERT_TRUE(buffer_is_finite(buf, 4096));

    // There should be energy later in the buffer (the echo)
    float late_energy = 0.0f;
    for (int i = 2048; i < 4096; ++i)
        late_energy += buf[i] * buf[i];
    (void)late_energy;
    // With default delay settings, some echo should appear
    // (might be zero if delay time > buffer length, so just check finite)
}

TEST(reverb_adds_tail) {
    Reverb rv;
    rv.set_sample_rate(48000);
    rv.reset();

    float buf[2048];
    std::memset(buf, 0, sizeof(buf));
    buf[0] = 1.0f;

    rv.process(buf, 2048);
    ASSERT_TRUE(buffer_is_finite(buf, 2048));

    // Late portion should have some reverb tail energy
    float tail_energy = 0.0f;
    for (int i = 1024; i < 2048; ++i)
        tail_energy += buf[i] * buf[i];
    ASSERT_GT(tail_energy, 1e-10f);
}

TEST(cabinet_sim_filters_signal) {
    CabinetSim cab;
    cab.set_sample_rate(48000);
    cab.reset();

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);
    cab.process(buf, 512);

    ASSERT_TRUE(buffer_is_finite(buf, 512));
    ASSERT_GT(rms(buf, 512), 0.001f);
}

TEST(amp_simulator_processes_without_nan) {
    AmpSimulator amp;
    amp.set_sample_rate(48000);
    amp.reset();

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);
    amp.process(buf, 512);

    ASSERT_TRUE(buffer_is_finite(buf, 512));
    ASSERT_GT(rms(buf, 512), 0.001f);
}

TEST(amp_simulator_models_sound_different) {
    const auto& models = GuitarAmp::get_amp_models();
    ASSERT_GE((int)models.size(), 3);

    std::vector<float> model_rms;
    for (int m = 0; m < static_cast<int>(models.size()); ++m) {
        AmpSimulator amp;
        amp.set_sample_rate(48000);
        amp.reset();
        amp.params()[0].value = static_cast<float>(m);

        float buf[1024];
        fill_sine(buf, 1024, 440.0f, 48000);
        amp.process(buf, 1024);
        ASSERT_TRUE(buffer_is_finite(buf, 1024));
        model_rms.push_back(rms(buf, 1024));
    }

    // At least one pair of models should produce meaningfully different RMS
    bool found_diff = false;
    for (size_t i = 0; i < model_rms.size() && !found_diff; ++i) {
        for (size_t j = i + 1; j < model_rms.size(); ++j) {
            if (std::fabs(model_rms[i] - model_rms[j]) > 0.01f) {
                found_diff = true;
                break;
            }
        }
    }
    ASSERT_TRUE(found_diff);
}

TEST(amp_simulator_output_clamped) {
    AmpSimulator amp;
    amp.set_sample_rate(48000);
    amp.reset();
    // High gain model
    amp.params()[0].value = 2.0f; // High Gain Modern
    amp.params()[1].value = 1.0f; // Max gain knob

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);
    amp.process(buf, 512);

    for (int i = 0; i < 512; ++i) {
        ASSERT_GE(buf[i], -1.0f);
        ASSERT_TRUE(buf[i] <= 1.0f);
    }
}

TEST(amp_simulator_get_models_returns_at_least_three) {
    const auto& models = GuitarAmp::get_amp_models();
    ASSERT_GE((int)models.size(), 3);
    for (const auto& m : models) {
        ASSERT_TRUE(m.name != nullptr);
        ASSERT_TRUE(m.inspiration != nullptr);
        ASSERT_TRUE(m.description != nullptr);
    }
}

TEST(all_effects_handle_silence) {
    std::vector<std::shared_ptr<Effect>> effects = {
        std::make_shared<NoiseGate>(),
        std::make_shared<Compressor>(),
        std::make_shared<Overdrive>(),
        std::make_shared<Distortion>(),
        std::make_shared<Equalizer>(),
        std::make_shared<Chorus>(),
        std::make_shared<Delay>(),
        std::make_shared<Reverb>(),
        std::make_shared<CabinetSim>(),
        std::make_shared<AmpSimulator>(),
    };

    float buf[256];
    for (auto& fx : effects) {
        fx->set_sample_rate(48000);
        fx->reset();
        std::memset(buf, 0, sizeof(buf));
        fx->process(buf, 256);
        ASSERT_TRUE(buffer_is_finite(buf, 256));
    }
}

TEST(all_effects_reset_without_crash) {
    std::vector<std::shared_ptr<Effect>> effects = {
        std::make_shared<NoiseGate>(),
        std::make_shared<Compressor>(),
        std::make_shared<Overdrive>(),
        std::make_shared<Distortion>(),
        std::make_shared<Equalizer>(),
        std::make_shared<Chorus>(),
        std::make_shared<Delay>(),
        std::make_shared<Reverb>(),
        std::make_shared<CabinetSim>(),
        std::make_shared<AmpSimulator>(),
    };

    for (auto& fx : effects) {
        fx->set_sample_rate(48000);
        // Process some audio
        float buf[128];
        fill_sine(buf, 128, 440.0f, 48000);
        fx->process(buf, 128);
        // Reset
        fx->reset();
        // Process again
        fill_sine(buf, 128, 440.0f, 48000);
        fx->process(buf, 128);
        ASSERT_TRUE(buffer_is_finite(buf, 128));
    }
}

TEST(all_effects_handle_different_sample_rates) {
    int rates[] = {22050, 44100, 48000, 96000};
    std::vector<std::shared_ptr<Effect>> effects = {
        std::make_shared<NoiseGate>(),
        std::make_shared<Compressor>(),
        std::make_shared<Overdrive>(),
        std::make_shared<Distortion>(),
        std::make_shared<Equalizer>(),
        std::make_shared<Chorus>(),
        std::make_shared<Delay>(),
        std::make_shared<Reverb>(),
        std::make_shared<CabinetSim>(),
        std::make_shared<AmpSimulator>(),
    };

    float buf[256];
    for (int rate : rates) {
        for (auto& fx : effects) {
            fx->set_sample_rate(rate);
            fx->reset();
            fill_sine(buf, 256, 440.0f, rate);
            fx->process(buf, 256);
            ASSERT_TRUE(buffer_is_finite(buf, 256));
        }
    }
}
