#pragma once

#include <cmath>
#include <vector>

#if defined(_MSC_VER)
 #define DESAT_RESTRICT __restrict
#else
 #define DESAT_RESTRICT __restrict__
#endif

namespace desat
{

/** A real FFT for power-of-two sizes.

    A real signal of N samples is transformed with a complex FFT of N/2 points (the even samples as
    the real part, the odd ones as the imaginary part) and untangled afterwards, which halves the
    work. Real and imaginary parts are kept in separate arrays and each stage has its own run of
    twiddles, so the inner loops are straight lines the compiler can vectorise.

    An instance owns scratch memory: use one per thread.
*/
class RealFft
{
public:
    explicit RealFft (int size) : N (size), M (size / 2)
    {
        const double pi = 3.14159265358979323846;

        int bits = 0;
        while ((1 << bits) < M)
            ++bits;

        rev.resize ((size_t) M);
        for (int i = 0; i < M; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b))
                    r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }

        // Twiddles for each stage, one after another: stage with `half` butterflies starts at half - 1.
        stageCos.resize ((size_t) std::max (1, M - 1));
        stageSin.resize ((size_t) std::max (1, M - 1));
        for (int half = 1; half < M; half <<= 1)
        {
            for (int k = 0; k < half; ++k)
            {
                stageCos[(size_t) (half - 1 + k)] = std::cos (pi * k / half);
                stageSin[(size_t) (half - 1 + k)] = std::sin (pi * k / half);
            }
        }

        postCos.resize ((size_t) M + 1);
        postSin.resize ((size_t) M + 1);
        for (int k = 0; k <= M; ++k)
        {
            postCos[(size_t) k] = std::cos (2.0 * pi * k / N);
            postSin[(size_t) k] = std::sin (2.0 * pi * k / N);
        }

        zr.resize ((size_t) M);
        zi.resize ((size_t) M);
    }

    int size() const noexcept { return N; }
    int numBins() const noexcept { return M + 1; }

    /** time: N samples. re, im: N/2 + 1 bins. */
    void forward (const double* time, double* re, double* im) noexcept
    {
        for (int n = 0; n < M; ++n)
        {
            const size_t r = (size_t) rev[(size_t) n];
            zr[r] = time[2 * n];
            zi[r] = time[2 * n + 1];
        }

        transform (-1.0);

        for (int k = 0; k <= M; ++k)
        {
            const size_t a = (size_t) (k == M ? 0 : k), b = (size_t) (k == 0 ? 0 : M - k);
            const double er = 0.5 * (zr[a] + zr[b]), ei = 0.5 * (zi[a] - zi[b]);
            const double dr = 0.5 * (zr[a] - zr[b]), di = 0.5 * (zi[a] + zi[b]);
            const double orr = di, oi = -dr; // the odd-sample spectrum, D / i
            const double c = postCos[(size_t) k], s = postSin[(size_t) k];
            re[k] = er + orr * c + oi * s;
            im[k] = ei + oi * c - orr * s;
        }
    }

    /** The inverse, scaled so that inverse(forward(x)) == x. */
    void inverse (const double* re, const double* im, double* time) noexcept
    {
        for (int k = 0; k < M; ++k)
        {
            const int m = M - k;
            const double er = 0.5 * (re[k] + re[m]), ei = 0.5 * (im[k] - im[m]);
            const double dr = 0.5 * (re[k] - re[m]), di = 0.5 * (im[k] + im[m]);
            const double c = postCos[(size_t) k], s = postSin[(size_t) k];
            const double orr = dr * c - di * s, oi = dr * s + di * c;
            const size_t r = (size_t) rev[(size_t) k];
            zr[r] = er - oi;
            zi[r] = ei + orr;
        }

        transform (1.0);

        const double scale = 1.0 / M;
        for (int n = 0; n < M; ++n)
        {
            time[2 * n] = zr[(size_t) n] * scale;
            time[2 * n + 1] = zi[(size_t) n] * scale;
        }
    }

private:
    /** In place on zr/zi, which are already in bit-reversed order. */
    void transform (double sign) noexcept
    {
        double* DESAT_RESTRICT r = zr.data();
        double* DESAT_RESTRICT i = zi.data();

        if (M >= 2)
        {
            for (int n = 0; n < M; n += 2)
            {
                const double ar = r[n], ai = i[n], br = r[n + 1], bi = i[n + 1];
                r[n] = ar + br;
                i[n] = ai + bi;
                r[n + 1] = ar - br;
                i[n + 1] = ai - bi;
            }
        }

        for (int half = 2; half < M; half <<= 1)
        {
            const double* DESAT_RESTRICT wc = stageCos.data() + (half - 1);
            const double* DESAT_RESTRICT ws = stageSin.data() + (half - 1);
            const int len = half * 2;

            for (int n = 0; n < M; n += len)
            {
                double* DESAT_RESTRICT ar = r + n;
                double* DESAT_RESTRICT ai = i + n;
                double* DESAT_RESTRICT br = r + n + half;
                double* DESAT_RESTRICT bi = i + n + half;

                for (int k = 0; k < half; ++k)
                {
                    const double c = wc[k], s = sign * ws[k];
                    const double vr = br[k] * c - bi[k] * s;
                    const double vi = br[k] * s + bi[k] * c;
                    br[k] = ar[k] - vr;
                    bi[k] = ai[k] - vi;
                    ar[k] += vr;
                    ai[k] += vi;
                }
            }
        }
    }

    int N, M;
    std::vector<int> rev;
    std::vector<double> stageCos, stageSin, postCos, postSin;
    std::vector<double> zr, zi;
};

} // namespace desat
