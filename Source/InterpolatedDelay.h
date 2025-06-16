#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

class InterpolatedDelay
{
public:
    enum class InterpolationType { Linear, Lagrange };
    
    void setPDCEnabled(bool enabled) noexcept { pdcEnabled = enabled; }

    InterpolatedDelay()
    {
        // Allocate buffer to maximum possible size ONCE
        constexpr double maxDelaySeconds = 2.0;
        constexpr double maxSampleRate = 192000.0;
        constexpr int maxOversampling = 2;
        const size_t maxBufferSize = static_cast<size_t>(maxDelaySeconds * maxSampleRate * maxOversampling) + 4; // +4 for Lagrange

        buffer.resize(maxBufferSize, 0.0f);
        writePos = 0;
    }

    void setInterpolationType(InterpolationType type) noexcept { interpolationType = type; }
    InterpolationType getInterpolationType() const noexcept { return interpolationType; }

    void setMaxDelayMs(float newMaxDelayMs) noexcept
    {
        constexpr float maxDelayMsPossible = 2.0f * 192000.0f * 2.0f * 0.001f; // 2s at 192kHz, 2x
        maxDelayMs = std::clamp(newMaxDelayMs, 1.0f, maxDelayMsPossible);
        // DBG("Delay class setMaxDelayMs: " << maxDelayMs);
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
        writePos = 0;
    }

    void reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    bool isInitialized() const noexcept
    {
        return !buffer.empty() && sampleRate > 0.0;
    }

    float getBufferUsagePercent() const noexcept
    {
        if (buffer.empty()) return 0.0f;
        return (static_cast<float>(writePos) / static_cast<float>(buffer.size())) * 100.0f;
    }

    float process(float input, float modSignal) noexcept
    {
        if (buffer.empty() || sampleRate <= 0.0f || buffer.size() < 4)
            return std::isfinite(input) ? input : 0.0f;

        // Defensive: Clamp writePos
        if (writePos < 0 || writePos >= static_cast<int>(buffer.size()))
            writePos = 0;

        // Write input to buffer
        buffer[writePos] = std::isfinite(input) ? input : 0.0f;

        // check for PDC mode (bipolar signal mapping vs simple scaling)
        float delayMs = 0.0f;
        if (pdcEnabled)
        {
            float mod = std::clamp(modSignal, -1.0f, 1.0f);
            float base = baseDelayMs;
            if (mod < 0.0f)
                delayMs = base + mod * base;
            else
                delayMs = base + mod * (maxDelayMs - base);
        }
        else
        {
            float safeMod = std::clamp(modSignal, 0.0f, 1.0f);
            delayMs = safeMod * maxDelayMs;
        }
        delayMs = std::clamp(delayMs, minDelayMs, maxDelayMs);
        
        // for debugging the stepped dial
        static float lastDelayMs = -1.0f;
        if (delayMs != lastDelayMs)
        {
            // DBG("Delay class process: delayMs used = " << delayMs);
            lastDelayMs = delayMs;
        }
        
        // Convert delay time to samples, clamp to buffer range
        float maxDelaySamples = static_cast<float>(buffer.size() - 4); // -4 for Lagrange
        float delaySamples = std::clamp(
            delayMs * static_cast<float>(sampleRate) * 0.001f,
            0.0f,
            maxDelaySamples
        );

        // Calculate fractional read position with wraparound
        float t_pos = static_cast<float>(writePos) - delaySamples;
        if (t_pos < 0.0f)
            t_pos += static_cast<float>(buffer.size());

        int idx = static_cast<int>(t_pos) % static_cast<int>(buffer.size());
        if (idx < 0) idx += static_cast<int>(buffer.size());
        float frac = t_pos - static_cast<float>(static_cast<int>(t_pos));

        float out = 0.0f;

        if (interpolationType == InterpolationType::Lagrange)
        {
            int idx_m1 = (idx - 1 + static_cast<int>(buffer.size())) % static_cast<int>(buffer.size());
            int idx_0  = idx;
            int idx_1  = (idx + 1) % static_cast<int>(buffer.size());
            int idx_2  = (idx + 2) % static_cast<int>(buffer.size());

            float y0 = buffer[idx_m1];
            float y1 = buffer[idx_0];
            float y2 = buffer[idx_1];
            float y3 = buffer[idx_2];

            out = lagrangeInterp(y0, y1, y2, y3, frac);
        }
        else // Linear interpolation
        {
            int next_idx = (idx + 1) % static_cast<int>(buffer.size());
            float y1 = buffer[idx];
            float y2 = buffer[next_idx];
            out = y1 * (1.0f - frac) + y2 * frac;
        }

        // Advance write pointer with wraparound
        writePos = (writePos + 1) % static_cast<int>(buffer.size());

        return std::isfinite(out) ? out : 0.0f;
    }

private:
    std::vector<float> buffer;
    int writePos = 0;
    double sampleRate = 44100.0;
    float maxDelayMs = 100.0f;
    float baseDelayMs = 0.0f;
    float minDelayMs = 0.0f;
    InterpolationType interpolationType = InterpolationType::Linear;
    bool pdcEnabled = false;

    // 3rd-order Lagrange interpolation (robust version)
    static inline float lagrangeInterp(float y0, float y1, float y2, float y3, float frac) noexcept
    {
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
};
