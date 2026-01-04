/*
 *  Copyright (C) 2024 DOSBox-X Team
 *
 *  SoundBlaster Microphone Input Support - Windows WASAPI Implementation
 */

#ifdef WIN32

#include "mic_input_win32.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <algorithm>
#include <vector>

// Link required libraries
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

// Define KSDATAFORMAT_SUBTYPE_IEEE_FLOAT GUID locally
// {00000003-0000-0010-8000-00aa00389b71}
static const GUID LOCAL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT =
    { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

// Reference time units (100-nanosecond intervals)
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

// Buffer duration for capture (in milliseconds)
#define CAPTURE_BUFFER_DURATION_MS  100

// Global instance
MicrophoneInput* g_MicrophoneInput = nullptr;

// ============================================================================
// MicCircularBuffer Implementation
// ============================================================================

MicCircularBuffer::MicCircularBuffer(size_t capacity)
    : buffer_(capacity), readPos_(0), writePos_(0), count_(0) {
}

MicCircularBuffer::~MicCircularBuffer() {
}

size_t MicCircularBuffer::Write(const uint8_t* data, size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t capacity = buffer_.size();
    size_t available = capacity - count_;
    size_t toWrite = std::min(bytes, available);

    for (size_t i = 0; i < toWrite; i++) {
        buffer_[writePos_] = data[i];
        writePos_ = (writePos_ + 1) % capacity;
    }
    count_ += toWrite;

    return toWrite;
}

size_t MicCircularBuffer::Read(uint8_t* data, size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t toRead = std::min(bytes, count_);
    size_t capacity = buffer_.size();

    for (size_t i = 0; i < toRead; i++) {
        data[i] = buffer_[readPos_];
        readPos_ = (readPos_ + 1) % capacity;
    }
    count_ -= toRead;

    return toRead;
}

size_t MicCircularBuffer::Available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

void MicCircularBuffer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    readPos_ = 0;
    writePos_ = 0;
    count_ = 0;
}

// ============================================================================
// MicrophoneInput Implementation
// ============================================================================

MicrophoneInput::MicrophoneInput()
    : deviceEnumerator_(nullptr)
    , device_(nullptr)
    , audioClient_(nullptr)
    , captureClient_(nullptr)
    , state_(MIC_STATE_CLOSED)
    , mixFormat_(nullptr)
    , captureThread_(nullptr)
    , stopEvent_(nullptr)
    , threadRunning_(false)
    , resamplePos_(0.0)
    , lastSampleL_(0.0f)
    , lastSampleR_(0.0f)
    , inputGain_(1.0f)  // Normal gain (no amplification)
    , selectedDeviceIndex_(-1) {

    memset(&captureFormat_, 0, sizeof(captureFormat_));
    memset(lastError_, 0, sizeof(lastError_));
}

MicrophoneInput::~MicrophoneInput() {
    Shutdown();
}

void MicrophoneInput::SetError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(lastError_, sizeof(lastError_), format, args);
    va_end(args);
}

bool MicrophoneInput::Initialize() {
    if (state_ != MIC_STATE_CLOSED) {
        return true;  // Already initialized
    }

    // Initialize COM for this thread
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        SetError("Failed to initialize COM: 0x%08X", hr);
        return false;
    }

    // Create device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&deviceEnumerator_
    );

    if (FAILED(hr)) {
        SetError("Failed to create device enumerator: 0x%08X", hr);
        return false;
    }

    // Create stop event for capture thread
    stopEvent_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent_) {
        SetError("Failed to create stop event");
        ReleaseDevice();
        return false;
    }

    state_ = MIC_STATE_OPEN;
    return true;
}

void MicrophoneInput::Shutdown() {
    StopCapture();
    ReleaseDevice();

    if (stopEvent_) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }

    if (deviceEnumerator_) {
        deviceEnumerator_->Release();
        deviceEnumerator_ = nullptr;
    }

    state_ = MIC_STATE_CLOSED;
}

std::vector<std::wstring> MicrophoneInput::GetDeviceList() {
    std::vector<std::wstring> devices;

    if (!deviceEnumerator_) {
        return devices;
    }

    IMMDeviceCollection* collection = nullptr;
    HRESULT hr = deviceEnumerator_->EnumAudioEndpoints(
        eCapture, DEVICE_STATE_ACTIVE, &collection);

    if (FAILED(hr)) {
        return devices;
    }

    UINT count = 0;
    collection->GetCount(&count);

    for (UINT i = 0; i < count; i++) {
        IMMDevice* device = nullptr;
        if (SUCCEEDED(collection->Item(i, &device))) {
            IPropertyStore* props = nullptr;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
                PROPVARIANT varName;
                PropVariantInit(&varName);
                if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
                    devices.push_back(varName.pwszVal);
                    PropVariantClear(&varName);
                }
                props->Release();
            }
            device->Release();
        }
    }

    collection->Release();
    return devices;
}

bool MicrophoneInput::SelectDevice(int deviceIndex) {
    if (state_ == MIC_STATE_CAPTURING) {
        StopCapture();
    }

    ReleaseDevice();
    selectedDeviceIndex_ = deviceIndex;

    return InitializeDevice();
}

std::wstring MicrophoneInput::GetCurrentDeviceName() const {
    if (!device_) {
        return L"(none)";
    }

    IPropertyStore* props = nullptr;
    std::wstring name = L"Unknown Device";

    if (SUCCEEDED(device_->OpenPropertyStore(STGM_READ, &props))) {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
            name = varName.pwszVal;
            PropVariantClear(&varName);
        }
        props->Release();
    }

    return name;
}

bool MicrophoneInput::InitializeDevice() {
    HRESULT hr;

    // Get the selected device or default
    if (selectedDeviceIndex_ < 0) {
        hr = deviceEnumerator_->GetDefaultAudioEndpoint(
            eCapture, eConsole, &device_);
    } else {
        IMMDeviceCollection* collection = nullptr;
        hr = deviceEnumerator_->EnumAudioEndpoints(
            eCapture, DEVICE_STATE_ACTIVE, &collection);
        if (SUCCEEDED(hr)) {
            hr = collection->Item(selectedDeviceIndex_, &device_);
            collection->Release();
        }
    }

    if (FAILED(hr) || !device_) {
        SetError("Failed to get audio capture device: 0x%08X", hr);
        return false;
    }

    // Activate audio client
    hr = device_->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        (void**)&audioClient_
    );

    if (FAILED(hr)) {
        SetError("Failed to activate audio client: 0x%08X", hr);
        ReleaseDevice();
        return false;
    }

    // Get default format
    hr = audioClient_->GetMixFormat(&mixFormat_);
    if (FAILED(hr)) {
        SetError("Failed to get mix format: 0x%08X", hr);
        ReleaseDevice();
        return false;
    }

    // Store capture format info
    captureFormat_.sampleRate = mixFormat_->nSamplesPerSec;
    captureFormat_.channels = mixFormat_->nChannels;

    // Handle extended format
    if (mixFormat_->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE* extFormat = (WAVEFORMATEXTENSIBLE*)mixFormat_;
        captureFormat_.bitsPerSample = extFormat->Samples.wValidBitsPerSample;
    } else {
        captureFormat_.bitsPerSample = mixFormat_->wBitsPerSample;
    }

    // Initialize audio client for capture
    REFERENCE_TIME bufferDuration = CAPTURE_BUFFER_DURATION_MS * REFTIMES_PER_MILLISEC;
    hr = audioClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        bufferDuration,
        0,
        mixFormat_,
        nullptr
    );

    if (FAILED(hr)) {
        SetError("Failed to initialize audio client: 0x%08X", hr);
        ReleaseDevice();
        return false;
    }

    // Get capture client
    hr = audioClient_->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&captureClient_
    );

    if (FAILED(hr)) {
        SetError("Failed to get capture client: 0x%08X", hr);
        ReleaseDevice();
        return false;
    }

    return true;
}

void MicrophoneInput::ReleaseDevice() {
    if (captureClient_) {
        captureClient_->Release();
        captureClient_ = nullptr;
    }

    if (audioClient_) {
        audioClient_->Release();
        audioClient_ = nullptr;
    }

    if (device_) {
        device_->Release();
        device_ = nullptr;
    }

    if (mixFormat_) {
        CoTaskMemFree(mixFormat_);
        mixFormat_ = nullptr;
    }
}

bool MicrophoneInput::StartCapture() {
    if (state_ == MIC_STATE_CAPTURING) {
        return true;  // Already capturing
    }

    if (!audioClient_ || !captureClient_) {
        if (!InitializeDevice()) {
            return false;
        }
    }

    // Clear buffer and reset resampling state
    audioBuffer_.Clear();
    resamplePos_ = 0.0;
    lastSampleL_ = 0.0f;
    lastSampleR_ = 0.0f;

    // Reset stop event
    ResetEvent(stopEvent_);

    // Start audio client
    HRESULT hr = audioClient_->Start();
    if (FAILED(hr)) {
        SetError("Failed to start audio client: 0x%08X", hr);
        state_ = MIC_STATE_ERROR;
        return false;
    }

    // Start capture thread
    threadRunning_ = true;
    captureThread_ = CreateThread(
        nullptr, 0, CaptureThreadProc, this, 0, nullptr);

    if (!captureThread_) {
        SetError("Failed to create capture thread");
        audioClient_->Stop();
        state_ = MIC_STATE_ERROR;
        return false;
    }

    state_ = MIC_STATE_CAPTURING;
    return true;
}

void MicrophoneInput::StopCapture() {
    if (state_ != MIC_STATE_CAPTURING) {
        return;
    }

    // Signal thread to stop
    threadRunning_ = false;
    SetEvent(stopEvent_);

    // Wait for thread to finish
    if (captureThread_) {
        WaitForSingleObject(captureThread_, 1000);
        CloseHandle(captureThread_);
        captureThread_ = nullptr;
    }

    // Stop audio client
    if (audioClient_) {
        audioClient_->Stop();
    }

    state_ = MIC_STATE_OPEN;
}

DWORD WINAPI MicrophoneInput::CaptureThreadProc(LPVOID param) {
    MicrophoneInput* mic = static_cast<MicrophoneInput*>(param);

    // Initialize COM for this thread
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    mic->CaptureLoop();

    CoUninitialize();
    return 0;
}

void MicrophoneInput::CaptureLoop() {
    // Capture interval (process every 10ms)
    const DWORD captureInterval = 10;

    while (threadRunning_) {
        // Wait for data or stop signal
        DWORD waitResult = WaitForSingleObject(stopEvent_, captureInterval);
        if (waitResult == WAIT_OBJECT_0) {
            break;  // Stop signaled
        }

        if (!captureClient_) {
            break;
        }

        UINT32 packetLength = 0;
        HRESULT hr = captureClient_->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) {
            continue;
        }

        while (packetLength > 0) {
            BYTE* data = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;

            hr = captureClient_->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
            if (FAILED(hr)) {
                break;
            }

            if (numFrames > 0) {
                // Calculate bytes
                size_t bytesPerFrame = mixFormat_->nBlockAlign;
                size_t totalBytes = numFrames * bytesPerFrame;

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    // Generate silence in the expected format
                    std::vector<uint8_t> silenceData(totalBytes, 0);
                    audioBuffer_.Write(silenceData.data(), totalBytes);
                } else if (data) {
                    // Write actual audio data to circular buffer
                    audioBuffer_.Write(data, totalBytes);
                }
            }

            captureClient_->ReleaseBuffer(numFrames);
            captureClient_->GetNextPacketSize(&packetLength);
        }
    }
}

void MicrophoneInput::SetInputGain(float gain) {
    inputGain_ = std::max(0.0f, std::min(1000.0f, gain));
}

size_t MicrophoneInput::GetSamples(uint8_t* buffer, size_t requestedBytes,
                                    uint32_t targetSampleRate, bool stereo,
                                    bool signed16bit) {
    if (state_ != MIC_STATE_CAPTURING) {
        memset(buffer, signed16bit ? 0 : 0x80, requestedBytes);
        return requestedBytes;
    }

    size_t targetBytesPerSample = signed16bit ? 2 : 1;
    size_t targetChannels = stereo ? 2 : 1;
    size_t targetBytesPerFrame = targetBytesPerSample * targetChannels;
    size_t targetFrames = requestedBytes / targetBytesPerFrame;

    // Calculate source frames needed
    // Use ceil + 1 to ensure we always have enough input for the resampler.
    // At 48000->44100, ratio=1.088, so for 882 output frames we need ~960 input.
    // The +1 accounts for floating point rounding and ensures no underrun.
    double ratio = (double)captureFormat_.sampleRate / (double)targetSampleRate;
    size_t sourceFramesNeeded = (size_t)ceil(targetFrames * ratio) + 1;

    size_t sourceBytesPerFrame = mixFormat_->nBlockAlign;
    size_t sourceBytesNeeded = sourceFramesNeeded * sourceBytesPerFrame;

    // Ensure we have enough buffer space
    if (resampleBuffer_.size() < sourceBytesNeeded / sizeof(float) + 1) {
        resampleBuffer_.resize(sourceBytesNeeded / sizeof(float) + 1);
    }
    uint8_t* sourceData = reinterpret_cast<uint8_t*>(resampleBuffer_.data());

    // Read from circular buffer - only what we need
    size_t bytesRead = audioBuffer_.Read(sourceData, sourceBytesNeeded);

    if (bytesRead == 0) {
        // No data - fill with last known sample to avoid clicks
        if (signed16bit) {
            int16_t* out = (int16_t*)buffer;
            int16_t lastVal = (int16_t)(lastSampleL_ * 32767.0f);
            for (size_t i = 0; i < requestedBytes / 2; i++) {
                out[i] = lastVal;
            }
        } else {
            uint8_t lastVal = (uint8_t)(lastSampleL_ * 127.0f + 128.0f);
            memset(buffer, lastVal, requestedBytes);
        }
        return requestedBytes;
    }

    size_t sourceFrames = bytesRead / sourceBytesPerFrame;

    // Convert to float
    std::vector<float> floatData(sourceFrames * 2);

    bool sourceFloat = (captureFormat_.bitsPerSample == 32) ||
        (mixFormat_->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
        (mixFormat_->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
         IsEqualGUID(((WAVEFORMATEXTENSIBLE*)mixFormat_)->SubFormat, LOCAL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));

    for (size_t i = 0; i < sourceFrames; i++) {
        float left, right;

        if (sourceFloat) {
            float* src = (float*)(sourceData + i * sourceBytesPerFrame);
            left = src[0];
            right = (captureFormat_.channels >= 2) ? src[1] : src[0];
        } else if (captureFormat_.bitsPerSample == 16) {
            int16_t* src = (int16_t*)(sourceData + i * sourceBytesPerFrame);
            left = src[0] / 32768.0f;
            right = (captureFormat_.channels >= 2) ? src[1] / 32768.0f : left;
        } else {
            uint8_t* src = sourceData + i * sourceBytesPerFrame;
            left = (src[0] - 128) / 128.0f;
            right = (captureFormat_.channels >= 2) ? (src[1] - 128) / 128.0f : left;
        }

        floatData[i * 2] = left * inputGain_;
        floatData[i * 2 + 1] = right * inputGain_;
    }

    // Store last sample for continuity
    if (sourceFrames > 0) {
        lastSampleL_ = floatData[(sourceFrames - 1) * 2];
        lastSampleR_ = floatData[(sourceFrames - 1) * 2 + 1];
    }

    // Resample and convert
    size_t bytesWritten = ResampleAndConvert(floatData.data(), sourceFrames, buffer, requestedBytes,
                                              targetSampleRate, stereo, signed16bit);

    // If we didn't fill the buffer, pad with last sample (avoid silence gaps)
    if (bytesWritten < requestedBytes) {
        if (signed16bit) {
            int16_t* out = (int16_t*)buffer;
            int16_t lastVal = (bytesWritten >= 2) ? out[bytesWritten/2 - 1] : 0;
            for (size_t i = bytesWritten / 2; i < requestedBytes / 2; i++) {
                out[i] = lastVal;
            }
        } else {
            uint8_t lastVal = (bytesWritten >= 1) ? buffer[bytesWritten - 1] : 0x80;
            memset(buffer + bytesWritten, lastVal, requestedBytes - bytesWritten);
        }
    }

    return requestedBytes;
}

size_t MicrophoneInput::ResampleAndConvert(const float* input, size_t inputFrames,
                                            uint8_t* output, size_t maxOutputBytes,
                                            uint32_t targetRate, bool stereo,
                                            bool signed16bit) {
    if (inputFrames == 0) {
        return 0;
    }

    size_t bytesPerSample = signed16bit ? 2 : 1;
    size_t channels = stereo ? 2 : 1;
    size_t bytesPerFrame = bytesPerSample * channels;
    size_t maxOutputFrames = maxOutputBytes / bytesPerFrame;

    // Simple decimation: pick every Nth sample
    // For 48000 -> 11025, we pick roughly every 4.35th sample
    double step = (double)captureFormat_.sampleRate / (double)targetRate;
    size_t outputFrames = 0;
    double pos = 0.0;

    while (outputFrames < maxOutputFrames && (size_t)pos < inputFrames) {
        size_t idx = (size_t)pos;

        // Get sample (nearest neighbor - simple and click-free)
        float left = input[idx * 2];
        float right = input[idx * 2 + 1];

        // Clamp to valid range
        left = std::max(-1.0f, std::min(1.0f, left));
        right = std::max(-1.0f, std::min(1.0f, right));

        // Convert to target format
        if (signed16bit) {
            int16_t* out = (int16_t*)(output + outputFrames * bytesPerFrame);
            out[0] = (int16_t)(left * 32767.0f);
            if (stereo) {
                out[1] = (int16_t)(right * 32767.0f);
            }
        } else {
            // 8-bit unsigned
            uint8_t* out = output + outputFrames * bytesPerFrame;
            out[0] = (uint8_t)((left * 127.0f) + 128.0f);
            if (stereo) {
                out[1] = (uint8_t)((right * 127.0f) + 128.0f);
            }
        }

        outputFrames++;
        pos += step;
    }

    return outputFrames * bytesPerFrame;
}

// ============================================================================
// Global Functions
// ============================================================================

bool MIC_Initialize() {
    if (g_MicrophoneInput) {
        return true;  // Already initialized
    }

    g_MicrophoneInput = new MicrophoneInput();
    if (!g_MicrophoneInput->Initialize()) {
        delete g_MicrophoneInput;
        g_MicrophoneInput = nullptr;
        return false;
    }

    // Start capturing immediately
    if (!g_MicrophoneInput->StartCapture()) {
        delete g_MicrophoneInput;
        g_MicrophoneInput = nullptr;
        return false;
    }

    return true;
}

void MIC_Shutdown() {
    if (g_MicrophoneInput) {
        g_MicrophoneInput->Shutdown();
        delete g_MicrophoneInput;
        g_MicrophoneInput = nullptr;
    }
}

bool MIC_IsAvailable() {
    return g_MicrophoneInput != nullptr &&
           g_MicrophoneInput->GetState() != MIC_STATE_CLOSED &&
           g_MicrophoneInput->GetState() != MIC_STATE_ERROR;
}

void MIC_GenerateInput(uint8_t* buffer, size_t bytes,
                       uint32_t sampleRate, bool stereo, bool signed16bit) {
    if (!g_MicrophoneInput || g_MicrophoneInput->GetState() != MIC_STATE_CAPTURING) {
        // Fill with silence
        if (signed16bit) {
            memset(buffer, 0, bytes);
        } else {
            memset(buffer, 0x80, bytes);  // 8-bit unsigned silence = 128
        }
        return;
    }

    g_MicrophoneInput->GetSamples(buffer, bytes, sampleRate, stereo, signed16bit);
}

// Direct ADC - Time-based continuous buffer approach
// This decouples the DOS polling rate from sample generation,
// ensuring smooth audio regardless of polling irregularities.
static uint8_t directADCBuffer[32768];  // Large ring buffer (~8 sec at 4kHz)
static const size_t DIRECT_ADC_BUFFER_SIZE = sizeof(directADCBuffer);
static const uint32_t DIRECT_ADC_SAMPLE_RATE = 4000;  // Target sample rate

static LARGE_INTEGER directADCPerfFreq;
static LARGE_INTEGER directADCStartTime;
static bool directADCInitialized = false;
static size_t directADCGeneratedSamples = 0;  // Total samples generated since start
static size_t directADCConsumedSamples = 0;   // Total samples returned to caller

// Helper: Generate samples up to a target position
static void DirectADC_GenerateTo(size_t targetSample) {
    if (!g_MicrophoneInput || g_MicrophoneInput->GetState() != MIC_STATE_CAPTURING) {
        return;
    }

    while (directADCGeneratedSamples < targetSample) {
        // Generate in chunks of 1024 samples
        size_t chunkSize = 1024;
        size_t writePos = directADCGeneratedSamples % DIRECT_ADC_BUFFER_SIZE;

        // Handle wrap-around
        size_t availableSpace = DIRECT_ADC_BUFFER_SIZE - writePos;
        size_t toGenerate = (chunkSize < availableSpace) ? chunkSize : availableSpace;

        g_MicrophoneInput->GetSamples(
            &directADCBuffer[writePos],
            toGenerate,
            DIRECT_ADC_SAMPLE_RATE,
            false,   // mono
            false    // 8-bit unsigned
        );

        directADCGeneratedSamples += toGenerate;
    }
}

uint8_t MIC_GetDirectADCSample() {
    if (!g_MicrophoneInput || g_MicrophoneInput->GetState() != MIC_STATE_CAPTURING) {
        return 0x80;  // Silence
    }

    // Initialize on first call
    if (!directADCInitialized) {
        QueryPerformanceFrequency(&directADCPerfFreq);
        QueryPerformanceCounter(&directADCStartTime);
        directADCGeneratedSamples = 0;
        directADCConsumedSamples = 0;

        // Pre-generate 500ms of audio (2000 samples at 4kHz)
        DirectADC_GenerateTo(2000);

        directADCInitialized = true;
    }

    // Calculate current time position in samples
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double elapsedSec = (double)(now.QuadPart - directADCStartTime.QuadPart) /
                        (double)directADCPerfFreq.QuadPart;
    size_t currentTimeSample = (size_t)(elapsedSec * DIRECT_ADC_SAMPLE_RATE);

    // Ensure we have audio generated ahead of current time
    // Generate at least 200ms ahead to avoid underruns
    size_t targetGenerate = currentTimeSample + (DIRECT_ADC_SAMPLE_RATE / 5);  // +200ms
    if (directADCGeneratedSamples < targetGenerate) {
        DirectADC_GenerateTo(targetGenerate);
    }

    // Return the sample at the current time position
    // This ensures smooth playback regardless of polling irregularities
    size_t sampleToReturn = currentTimeSample;

    // Prevent reading samples we haven't generated yet
    if (sampleToReturn >= directADCGeneratedSamples) {
        sampleToReturn = directADCGeneratedSamples - 1;
    }

    // Handle the case where generated samples wrap around the ring buffer
    // We need to ensure we don't read stale data
    size_t oldestValidSample = 0;
    if (directADCGeneratedSamples > DIRECT_ADC_BUFFER_SIZE) {
        oldestValidSample = directADCGeneratedSamples - DIRECT_ADC_BUFFER_SIZE;
    }
    if (sampleToReturn < oldestValidSample) {
        sampleToReturn = oldestValidSample;
    }

    size_t bufferPos = sampleToReturn % DIRECT_ADC_BUFFER_SIZE;
    return directADCBuffer[bufferPos];
}

#endif // WIN32
