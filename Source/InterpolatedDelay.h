#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

class InterpolatedDelay
{
public:
    enum class InterpolationType { Linear, Lagrange };

    InterpolatedDelay()
    {
        // Allocate buffer to maximum possible size ONCE
        constexpr double maxDelaySeconds = 2.0;
        constexpr double maxSampleRate = 192000.0;
        constexpr int maxOversampling = 2;
        const size_t maxBufferSize = static_cast<size_t>(maxDelaySeconds * maxSampleRate * maxOversampling) + 4; // +4 for Lagrange

        buffer.resize(maxBufferSize, 0.0f);
        delayRegionSize = static_cast<int>(maxBufferSize);
        writePos = 0;
    }

    void setInterpolationType(InterpolationType type) { interpolationType = type; }
    InterpolationType getInterpolationType() const { return interpolationType; }

    void setMaxDelayMs(float newMaxDelayMs) 
    { 
        // Clamp to what the buffer can actually support
        constexpr float maxDelayMsPossible = 2.0f * 192000.0f * 2.0f * 0.001f; // 2s at 192kHz, 2x
        maxDelayMs = std::clamp(newMaxDelayMs, 1.0f, maxDelayMsPossible);
    }
    void setBaseDelayMs(float newBaseDelayMs) { baseDelayMs = newBaseDelayMs; }
    void setMinDelayMs(float newMinDelayMs) { minDelayMs = newMinDelayMs; }

    float getMaxDelayMs() const { return maxDelayMs; }
    float getBaseDelayMs() const { return baseDelayMs; }
    float getMinDelayMs() const { return minDelayMs; }

    void prepare(double newSampleRate, float newMaxDelayMs)
    {
        sampleRate = newSampleRate;
        setMaxDelayMs(newMaxDelayMs);
        writePos = 0;
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    // Utility method to check if delay is properly initialized
    bool isInitialized() const
    {
        return !buffer.empty() && sampleRate > 0.0;
    }

    // Get current buffer usage for debugging
    float getBufferUsagePercent() const
    {
        if (delayRegionSize == 0) return 0.0f;
        return (static_cast<float>(writePos) / static_cast<float>(delayRegionSize)) * 100.0f;
    }

    float process(float input, float modSignal)
    {
        if (buffer.empty() || sampleRate <= 0.0f || buffer.size() < 4)
            return std::isfinite(input) ? input : 0.0f;

        // Defensive: Clamp writePos
        if (writePos < 0 || writePos >= (int)buffer.size())
            writePos = 0;

        // Write input to buffer
        buffer[writePos] = std::isfinite(input) ? input : 0.0f;

        // Clamp modSignal and map to delay time
        float safeMod = std::clamp(modSignal, 0.0f, 1.0f);
        float delayMs = std::clamp(safeMod * maxDelayMs, minDelayMs, maxDelayMs);

        // Convert delay time to samples, clamp to buffer range
        float maxDelaySamples = static_cast<float>(buffer.size() - 4); // -4 for Lagrange
        float delaySamples = std::clamp(
            static_cast<float>(delayMs * sampleRate * 0.001f),
            0.0f,
            maxDelaySamples
        );

        // Calculate fractional read position with wraparound
        float t_pos = float(writePos) - delaySamples;
        if (t_pos < 0.0f)
            t_pos += buffer.size();

        int idx = static_cast<int>(t_pos) % buffer.size();
        if (idx < 0) idx += buffer.size();
        float frac = t_pos - float(static_cast<int>(t_pos));

        float out = 0.0f;

        if (interpolationType == InterpolationType::Lagrange)
        {
            int idx_m1 = (idx - 1 + buffer.size()) % buffer.size();
            int idx_0  = idx;
            int idx_1  = (idx + 1) % buffer.size();
            int idx_2  = (idx + 2) % buffer.size();

            float y0 = buffer[idx_m1];
            float y1 = buffer[idx_0];
            float y2 = buffer[idx_1];
            float y3 = buffer[idx_2];

            out = lagrangeInterp(y0, y1, y2, y3, frac);
        }
        else // Linear interpolation
        {
            int next_idx = (idx + 1) % buffer.size();
            float y1 = buffer[idx];
            float y2 = buffer[next_idx];
            out = y1 * (1.0f - frac) + y2 * frac;
        }

        // Advance write pointer with wraparound
        writePos = (writePos + 1) % buffer.size();

        return std::isfinite(out) ? out : 0.0f;
    }

    private:
        std::vector<float> buffer;
        int writePos = 0;
        int delayRegionSize = 0;
        double sampleRate = 44100.0;
        float maxDelayMs = 900.0f;  // This can be changed at runtime, buffer is always big enough
        float baseDelayMs = 0.0f;
        float minDelayMs = 0.0f;
        InterpolationType interpolationType = InterpolationType::Linear;

        // 3rd-order Lagrange interpolation (robust version)
        static float lagrangeInterp(float y0, float y1, float y2, float y3, float frac)
        {
            float c0 = y1;
            float c1 = 0.5f * (y2 - y0);
            float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * frac + c2) * frac + c1) * frac + c0;
        }
};
