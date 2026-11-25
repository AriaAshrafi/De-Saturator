#pragma once

// A small WAV reader and writer for the offline tools: PCM 16/24/32-bit and 32-bit float.

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace wav
{

struct Audio
{
    double sampleRate = 48000.0;
    std::vector<std::vector<double>> channels;

    int numChannels() const { return (int) channels.size(); }
    int numSamples() const { return channels.empty() ? 0 : (int) channels[0].size(); }
};

inline bool read (const std::string& path, Audio& out)
{
    std::ifstream f (path, std::ios::binary);
    if (! f)
        return false;

    std::vector<uint8_t> bytes ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
    if (bytes.size() < 44 || std::memcmp (bytes.data(), "RIFF", 4) != 0 || std::memcmp (bytes.data() + 8, "WAVE", 4) != 0)
        return false;

    auto u16 = [&] (size_t p) { return (uint32_t) bytes[p] | ((uint32_t) bytes[p + 1] << 8); };
    auto u32 = [&] (size_t p) { return u16 (p) | (u16 (p + 2) << 16); };

    uint32_t format = 0, numCh = 0, rate = 0, bits = 0;
    size_t dataPos = 0, dataSize = 0;

    for (size_t p = 12; p + 8 <= bytes.size();)
    {
        const size_t size = u32 (p + 4);
        if (std::memcmp (bytes.data() + p, "fmt ", 4) == 0)
        {
            format = u16 (p + 8);
            numCh = u16 (p + 10);
            rate = u32 (p + 12);
            bits = u16 (p + 22);
            if (format == 0xFFFE && size >= 26)
                format = u16 (p + 32); // WAVE_FORMAT_EXTENSIBLE: the real format is in the sub-format GUID
        }
        else if (std::memcmp (bytes.data() + p, "data", 4) == 0)
        {
            dataPos = p + 8;
            dataSize = std::min (size, bytes.size() - dataPos);
            break;
        }
        p += 8 + size + (size & 1);
    }

    if (dataPos == 0 || numCh == 0 || (format != 1 && format != 3))
        return false;

    const size_t bytesPer = bits / 8;
    const size_t frames = dataSize / (bytesPer * numCh);
    out.sampleRate = rate;
    out.channels.assign (numCh, std::vector<double> (frames));

    for (size_t i = 0; i < frames; ++i)
    {
        for (uint32_t c = 0; c < numCh; ++c)
        {
            const uint8_t* s = bytes.data() + dataPos + (i * numCh + c) * bytesPer;
            double v = 0.0;
            if (format == 3 && bits == 32)
            {
                float x;
                std::memcpy (&x, s, 4);
                v = x;
            }
            else if (bits == 16)
                v = (int16_t) (s[0] | (s[1] << 8)) / 32768.0;
            else if (bits == 24)
                v = (int32_t) ((uint32_t) s[0] << 8 | (uint32_t) s[1] << 16 | (uint32_t) s[2] << 24) / 2147483648.0;
            else if (bits == 32)
                v = (int32_t) ((uint32_t) s[0] | (uint32_t) s[1] << 8 | (uint32_t) s[2] << 16 | (uint32_t) s[3] << 24) / 2147483648.0;
            out.channels[c][i] = v;
        }
    }

    return true;
}

/** Writes 32-bit float, so nothing is clipped or dithered on the way out. */
inline bool write (const std::string& path, const Audio& a)
{
    std::ofstream f (path, std::ios::binary);
    if (! f)
        return false;

    const uint32_t numCh = (uint32_t) a.numChannels(), frames = (uint32_t) a.numSamples();
    const uint32_t dataSize = frames * numCh * 4;

    auto put32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto put16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };

    f.write ("RIFF", 4);
    put32 (36 + dataSize);
    f.write ("WAVEfmt ", 8);
    put32 (16);
    put16 (3);
    put16 ((uint16_t) numCh);
    put32 ((uint32_t) a.sampleRate);
    put32 ((uint32_t) a.sampleRate * numCh * 4);
    put16 ((uint16_t) (numCh * 4));
    put16 (32);
    f.write ("data", 4);
    put32 (dataSize);

    for (uint32_t i = 0; i < frames; ++i)
    {
        for (uint32_t c = 0; c < numCh; ++c)
        {
            const float v = (float) a.channels[c][i];
            f.write ((const char*) &v, 4);
        }
    }

    return (bool) f;
}

} // namespace wav
