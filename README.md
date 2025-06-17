Copyright © 2025 AmateurTools DSP (Josh Gura)
All rights reserved.

This software is provided for personal use only.
You may not copy, modify, distribute, sublicense, or sell any part of this software, in whole or in part, without explicit, prior written permission from the copyright holder.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.
🎛️ FM Engine (Working Title)

formerly Voltage Phase Module – Built with JUCE 8.07

    A modular FM engine conceived as a delay effect with advanced audio-rate modulation. Inspired by classic FM synthesis, but focused on creative signal processing for modern production.
    Audio Demo: Piano modulating itself, then with Sine waves

📝 Overview

FM Engine is a JUCE 8.07 plugin module centered on a robust, modular delay line (Delay.cpp) designed for audio-rate frequency modulation. Unlike traditional FM synths, this module does not generate sound directly; instead, it processes incoming audio, enabling deep creative effects for instruments, vocals, or any source.

    Delay line: Uses cubic interpolation for smooth, time-modulated effects.

    FM Focus: Implements only the frequency modulation section of classic FM synths—no oscillators or direct sound generation.

⚡️ Technical Highlights

    Oversampling:
    Employs JUCE’s built-in oversampling. Plans are underway to implement SIMD-based oversampling for improved efficiency and fidelity.

    Audio-Rate Time Modulation:
    Delay time is modulated at audio rate by a sidechain amplitude signal. Oversampling is crucial to minimize noise when processing complex program material.

    Latency & PDC:
    The plugin supports precise latency reporting and lookahead (PDC), with controls designed for both automation and manual adjustment

    .

📚 Background

    Original article (2014, Bedroom Producers Blog)

The project’s name has evolved: originally thought to be phase modulation, it is correctly frequency modulation. Documentation and code are being updated to reflect this.
🔀 Signal Flow

text
graph TD
    A[stereo + sidechain] --> B[Routing.cpp]
    B --> C[car L, car R, mod L, mod R]
    C --> D[out L]
    C --> E[out R]

Processing equations:

cpp
out L = DelayL(carrier L, maxDelayLength * lowpass(atan(clipper(mod L signal + 1) * 0.5)))
out R = DelayR(carrier R, maxDelayLength * lowpass(atan(clipper(mod R signal + 1) * 0.5)))

    Modulation: Bipolar audio signal is shifted to unipolar, scaled 0–1, and used to modulate delay time at audio rate.

🎚️ Controls

    Second Knob:
    Coarse range control, manages plugin delay compensation (PDC). Not intended for frequent automation.

    Leftmost Dial:
    Designed for real-time automation or manual tweaking.

🛠️ Development Notes

    Core FM processing is in Delay.cpp.

    Delay time is cubic-interpolated for smooth, high-quality modulation.

    SIMD optimization for oversampling is a key future goal.

    Focus on minimizing noise and maximizing signal quality during audio-rate modulation

.

Custom UI controls are being developed for intuitive user interaction

    .

📢 Feedback & Contributions

    Suggestions on oversampling, SIMD, or audio-rate modulation are welcome!

    If you have expertise in advanced delay, routing, or FM techniques, please open an issue or PR.

Thank you for checking out FM Engine! For more details, see the original blog post or explore the source code.
