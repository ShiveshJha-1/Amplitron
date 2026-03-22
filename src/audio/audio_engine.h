#pragma once

#include "common.h"
#include "audio/effect.h"
#include "audio/recorder.h"
#include <portaudio.h>

namespace GuitarAmp {

struct AudioDeviceInfo {
    int index;
    std::string name;
    int max_input_channels;
    int max_output_channels;
    double default_sample_rate;
    bool is_usb_device;
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool initialize();
    void shutdown();
    bool start();
    void stop();
    bool restart();  // stop + start (for manual recovery)

    // Error reporting
    std::string get_last_error() const { return last_error_; }
    void clear_error() { last_error_.clear(); }

    // Device management
    std::vector<AudioDeviceInfo> get_input_devices() const;
    std::vector<AudioDeviceInfo> get_output_devices() const;
    bool set_input_device(int device_index);
    bool set_output_device(int device_index);
    int get_input_device() const { return input_device_; }
    int get_output_device() const { return output_device_; }
    std::string get_input_device_name() const;
    std::string get_output_device_name() const;

    // Effect chain
    void add_effect(std::shared_ptr<Effect> effect);
    void remove_effect(int index);
    void move_effect(int from, int to);
    std::vector<std::shared_ptr<Effect>>& effects() { return effects_; }

    // Settings
    void set_buffer_size(int size);
    void set_sample_rate(int rate);
    int get_buffer_size() const { return buffer_size_; }
    int get_sample_rate() const { return sample_rate_; }
    bool is_running() const { return running_; }

    // Metering
    float get_input_level() const { return input_level_.load(); }
    float get_output_level() const { return output_level_.load(); }

    // Master volume
    void set_input_gain(float gain) { input_gain_ = gain; }
    void set_output_gain(float gain) { output_gain_ = gain; }
    float get_input_gain() const { return input_gain_; }
    float get_output_gain() const { return output_gain_; }

    // Recording
    Recorder& recorder() { return recorder_; }

private:
    static int audio_callback(const void* input, void* output,
                              unsigned long frame_count,
                              const PaStreamCallbackTimeInfo* time_info,
                              PaStreamCallbackFlags status_flags,
                              void* user_data);

    void process_audio(const float* input, float* output, int frame_count);
    void auto_detect_devices();
    static bool is_usb_device_name(const std::string& name);
    bool devices_share_host_api(int input_dev, int output_dev) const;

    PaStream* stream_ = nullptr;
    bool initialized_ = false;
    bool running_ = false;

    int input_device_ = -1;
    int output_device_ = -1;
    int sample_rate_ = DEFAULT_SAMPLE_RATE;
    int buffer_size_ = DEFAULT_BUFFER_SIZE;

    float input_gain_ = 1.0f;
    float output_gain_ = 0.8f;

    std::atomic<float> input_level_{0.0f};
    std::atomic<float> output_level_{0.0f};

    std::vector<std::shared_ptr<Effect>> effects_;
    std::vector<float> process_buffer_;
    std::mutex effect_mutex_;
    Recorder recorder_;
    std::string last_error_;
};

} // namespace GuitarAmp
