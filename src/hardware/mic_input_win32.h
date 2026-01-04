/*
 *  Copyright (C) 2024 DOSBox-X Team
 *
 *  SoundBlaster Microphone Input Support - Windows WASAPI Implementation
 *
 *  This module provides real microphone input capture for SoundBlaster
 *  recording emulation using Windows Audio Session API (WASAPI).
 */

#ifndef DOSBOX_MIC_INPUT_WIN32_H
#define DOSBOX_MIC_INPUT_WIN32_H

#ifdef WIN32

// Prevent Windows min/max macros from conflicting with std::min/std::max
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <mutex>

// Forward declarations
class MicrophoneInput;

// Microphone input states
enum MicInputState {
    MIC_STATE_CLOSED = 0,
    MIC_STATE_OPEN,
    MIC_STATE_CAPTURING,
    MIC_STATE_ERROR
};

// Audio format for captured data
struct MicAudioFormat {
    uint32_t sampleRate;
    uint16_t bitsPerSample;
    uint16_t channels;
};

// Circular buffer for audio samples
class MicCircularBuffer {
public:
    MicCircularBuffer(size_t capacity = 262144);  // 256KB for ~340ms at 48kHz
    ~MicCircularBuffer();

    // Write samples to buffer (producer - capture thread)
    size_t Write(const uint8_t* data, size_t bytes);

    // Read samples from buffer (consumer - emulation thread)
    size_t Read(uint8_t* data, size_t bytes);

    // Get available bytes to read
    size_t Available() const;

    // Clear buffer
    void Clear();

private:
    std::vector<uint8_t> buffer_;
    size_t readPos_;
    size_t writePos_;
    size_t count_;
    mutable std::mutex mutex_;
};

// Main microphone input class using WASAPI
class MicrophoneInput {
public:
    MicrophoneInput();
    ~MicrophoneInput();

    // Initialization
    bool Initialize();
    void Shutdown();

    // Device enumeration
    std::vector<std::wstring> GetDeviceList();
    bool SelectDevice(int deviceIndex);  // -1 for default
    std::wstring GetCurrentDeviceName() const;

    // Capture control
    bool StartCapture();
    void StopCapture();
    bool IsCapturing() const { return state_ == MIC_STATE_CAPTURING; }

    // Audio retrieval - called by emulation
    // Retrieves audio in the format requested by the DOS program
    size_t GetSamples(uint8_t* buffer, size_t requestedBytes,
                      uint32_t targetSampleRate, bool stereo, bool signed16bit);

    // Get native capture format
    MicAudioFormat GetCaptureFormat() const { return captureFormat_; }

    // Volume control (0.0 to 1.0)
    void SetInputGain(float gain);
    float GetInputGain() const { return inputGain_; }

    // State
    MicInputState GetState() const { return state_; }
    const char* GetLastError() const { return lastError_; }

private:
    // WASAPI interfaces
    IMMDeviceEnumerator* deviceEnumerator_;
    IMMDevice* device_;
    IAudioClient* audioClient_;
    IAudioCaptureClient* captureClient_;

    // Capture state
    MicInputState state_;
    MicAudioFormat captureFormat_;
    WAVEFORMATEX* mixFormat_;

    // Capture thread
    HANDLE captureThread_;
    HANDLE stopEvent_;
    bool threadRunning_;

    // Audio buffer
    MicCircularBuffer audioBuffer_;

    // Resampling state
    std::vector<float> resampleBuffer_;
    double resamplePos_;
    float lastSampleL_;
    float lastSampleR_;

    // Settings
    float inputGain_;
    int selectedDeviceIndex_;

    // Error handling
    char lastError_[256];
    void SetError(const char* format, ...);

    // Internal methods
    bool InitializeDevice();
    void ReleaseDevice();
    static DWORD WINAPI CaptureThreadProc(LPVOID param);
    void CaptureLoop();

    // Sample rate conversion
    size_t ResampleAndConvert(const float* input, size_t inputFrames,
                              uint8_t* output, size_t maxOutputBytes,
                              uint32_t targetRate, bool stereo, bool signed16bit);
};

// Global microphone input instance
extern MicrophoneInput* g_MicrophoneInput;

// Initialization functions called from DOSBox-X startup
bool MIC_Initialize();
void MIC_Shutdown();

// Check if microphone input is available and enabled
bool MIC_IsAvailable();

// Generate microphone input samples for SoundBlaster DMA
// This replaces gen_input_silence/hiss/tone when microphone source is selected
void MIC_GenerateInput(uint8_t* buffer, size_t bytes,
                       uint32_t sampleRate, bool stereo, bool signed16bit);

// Get a single sample for Direct ADC mode (DSP command 0x20)
// Used by Creative Parrot and other programs that poll for samples
uint8_t MIC_GetDirectADCSample();

#endif // WIN32
#endif // DOSBOX_MIC_INPUT_WIN32_H
