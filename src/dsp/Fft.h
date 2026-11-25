#pragma once

#include <cmath>
#include <complex>
#include <vector>

namespace desat
{

/** A plain radix-2 complex FFT. Sizes are powers of two. */
class Fft
{
public:
    explicit Fft (int size) : n (size)
    {
        int bits = 0;
        while ((1 << bits) < n)
            ++bits;

        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b))
                    r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }

        const double pi = 3.14159265358979323846;
        cosT.resize ((size_t) n / 2);
        sinT.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k)
        {
            cosT[(size_t) k] = std::cos (2.0 * pi * k / n);
            sinT[(size_t) k] = std::sin (2.0 * pi * k / n);
        }
    }

    int size() const noexcept { return n; }

    void forward (std::complex<double>* data) const noexcept { transform (data, -1.0); }

    /** Inverse transform, scaled by 1/N. */
    void inverse (std::complex<double>* data) const noexcept
    {
        transform (data, 1.0);
        const double s = 1.0 / n;
        auto* d = reinterpret_cast<double*> (data);
        for (int i = 0; i < 2 * n; ++i)
            d[i] *= s;
    }

private:
    void transform (std::complex<double>* data, double sign) const noexcept
    {
        // std::complex<double> is layout-compatible with double[2] (re, im).
        auto* d = reinterpret_cast<double*> (data);

        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (i < j)
            {
                std::swap (d[2 * i], d[2 * j]);
                std::swap (d[2 * i + 1], d[2 * j + 1]);
            }
        }

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2;
            const int step = n / len;
            for (int i = 0; i < n; i += len)
            {
                for (int k = 0; k < half; ++k)
                {
                    const double wr = cosT[(size_t) (k * step)];
                    const double wi = sign * sinT[(size_t) (k * step)];
                    double* a = d + 2 * (i + k);
                    double* b = d + 2 * (i + k + half);
                    const double vr = b[0] * wr - b[1] * wi;
                    const double vi = b[0] * wi + b[1] * wr;
                    b[0] = a[0] - vr;
                    b[1] = a[1] - vi;
                    a[0] += vr;
                    a[1] += vi;
                }
            }
        }
    }

    int n;
    std::vector<int> rev;
    std::vector<double> cosT, sinT;
};

} // namespace desat
