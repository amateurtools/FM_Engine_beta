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
# 🔀 Signal Flow

The FM Engine’s routing is flexible, supporting three primary algorithms and optional carrier/modulator inversion. The routing logic is implemented in `Routing.cpp` and operates as follows:

## **Routing Algorithms**

- **Algorithm 0:**  
  - *Mono Carrier, Mono Modulator*  
    - Carrier: Left input duplicated to both channels  
    - Modulator: Right input duplicated to both channels

- **Algorithm 1:**  
  - *Mono Mix*  
    - Carrier: Average of L+R sent to both channels  
    - Modulator: Average of SC_L+SC_R (sidechain) sent to both channels

- **Algorithm 2:**  
  - *Full Stereo*  
    - Carrier: L and R inputs preserved  
    - Modulator: SC_L and SC_R (sidechain) preserved

- **Invert:**  
  - If enabled, swaps the carrier and modulator channels.

---

## **Signal Flow Diagram**

flowchart TD
A[Input L] -->|Algorithm 0/1: Mono mix or duplicate| B[Carrier L]
B --> F[Delay L]
A2[Input R] -->|Algorithm 0: Modulator| C[Modulator L]
C --> G[Delay L]
D[SC_L (Sidechain L)] -->|Algorithm 1/2: Modulator| E[Modulator L]
E --> G
D2[SC_R (Sidechain R)] -->|Algorithm 2: Modulator| H[Modulator R]
H --> I[Delay R]
A3[Input L] -->|Algorithm 2: Carrier| J[Carrier L]
J --> F
A4[Input R] -->|Algorithm 2: Carrier| K[Carrier R]
K --> I
%% Output nodes
F --> O[Output L]
I --> P[Output R]
G -.-> O
H -.-> P


---

## **Routing Logic Summary**

- **Inputs:**  
  - Main stereo input: L, R  
  - Sidechain stereo input: SC_L, SC_R

- **Outputs:**  
  - Carrier (L, R)  
  - Modulator (L, R)  
  - Both passed to the delay engine for FM processing

- **Inversion:**  
  - If enabled, swaps carrier and modulator channels before processing.

---

## **Processing Equations**



out L = DelayL(carrier L, maxDelayLength * lowpass(atan(clipper(modulator L + 1) * 0.5)))
out R = DelayR(carrier R, maxDelayLength * lowpass(atan(clipper(modulator R + 1) * 0.5)))


---

## **Example Routing Table**

| Algorithm | Carrier L      | Carrier R      | Modulator L     | Modulator R    |
|-----------|---------------|---------------|----------------|---------------|
| 0         | Input L       | Input L       | Input R        | Input R       |
| 1         | (L+R)/2       | (L+R)/2       | (SC_L+SC_R)/2  | (SC_L+SC_R)/2 |
| 2         | Input L       | Input R       | SC_L           | SC_R          |

- **Invert:** Swaps carrier and modulator channels for both L and R.

---

*This routing system enables a wide range of FM and sidechain-based effects, from classic mono FM to advanced stereo and sidechain-driven modulation. For details, see `Routing.cpp` in the source code.*

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
