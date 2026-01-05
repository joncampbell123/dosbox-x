# DOSBox-X SoundBlaster Microphone Implementation Notes

## Build Environment

### Visual Studio 2022 Build Command
```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1' -Arch x86; Set-Location C:\temp\dosx\dosbox-x; msbuild vs\dosbox-x.sln /t:dosbox-x /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v143 /m"
```

### Key Build Notes
- Use `/p:PlatformToolset=v143` for VS 2022 (v142 is VS 2019)
- Close DOSBox-X before rebuilding (executable gets locked)
- If `Microsoft.Cpp.Default.props` not found, install C++ Desktop workload via:
  ```
  winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
  ```

## Testing

### Test Configuration (dosbox-x-clean.conf)
```ini
[sblaster]
sbtype=sb16
sbbase=220
irq=7
dma=1
hdma=5
sbmixer=true
recording source=microphone
```

### Test Programs
1. **SBREC.COM** - DMA-based recording (DSP commands 0x24, 0x2C, etc.)
   - Works well with MIC_GenerateInput()

2. **Creative Parrot** - Direct ADC recording (DSP command 0x20)
   - Polls DSP for individual samples
   - Requires MIC_GetDirectADCSample()
   - Location: `C:\temp\dosx\dos_test\SBPRO\PARROT\`

## SoundBlaster Recording Modes

### DMA Recording (High-level)
- DSP commands: 0x24, 0x2C, 0x98, 0x99, etc.
- Uses DMA transfers for bulk sample data
- Called via `gen_input()` in sblaster.cpp
- Sample rate specified by program

### Direct ADC Recording (Low-level)
- DSP command: 0x20
- Polls for one sample at a time
- Used by Creative Parrot and similar programs
- Polling rate varies: 2700-6400 Hz (average ~4500 Hz)

## Key Technical Findings

### Polling Rate Measurement
From debug logs, Creative Parrot polls at:
- Range: 2700 - 6400 Hz
- Average: ~4500 Hz
- Varies significantly during recording
- Drops to 71-878 Hz during playback

### Sample Rate Selection
- **Wrong approach**: Match sample rate to polling rate dynamically
  - Causes audio artifacts from constantly changing resample rate

- **Wrong approach**: Fixed high rate (22050 Hz) with 1:1 poll mapping
  - Causes slow motion (5x slower) and large delay

- **Correct approach**: Time-based sample selection at average polling rate
  - Use real elapsed time to determine sample position
  - Generate at ~4500 Hz (measured average)
  - Returns sample based on wall-clock time, not poll count

### Time-Based Sample Generation
```cpp
// Calculate which sample corresponds to current real time
double elapsedSec = (now.QuadPart - startTime.QuadPart) / perfFreq.QuadPart;
size_t timeSample = (size_t)(elapsedSec * SAMPLE_RATE);
return buffer[timeSample % BUFFER_SIZE];
```

Benefits:
- Correct playback speed regardless of polling variations
- If Parrot polls faster: some samples duplicated (minor)
- If Parrot polls slower: some samples skipped (minor)
- Overall timing preserved

## Windows WASAPI Audio Capture

### Key Components
- `IMMDeviceEnumerator` - Enumerate audio devices
- `IMMDevice` - Audio device handle
- `IAudioClient` - Audio stream configuration
- `IAudioCaptureClient` - Capture buffer access

### Capture Format
- Windows typically provides: 48kHz, 32-bit float, stereo
- Must resample to DOS rates: 4-22 kHz
- Must convert to 8-bit unsigned or 16-bit signed

### Threading Model
- Capture thread polls every 10ms
- Writes to circular buffer (256KB, ~1.3 sec at 48kHz)
- Main thread reads and resamples on demand

### Common Issues
1. **NOMINMAX** - Define before windows.h to avoid min/max macro conflicts
2. **KSDATAFORMAT_SUBTYPE_IEEE_FLOAT** - May need local GUID definition
3. **COM initialization** - Call `CoInitializeEx()` in each thread

## Files Modified

### src/hardware/mic_input_win32.cpp
- WASAPI microphone capture implementation
- MIC_GenerateInput() for DMA recording
- MIC_GetDirectADCSample() for Direct ADC

### src/hardware/mic_input_win32.h
- Class declarations and global function prototypes

### src/hardware/sblaster.cpp
- Added REC_MICROPHONE enum value
- Modified gen_input() to call MIC_GenerateInput()
- Modified DSP command 0x20 handler for Direct ADC

### src/dosbox.cpp
- Added "microphone" to sb_recording_sources array

### vs/dosbox-x.vcxproj
- Added mic_input_win32.cpp/h to project

## Debugging Tips

### Add Logging
```cpp
#include "logging.h"
LOG_MSG("MIC: poll rate: %.1f Hz", measuredRate);
```

### Check Log File
```
C:\temp\dosx\dosbox-x\bin\Win32\Debug\dosbox-x.log
```

### Monitor DSP Commands
Look for command 0x20 in logs to confirm Direct ADC is being used.

## Performance Considerations

- Pre-buffer 100ms of audio to minimize latency
- Generate samples ahead to prevent underruns
- Use 2048-4096 sample chunks for efficient resampling
- Ring buffer size: 32KB (~7 sec at 4500 Hz)
