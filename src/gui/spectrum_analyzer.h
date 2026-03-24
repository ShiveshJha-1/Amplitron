#pragma once

#include "common.h"
#include <imgui.h>
#include <array>
#include <complex>

namespace GuitarAmp {

class SpectrumAnalyzer {
public:
    static constexpr int FFT_SIZE = 2048;
    static constexpr int FFT_BINS = FFT_SIZE / 2;
    static constexpr int DISPLAY_BARS = 96;

    enum class DisplayMode {
        Input = 0,
        Output = 1,
        Overlay = 2,
    };

    SpectrumAnalyzer();

    void update(const float* input_samples,
                const float* output_samples,
                int sample_rate,
                float dt_seconds);

    void draw(ImDrawList* draw_list,
              const ImVec2& pos,
              const ImVec2& size,
              DisplayMode mode) const;

private:
    void compute_spectrum_bars(const float* samples,
                               int sample_rate,
                               std::array<float, DISPLAY_BARS>& bars_db);
    void run_fft(std::array<std::complex<float>, FFT_SIZE>& data) const;

    std::array<float, FFT_SIZE> window_{};
    std::array<std::complex<float>, FFT_SIZE> fft_work_{};

    std::array<float, DISPLAY_BARS> smoothed_input_db_{};
    std::array<float, DISPLAY_BARS> smoothed_output_db_{};
    std::array<float, DISPLAY_BARS> input_peak_db_{};
    std::array<float, DISPLAY_BARS> output_peak_db_{};
};

} // namespace GuitarAmp
