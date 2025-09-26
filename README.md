# 🎛️ FM Engine
**A Modular FM Delay Processor for Creative Audio Production**

[![License](https://img.shields.io/badge/License-Proprietary-red.svg)](LICENSE)
[![JUCE](https://img.shields.io/badge/JUCE-8.07-blue.svg)](https://juce.com/)
[![Platform](https://img.shields.io/badge/Platform-VST3-green.svg)](https://www.steinberg.net/vst/)
[![C++](https://img.shields.io/badge/C++-17-orange.svg)](https://isocpp.org/)

> **Formerly known as:** Voltage Phase Module (VPM)  
> **Audio Demo:** [Piano Self-Modulation + Sine Waves](https://soundcloud.com/florianhertz/vpm2_2025/)

---

## 🚀 Overview

Think of FM Engine as *just* the FM part of an FM synthesizer. Except it allows the oft dreamt of
applying FM to any two sources, using one as the carrier and one as the modulator. 
Smooth sine shaped modulators sound better, but it can be used in as many ways as you can
imagine. Obviously if you use modulators that contain high frequencies or noise it will just
result in noise, so there's a built in low pass filter. However, try feeding it two different
sine synths, or playing with a single sine synth in mode 1 and play a chord.

### ✨ Key Features

- **🔄 Modular FM Processing** - Pure frequency modulation without oscillators
- **🎯 Audio-Rate Time Modulation** - Delay time modulated by sidechain amplitude
- **📈 Advanced Oversampling** - JUCE built-in with planned SIMD optimization
- **🎛️ Flexible Routing** - Three algorithms supporting mono to full stereo processing
- **⚡ Low Latency** - Precise PDC (Plugin Delay Compensation) support
- **🔊 Multiple Interpolation** - Linear and Lagrange cubic interpolation

---

## 🏗️ Architecture

### Signal Flow Diagram

```mermaid
flowchart TD
    %% Input Stage
    subgraph "Input Sources"
        L[Main Input L]
        R[Main Input R]
        SC_L[Sidechain L]
        SC_R[Sidechain R]
    end
    
    %% Routing Logic
    subgraph "Routing Matrix"
        ALG0[Algorithm 0<br/>Mono Car/Mod]
        ALG1[Algorithm 1<br/>Mono Mix]
        ALG2[Algorithm 2<br/>Full Stereo]
        INV{Invert?}
    end
    
    %% Processing Chain
    subgraph "Signal Processing"
        MOD_PROC[Modulator Processing]
        SMOOTH[Parameter Smoothing]
        CLIP[Tanh Soft Clipping]
        LPF[Lowpass Filter]
        NORM[Normalization]
    end
    
    %% Delay Engine
    subgraph "FM Delay Engine"
        OS_UP[Oversampling ↑]
        DELAY_L[Delay Line L<br/>Cubic Interpolation]
        DELAY_R[Delay Line R<br/>Cubic Interpolation]
        OS_DOWN[Oversampling ↓]
    end
    
    %% Output Stage
    subgraph "Output"
        OUT_L[Output L]
        OUT_R[Output R]
    end
    
    %% Connections
    L --> ALG0
    R --> ALG0
    L --> ALG1
    R --> ALG1
    SC_L --> ALG1
    SC_R --> ALG1
    L --> ALG2
    R --> ALG2
    SC_L --> ALG2
    SC_R --> ALG2
    
    ALG0 --> INV
    ALG1 --> INV
    ALG2 --> INV
    
    INV --> MOD_PROC
    MOD_PROC --> SMOOTH
    SMOOTH --> CLIP
    CLIP --> LPF
    LPF --> NORM
    
    NORM --> OS_UP
    OS_UP --> DELAY_L
    OS_UP --> DELAY_R
    DELAY_L --> OS_DOWN
    DELAY_R --> OS_DOWN
    
    OS_DOWN --> OUT_L
    OS_DOWN --> OUT_R
    
    %% Styling
    classDef input fill:#e1f5fe
    classDef processing fill:#f3e5f5
    classDef delay fill:#fff3e0
    classDef output fill:#e8f5e8
    
    class L,R,SC_L,SC_R input
    class ALG0,ALG1,ALG2,INV,MOD_PROC,SMOOTH,CLIP,LPF,NORM processing
    class OS_UP,DELAY_L,DELAY_R,OS_DOWN delay
    class OUT_L,OUT_R output
```

### Algorithm Routing Matrix

```mermaid
graph LR
    subgraph "Algorithm 0: Mono Carrier/Modulator"
        A0_L[Input L] --> A0_CL[Carrier L]
        A0_L --> A0_CR[Carrier R]
        A0_R[Input R] --> A0_ML[Modulator L]
        A0_R --> A0_MR[Modulator R]
    end
    
    subgraph "Algorithm 1: Mono Mix"
        A1_L[Input L] --> A1_MIX[LR Mix]
        A1_R[Input R] --> A1_MIX
        A1_SC_L[SC L] --> A1_SC_MIX[SC Mix]
        A1_SC_R[SC R] --> A1_SC_MIX
        A1_MIX --> A1_CL[Carrier L]
        A1_MIX --> A1_CR[Carrier R]
        A1_SC_MIX --> A1_ML[Modulator L]
        A1_SC_MIX --> A1_MR[Modulator R]
    end
    
    subgraph "Algorithm 2: Full Stereo"
        A2_L[Input L] --> A2_CL[Carrier L]
        A2_R[Input R] --> A2_CR[Carrier R]
        A2_SC_L[SC L] --> A2_ML[Modulator L]
        A2_SC_R[SC R] --> A2_MR[Modulator R]
    end
```

---

## 📊 Technical Specifications

### Core Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **Modulation Depth** | 0.0 - 1.0 | 0.0 | Controls FM intensity with 0.5 skew |
| **Max Delay Range** | 10ms/100ms/1000ms | 100ms | Maximum delay time range |
| **Algorithm** | 0-2 | 0 | Routing algorithm selection |
| **Invert** | On/Off | Off | Swaps carrier and modulator |
| **Lowpass Cutoff** | 30Hz - 20kHz | 20kHz | Modulator filtering (log scale) |
| **PDC** | On/Off | Off | Plugin Delay Compensation |
| **Interpolation** | Linear/Lagrange | Linear | Delay line interpolation type |
| **Oversampling** | On/Off | Off | 2x oversampling for quality |

### Processing Equations

The core delay modulation follows these equations:

```
out_L = DelayL(carrier_L, maxDelayLength × lowpass(atan(clipper(modulator_L + 1) × 0.5)))
out_R = DelayR(carrier_R, maxDelayLength × lowpass(atan(clipper(modulator_R + 1) × 0.5)))
```

### PDC Mode Processing

When PDC is enabled, the plugin uses bipolar modulation mapping:

```mermaid
graph TB
    subgraph "PDC Disabled (Standard)"
        STD_MOD[Modulator Signal<br/>-1.0 to +1.0] --> STD_NORM[Normalize<br/>(mod + 1) × 0.5] --> STD_DELAY[Delay Time<br/>0 to maxDelay]
    end
    
    subgraph "PDC Enabled (Bipolar)"
        PDC_MOD[Modulator Signal<br/>-1.0 to +1.0] --> PDC_BASE{mod < 0?}
        PDC_BASE -->|Yes| PDC_NEG[base + mod × base]
        PDC_BASE -->|No| PDC_POS[base + mod × (max - base)]
        PDC_NEG --> PDC_OUT[Delay Time]
        PDC_POS --> PDC_OUT
    end
```

---

## 🔧 Building & Installation

### Prerequisites

- **JUCE Framework** 8.07 or higher
- **C++17** compatible compiler
- **CMake** 3.15+ (for CMake builds) or **Projucer**

### Build Instructions

#### Using Projucer
1. Open `FM_Engine.jucer` in Projucer
2. Configure your target IDE/build system
3. Export and build in your IDE

#### Using CMake
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Installation
1. Copy the built VST3 to your plugin directory:
   - **Windows:** `C:\Program Files\Common Files\VST3\`
   - **macOS:** `~/Library/Audio/Plug-Ins/VST3/`
   - **Linux:** `~/.vst3/`

---

## 🎵 Usage Examples

### Creative Applications

1. **Self-Modulating Instruments**
   - Route piano → Algorithm 0 → Use velocity as modulator
   - Creates dynamic FM textures based on playing dynamics

2. **Sidechain FM Effects**
   - Main: Vocal, Sidechain: Kick drum
   - Algorithm 2 for stereo sidechain modulation
   - Rhythmic vocal texturing

3. **Stereo Image Manipulation**
   - Algorithm 2 with independent L/R sidechain sources
   - Creates complex stereo movement and spatial effects

### Parameter Automation Tips

- **Modulation Depth**: Automate for dramatic build-ups
- **Max Delay Range**: Switch between ranges for different effects
- **Algorithm**: Real-time routing changes for arrangement dynamics
- **Lowpass Cutoff**: Filter sweeps on the modulator signal

---

## 🔬 Technical Deep Dive

### Delay Line Implementation

The `InterpolatedDelay` class features:

- **Circular Buffer**: Efficient memory usage with wraparound
- **Lagrange Interpolation**: 3rd-order polynomial for smooth delays
- **Oversampling Support**: Handles 2x oversampled processing
- **PDC Integration**: Bipolar modulation for advanced timing control

### Routing System

The `routeSample()` function implements three distinct algorithms:

```cpp
// Algorithm 0: Mono Carrier/Modulator
carrier = {L, L}, modulator = {R, R}

// Algorithm 1: Mono Mix
carrier = {(L+R)/2, (L+R)/2}, modulator = {(SC_L+SC_R)/2, (SC_L+SC_R)/2}

// Algorithm 2: Full Stereo  
carrier = {L, R}, modulator = {SC_L, SC_R}
```

### Oversampling Pipeline

```mermaid
sequenceDiagram
    participant Input
    participant Router
    participant Oversampler
    participant DelayEngine
    participant Output
    
    Input->>Router: Audio Block
    Router->>Oversampler: Routed Channels
    Oversampler->>Oversampler: Upsample 2x
    Oversampler->>DelayEngine: Process at 2x Rate
    DelayEngine->>DelayEngine: FM Processing
    DelayEngine->>Oversampler: Processed Audio
    Oversampler->>Oversampler: Downsample
    Oversampler->>Output: Final Audio
```

---

## 🐛 Troubleshooting

### Common Issues

**No Sidechain Input**
- Ensure your DAW supports sidechain routing
- Check that the sidechain bus is enabled in your DAW
- Verify Algorithm 1 or 2 is selected for sidechain processing

**High CPU Usage**
- Disable oversampling if not needed
- Use Linear interpolation instead of Lagrange
- Reduce the maximum delay range

**Artifacts/Clicking**
- Enable oversampling for complex program material
- Adjust the lowpass cutoff to filter high-frequency modulation
- Check that input levels aren't clipping the modulator

---

## 📈 Roadmap

### Upcoming Features

- [ ] **SIMD Oversampling** - Custom implementation for better performance
- [ ] **Multi-tap Delays** - Multiple delay taps with independent modulation
- [ ] **LFO Modulation** - Built-in LFOs for modulator enhancement
- [ ] **Preset System** - Save/load custom configurations
- [ ] **Visual Feedback** - Real-time delay time and modulation visualization

### Performance Optimizations

- [ ] Buffer optimization for real-time performance
- [ ] SIMD processing for delay calculations
- [ ] Improved parameter smoothing algorithms

---

## 📚 Background & History

FM Engine evolved from the original **Voltage Phase Module** concept, first documented in a 2014 [Bedroom Producers Blog article](https://bedroomproducersblog.com/2014/06/18/voltage-phase-module/). The project has undergone significant development:

- **2014**: Initial phase modulation concept
- **2024**: Realization that the effect is actually frequency modulation
- **2025**: Complete rewrite with JUCE 8.07 and modern architecture

The name change from "Voltage Phase Module" to "FM Engine" reflects the correct understanding of the underlying DSP principles.

---

## 🤝 Contributing

This is a proprietary plugin developed by AmateurTools DSP. The source code is provided for educational purposes only.

### License

```
Copyright © 2025 AmateurTools DSP (Josh Gura)
All rights reserved.

This software is provided for personal use only.
You may not copy, modify, distribute, sublicense, or sell any part 
of this software, in whole or in part, without explicit, prior 
written permission from the copyright holder.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
EXPRESS OR IMPLIED.
```

---

## 📞 Support

For technical support or questions about FM Engine:

- **Email**: [Contact AmateurTools DSP]
- **Audio Demo**: [SoundCloud Demo](https://soundcloud.com/florianhertz/vpm2_2025/)
- **Original Article**: [Bedroom Producers Blog](https://bedroomproducersblog.com/2014/06/18/voltage-phase-module/)

---

*Built with ❤️ using JUCE 8.07*
