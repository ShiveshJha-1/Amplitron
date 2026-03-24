#include "gui/spectrum_analyzer.h"
#include "gui/theme.h"

#include <algorithm>
#include <cmath>

namespace GuitarAmp {

namespace {

constexpr float kMinHz = 20.0f;
constexpr float kMaxHz = 20000.0f;
constexpr float kMinDb = -90.0f;
constexpr float kMaxDb = 0.0f;

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float hz_to_log_norm(float hz) {
    const float lo = std::log10(kMinHz);
    const float hi = std::log10(kMaxHz);
    return clamp((std::log10(hz) - lo) / (hi - lo), 0.0f, 1.0f);
}

} // namespace

SpectrumAnalyzer::SpectrumAnalyzer() {
    for (int i = 0; i < FFT_SIZE; ++i) {
        const float phase = (TWO_PI * static_cast<float>(i)) / static_cast<float>(FFT_SIZE - 1);
        window_[i] = 0.5f * (1.0f - std::cos(phase));
    }

    smoothed_input_db_.fill(kMinDb);
    smoothed_output_db_.fill(kMinDb);
    input_peak_db_.fill(kMinDb);
    output_peak_db_.fill(kMinDb);
}

void SpectrumAnalyzer::run_fft(std::array<std::complex<float>, FFT_SIZE>& data) const {
    for (int i = 1, j = 0; i < FFT_SIZE; ++i) {
        int bit = FFT_SIZE >> 1;
        for (; (j & bit) != 0; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }

    for (int len = 2; len <= FFT_SIZE; len <<= 1) {
        const float angle = -TWO_PI / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < FFT_SIZE; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            const int half = len >> 1;
            for (int j = 0; j < half; ++j) {
                const std::complex<float> u = data[i + j];
                const std::complex<float> v = data[i + j + half] * w;
                data[i + j] = u + v;
                data[i + j + half] = u - v;
                w *= wlen;
            }
        }
    }
}

void SpectrumAnalyzer::compute_spectrum_bars(const float* samples,
                                             int sample_rate,
                                             std::array<float, DISPLAY_BARS>& bars_db) {
    if (!samples || sample_rate <= 0) {
        bars_db.fill(kMinDb);
        return;
    }

    for (int i = 0; i < FFT_SIZE; ++i) {
        fft_work_[i] = std::complex<float>(samples[i] * window_[i], 0.0f);
    }

    run_fft(fft_work_);

    std::array<float, FFT_BINS> mags_db{};
    mags_db.fill(kMinDb);

    const float norm = 2.0f / static_cast<float>(FFT_SIZE);
    for (int i = 1; i < FFT_BINS; ++i) {
        const float mag = std::abs(fft_work_[i]) * norm;
        mags_db[i] = clamp(20.0f * std::log10(std::max(mag, 1e-8f)), kMinDb, kMaxDb);
    }

    for (int bar = 0; bar < DISPLAY_BARS; ++bar) {
        const float t0 = static_cast<float>(bar) / static_cast<float>(DISPLAY_BARS);
        const float t1 = static_cast<float>(bar + 1) / static_cast<float>(DISPLAY_BARS);
        const float hz0 = std::pow(10.0f, lerp(std::log10(kMinHz), std::log10(kMaxHz), t0));
        const float hz1 = std::pow(10.0f, lerp(std::log10(kMinHz), std::log10(kMaxHz), t1));

        int bin0 = static_cast<int>(hz0 * FFT_SIZE / static_cast<float>(sample_rate));
        int bin1 = static_cast<int>(hz1 * FFT_SIZE / static_cast<float>(sample_rate));
        bin0 = std::max(1, std::min(bin0, FFT_BINS - 1));
        bin1 = std::max(bin0 + 1, std::min(bin1, FFT_BINS));

        float peak_db = kMinDb;
        for (int b = bin0; b < bin1; ++b) {
            peak_db = std::max(peak_db, mags_db[b]);
        }
        bars_db[bar] = peak_db;
    }
}

void SpectrumAnalyzer::update(const float* input_samples,
                              const float* output_samples,
                              int sample_rate,
                              float dt_seconds) {
    std::array<float, DISPLAY_BARS> input_db{};
    std::array<float, DISPLAY_BARS> output_db{};
    compute_spectrum_bars(input_samples, sample_rate, input_db);
    compute_spectrum_bars(output_samples, sample_rate, output_db);

    const float smooth_alpha = 0.26f;
    const float peak_decay_db_per_sec = 30.0f;
    const float decay = peak_decay_db_per_sec * std::max(dt_seconds, 1.0f / 240.0f);

    for (int i = 0; i < DISPLAY_BARS; ++i) {
        smoothed_input_db_[i] = lerp(smoothed_input_db_[i], input_db[i], smooth_alpha);
        smoothed_output_db_[i] = lerp(smoothed_output_db_[i], output_db[i], smooth_alpha);

        input_peak_db_[i] = std::max(smoothed_input_db_[i], input_peak_db_[i] - decay);
        output_peak_db_[i] = std::max(smoothed_output_db_[i], output_peak_db_[i] - decay);
    }
}

void SpectrumAnalyzer::draw(ImDrawList* draw_list,
                            const ImVec2& pos,
                            const ImVec2& size,
                            DisplayMode mode) const {
    if (!draw_list || size.x <= 2.0f || size.y <= 2.0f) {
        return;
    }

    const ImVec2 pmax(pos.x + size.x, pos.y + size.y);
    draw_list->AddRect(pos, pmax, IM_COL32(72, 78, 92, 220), Theme::ROUNDING_SM);

    const float ref_lines[] = {-60.0f, -48.0f, -36.0f, -24.0f, -12.0f};
    for (float db : ref_lines) {
        float t = (db - kMinDb) / (kMaxDb - kMinDb);
        float y = pmax.y - t * size.y;
        draw_list->AddLine(ImVec2(pos.x, y), ImVec2(pmax.x, y), IM_COL32(58, 64, 76, 180), 1.0f);
    }

    const ImU32 input_col = IM_COL32(82, 220, 135, 220);
    const ImU32 output_col = IM_COL32(92, 170, 255, 220);
    const ImU32 peak_col = IM_COL32(255, 240, 165, 255);

    const auto draw_set = [&](const std::array<float, DISPLAY_BARS>& bars,
                              const std::array<float, DISPLAY_BARS>& peaks,
                              ImU32 bar_col,
                              float width_scale) {
        for (int i = 0; i < DISPLAY_BARS; ++i) {
            const float x0 = pos.x + (static_cast<float>(i) / DISPLAY_BARS) * size.x;
            const float x1 = pos.x + (static_cast<float>(i + 1) / DISPLAY_BARS) * size.x;
            const float db = clamp(bars[i], kMinDb, kMaxDb);
            const float t = (db - kMinDb) / (kMaxDb - kMinDb);
            const float y = pmax.y - t * size.y;

            const float center = (x0 + x1) * 0.5f;
            const float half = (x1 - x0) * 0.5f * width_scale;
            draw_list->AddRectFilled(ImVec2(center - half, y), ImVec2(center + half, pmax.y), bar_col, 1.5f);

            const float peak_db = clamp(peaks[i], kMinDb, kMaxDb);
            const float peak_t = (peak_db - kMinDb) / (kMaxDb - kMinDb);
            const float py = pmax.y - peak_t * size.y;
            draw_list->AddLine(ImVec2(center - half, py), ImVec2(center + half, py), peak_col, 1.0f);
        }
    };

    switch (mode) {
        case DisplayMode::Input:
            draw_set(smoothed_input_db_, input_peak_db_, input_col, 0.82f);
            break;
        case DisplayMode::Output:
            draw_set(smoothed_output_db_, output_peak_db_, output_col, 0.82f);
            break;
        case DisplayMode::Overlay:
            draw_set(smoothed_input_db_, input_peak_db_, input_col, 0.42f);
            draw_set(smoothed_output_db_, output_peak_db_, output_col, 0.42f);
            break;
    }

    const float ticks[] = {20.0f, 100.0f, 1000.0f, 5000.0f, 10000.0f, 20000.0f};
    for (float hz : ticks) {
        float x = pos.x + hz_to_log_norm(hz) * size.x;
        draw_list->AddLine(ImVec2(x, pos.y), ImVec2(x, pmax.y), IM_COL32(52, 58, 72, 180), 1.0f);
    }
}

} // namespace GuitarAmp
