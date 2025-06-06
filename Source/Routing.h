#pragma once

// Simple stereo container for two float samples
struct StereoSample
{
    float left;
    float right;
};

// Output routing structure returned by routeSample()
// - carrier: signal to be delayed
// - modulator: signal controlling delay time
// - sideChain: unprocessed SC input (passed through for UI/monitoring etc.)
struct RoutingOutputs
{
    StereoSample carrier;
    StereoSample modulator;
    StereoSample sideChain;
};

// Routes a single frame of audio based on algorithm and inversion
// Inputs:
// - L, R: main stereo input
// - SC_L, SC_R: sidechain stereo input
// - algorithm: 0 (mainL/carrier, mainR/modulator)
//             1 (mono main = carrier, mono SC = modulator)
//             2 (main stereo = carrier, SC stereo = modulator)
// - invert: if 1, swaps carrier and modulator
RoutingOutputs routeSample(float L, float R, float SC_L, float SC_R, int algorithm, int invert);