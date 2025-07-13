#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Pure brickwall limiter (mono, no oversampling)
class BrickWallLimiter {
public:
    BrickWallLimiter() = default;
    int getLookaheadSamples() const { return lookaheadSamples; }

    void prepare(double sampleRate, int maxBlockSize = 512) {
        this->sampleRate = sampleRate;

        // Lookahead buffer for true peak detection (1ms default)
        lookaheadSamples = static_cast<int>(0.003 * sampleRate); // 1ms lookahead
        if (lookaheadSamples < 4) lookaheadSamples = 4;

        lookaheadBuffer.resize(lookaheadSamples, 0.0f);
        lookaheadIndex = 0;

        // Envelope follower for smooth gain reduction
        float attackTimeMs = 0.1f; // 10 microseconds
        float releaseTimeMs = 2.0f;  // 1ms - very fast

        attackCoeff = std::exp(-1.0f / (attackTimeMs * 0.001f * sampleRate));
        releaseCoeff = std::exp(-1.0f / (releaseTimeMs * 0.001f * sampleRate));

        clear();
    }

    void setCeiling(float ceilingDb) {
        ceiling = std::pow(10.0f, ceilingDb / 20.0f);
        ceiling = std::min(ceiling, 0.999f); // Never allow exactly 1.0
    }

    void process(float* samples, int numSamples) {

        if (lookaheadBuffer.empty() || samples == nullptr || numSamples <= 0)
            return; // or throw std::runtime_error("Limiter not prepared!");

        for (int i = 0; i < numSamples; ++i) {
            float input = samples[i];

            // Store in lookahead buffer
            lookaheadBuffer[lookaheadIndex] = input;

            // Get delayed sample for processing
            int delayedIndex = (lookaheadIndex + 1) % lookaheadSamples;
            float delayed = lookaheadBuffer[delayedIndex];

            // True peak detection
            float peak = std::abs(input);

            // Calculate required gain reduction
            float targetGain = 1.0f;
            if (peak > ceiling) {
                targetGain = ceiling / peak;
            }

            // Smooth gain changes
            if (targetGain < gainReduction) {
                gainReduction = targetGain + (gainReduction - targetGain) * attackCoeff;
            } else {
                gainReduction = targetGain + (gainReduction - targetGain) * releaseCoeff;
            }

            // Apply limiting
            float limited = delayed * gainReduction;

            // Absolute safety net - hard clip at ceiling
            limited = std::clamp(limited, -ceiling, ceiling);

            // Store processed sample
            samples[i] = limited;

            lookaheadIndex = (lookaheadIndex + 1) % lookaheadSamples;
        }
    }

    float processSample(float input)
    {
        if (lookaheadBuffer.empty()) return input; // Defensive

        lookaheadBuffer[lookaheadIndex] = input;

        int delayedIndex = (lookaheadIndex + 1) % lookaheadSamples;
        float delayed = lookaheadBuffer[delayedIndex];

        float peak = std::abs(input);
        float targetGain = 1.0f;
        if (peak > ceiling && peak > 0.00001f)
            targetGain = ceiling / peak;

        if (targetGain < gainReduction)
            gainReduction = targetGain + (gainReduction - targetGain) * attackCoeff;
        else
            gainReduction = targetGain + (gainReduction - targetGain) * releaseCoeff;

        float limited = delayed * gainReduction;
        limited = std::clamp(limited, -ceiling, ceiling);
        if (!std::isfinite(limited)) limited = 0.0f;

        lookaheadIndex = (lookaheadIndex + 1) % lookaheadSamples;
        return limited;
    }


    // Get current gain reduction for metering
    float getGainReductionDb() const {
        return 20.0f * std::log10(std::max(0.001f, gainReduction));
    }

    void clear() {
        std::fill(lookaheadBuffer.begin(), lookaheadBuffer.end(), 0.0f);
        lookaheadIndex = 0;
        gainReduction = 1.0f;
    }

private:
    double sampleRate = 44100.0;
    int lookaheadSamples = 44;

    // Buffers
    std::vector<float> lookaheadBuffer;
    int lookaheadIndex = 0;

    // Parameters
    float ceiling = 0.95f;
    float gainReduction = 1.0f;

    // Envelope coefficients
    float attackCoeff = 0.9f;
    float releaseCoeff = 0.999f;
};
