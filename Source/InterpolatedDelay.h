#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

class InterpolatedDelay
{
public:
    InterpolatedDelay()
    {
        constexpr double maxDelaySeconds = 2.0;
        constexpr double maxSampleRate = 192000.0;
        constexpr int maxOversampling = 4;
        const size_t maxBufferSize = static_cast<size_t>(maxDelaySeconds * maxSampleRate * maxOversampling) + 4;

        buffer.resize(maxBufferSize, 0.0f);
        writePos = 0;
    }

    void setMaxDelayMs(float newMaxDelayMs) noexcept
    {
        constexpr float maxDelayMsPossible = 2000.0f; // Updated to 2000ms
        maxDelayMs = std::clamp(newMaxDelayMs, 1.0f, maxDelayMsPossible);
    }

    void setBaseDelayMs(float newBaseDelayMs) noexcept { baseDelayMs = newBaseDelayMs; }
    void setMinDelayMs(float newMinDelayMs) noexcept { minDelayMs = newMinDelayMs; }

    float getMaxDelayMs() const noexcept { return maxDelayMs; }
    float getBaseDelayMs() const noexcept { return baseDelayMs; }
    float getMinDelayMs() const noexcept { return minDelayMs; }

    void prepare(double newSampleRate, float newMaxDelayMs) noexcept
    {
        sampleRate = newSampleRate;
        setMaxDelayMs(newMaxDelayMs);
        minDelayMs = std::min(static_cast<float>(1.0 / sampleRate * 1000.0), maxDelayMs - 0.1f);
        writePos = 0;
    }

    void reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    float process(float input, float modSignal) noexcept
    {
        if (!std::isfinite(input)) input = 0.0f;
        if (!std::isfinite(modSignal)) modSignal = 0.0f;
        
        if (buffer.empty() || sampleRate <= 0.0f || buffer.size() < 4)
            return input;

        // Handle write position
        if (writePos < 0 || writePos >= static_cast<int>(buffer.size()))
            writePos = 0;

        // Write to buffer
        buffer[writePos] = input;

        // Calculate delay time
        // float effectiveBaseDelayMs = (baseDelayMs > 0.0f) ? baseDelayMs : 0.5f * maxDelayMs;
        float effectiveBaseDelayMs = 0.0f;

        float delayMs = effectiveBaseDelayMs + std::clamp(modSignal, 0.0f, 1.0f) * 
                        (maxDelayMs - effectiveBaseDelayMs);
        delayMs = std::clamp(delayMs, minDelayMs, maxDelayMs); 

        // Convert to samples
        float delaySamples = std::clamp(
            delayMs * static_cast<float>(sampleRate) * 0.001f,
            0.0f,
            static_cast<float>(buffer.size() - 4)
        );

        // Calculate read position
        float t_pos = writePos - delaySamples;
        if (t_pos < 0.0f) 
            t_pos += buffer.size();

        int idx = static_cast<int>(t_pos) % buffer.size();
        if (idx < 0) idx += buffer.size();
        float frac = t_pos - idx;
        frac = std::clamp(frac, 0.0f, 1.0f);

        // Lagrange interpolation
        int idx_m1 = (idx - 1 + buffer.size()) % buffer.size();
        int idx_0  = idx;
        int idx_1  = (idx + 1) % buffer.size();
        int idx_2  = (idx + 2) % buffer.size();

        float out = lagrangeInterp(
            buffer[idx_m1],
            buffer[idx_0],
            buffer[idx_1],
            buffer[idx_2],
            frac
        );

        // Advance write pointer
        writePos = (writePos + 1) % buffer.size();
        return out;
    }

private:
    std::vector<float> buffer;
    int writePos = 0;
    double sampleRate = 44100.0;
    float maxDelayMs = 100.0f;
    float baseDelayMs = 0.0f;
    float minDelayMs = 0.0f;

    static inline float lagrangeInterp(float y0, float y1, float y2, float y3, float frac) noexcept
    {
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
};
