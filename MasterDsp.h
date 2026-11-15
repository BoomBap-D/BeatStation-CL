#pragma once
#include "FxDsp.h"

// The sound and the measuring tools of the Mastering Station. Plain C++ (no JUCE), so each part can be tested on its own.
namespace fxdsp
{
// ------------------------------------------------------------------ TRUE PEAK
// Digital audio can peak between the samples. This rebuilds the wave at eight times the sample rate (the ITU BS.1770 standard asks
// for at least four) and reports the highest point, so the reading and the limiter both work on the real peak, not just the sample values.
class TruePeak
{
public:
    TruePeak()
    {
        auto bessel = [] (double x) { double s = 1.0, t = 1.0; for (int k = 1; k < 40; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
        const int total = taps * phases;
        for (int i = 0; i < total; ++i)
        {
            const double m = i - (total - 1) / 2.0, x = m / (double) phases;
            const double sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (3.14159265358979 * x) / (3.14159265358979 * x);
            const double r = 2.0 * i / (total - 1) - 1.0;
            h[i % phases][i / phases] = (float) (sinc * bessel (8.0 * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel (8.0));
        }
        for (auto& ph : h) { float sum = 0.f; for (float v : ph) sum += v; for (float& v : ph) v /= sum; }   // each phase passes a steady level unchanged
    }
    void reset() { buf.fill (0.f); pos = 0; }
    float tick (float x)                                                 // returns the highest of the in-between points for this sample
    {
        buf[(size_t) pos] = x;
        float peak = 0.f;
        for (int p = 0; p < phases; ++p)
        {
            float acc = 0.f;
            int idx = pos;
            for (int k = 0; k < taps; ++k) { acc += h[p][taps - 1 - k] * buf[(size_t) idx]; if (--idx < 0) idx = taps - 1; }
            peak = std::max (peak, std::abs (acc));
        }
        pos = (pos + 1) % taps;
        return peak;
    }
    static constexpr int phases = 8, taps = 24, latency = 12;                        // the reading is about 12 samples behind the input
private:
    float h[phases][taps] {};
    std::array<float, taps> buf {};
    int pos = 0;
};

// ------------------------------------------------------------------ LOUDNESS (LUFS)
// The ITU BS.1770 / EBU R128 loudness measure used by every streaming service. "K-weighting" makes it hear roughly like an ear
// (less bass, a little more treble). Momentary = the last 0.4 s, short-term = the last 3 s, integrated = the whole song so far,
// ignoring silence and very quiet passages (the standard's two gates).
class LoudnessMeter
{
public:
    float momentary = -100.f, shortTerm = -100.f, integrated = -100.f;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double pi2 = 3.14159265358979;
        {   // stage 1: high shelf, +4 dB above about 1.7 kHz
            const double K = std::tan (pi2 * 1681.974450955533 / sr), Vh = std::pow (10.0, 3.999843853973347 / 20.0), Vb = std::pow (Vh, 0.4996667741545416), Q = 0.7071752369554196;
            const double a0 = 1.0 + K / Q + K * K;
            sh[0] = (Vh + Vb * K / Q + K * K) / a0;  sh[1] = 2.0 * (K * K - Vh) / a0;  sh[2] = (Vh - Vb * K / Q + K * K) / a0;
            sh[3] = 2.0 * (K * K - 1.0) / a0;        sh[4] = (1.0 - K / Q + K * K) / a0;
        }
        {   // stage 2: high-pass at about 38 Hz
            const double K = std::tan (pi2 * 38.13547087602444 / sr), Q = 0.5003270373238773, a0 = 1.0 + K / Q + K * K;
            hp[0] = 1.0;  hp[1] = -2.0;  hp[2] = 1.0;  hp[3] = 2.0 * (K * K - 1.0) / a0;  hp[4] = (1.0 - K / Q + K * K) / a0;
        }
        hopLen = (int) (sr * 0.1);
        reset();
    }

    void reset()
    {
        for (auto& z : state) z = 0.0;
        hopEnergy = 0.0;  hopCount = 0;  ring.fill (0.0);  ringPos = 0;  filled = 0;
        hist.fill (0);  histEnergy.fill (0.0);
        momentary = shortTerm = integrated = -100.f;
    }

    void process (const float* L, const float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const double l = filter (L[i], 0), r = filter (R[i], 1);
            hopEnergy += l * l + r * r;
            if (++hopCount >= hopLen)
            {
                ring[(size_t) ringPos] = hopEnergy / (double) hopLen;    // mean square of this 100 ms (both channels added)
                ringPos = (ringPos + 1) % ringSize;
                filled = std::min (filled + 1, ringSize);
                hopEnergy = 0.0;
                hopCount = 0;
                update();
            }
        }
    }

private:
    static constexpr int ringSize = 30, bins = 800;                      // 3 s of 100 ms pieces; loudness histogram -75 .. +5 in 0.1 steps
    static float toLufs (double meanSquare) { return meanSquare > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (meanSquare)) : -100.f; }

    double filter (double x, int ch)
    {
        double* z = state + ch * 4;
        const double y1 = sh[0] * x + z[0];
        z[0] = sh[1] * x - sh[3] * y1 + z[1];
        z[1] = sh[2] * x - sh[4] * y1;
        const double y2 = hp[0] * y1 + z[2];
        z[2] = hp[1] * y1 - hp[3] * y2 + z[3];
        z[3] = hp[2] * y1 - hp[4] * y2;
        return y2;
    }

    void update()
    {
        auto meanOfLast = [this] (int count)
        {
            count = std::min (count, filled);
            if (count <= 0) return 0.0;
            double s = 0.0;
            for (int k = 1; k <= count; ++k) s += ring[(size_t) ((ringPos - k + ringSize) % ringSize)];
            return s / count;
        };
        const double m = meanOfLast (4);
        momentary = toLufs (m);
        shortTerm = toLufs (meanOfLast (30));
        if (filled >= 4)
        {
            const float l = toLufs (m);
            if (l > -70.f)                                               // first gate: ignore silence
            {
                const int b = std::clamp ((int) ((l + 75.f) * 10.f), 0, bins - 1);
                ++hist[(size_t) b];
                histEnergy[(size_t) b] += m;
            }
            double sum = 0.0;
            long long cnt = 0;
            for (int b = 0; b < bins; ++b) { sum += histEnergy[(size_t) b]; cnt += hist[(size_t) b]; }
            if (cnt > 0)
            {
                const float gate = toLufs (sum / (double) cnt) - 10.f;   // second gate: ignore anything 10 LU quieter than the average
                const int from = std::clamp ((int) std::ceil ((gate + 75.f) * 10.f), 0, bins - 1);
                double s2 = 0.0;
                long long c2 = 0;
                for (int b = from; b < bins; ++b) { s2 += histEnergy[(size_t) b]; c2 += hist[(size_t) b]; }
                integrated = c2 > 0 ? toLufs (s2 / (double) c2) : -100.f;
            }
        }
    }

    double sr = 48000.0, sh[5] {}, hp[5] {}, state[8] {}, hopEnergy = 0.0;
    int hopLen = 4800, hopCount = 0, ringPos = 0, filled = 0;
    std::array<double, ringSize> ring {};
    std::array<long long, bins> hist {};
    std::array<double, bins> histEnergy {};
};

// ------------------------------------------------------------------ three-band split (Linkwitz-Riley, 24 dB/octave)
// Splits the sound into low / mid / high so each can be treated on its own. Put back together untouched, the level is exactly flat.
class ThreeBand
{
public:
    void prepare (double sampleRate) { sr = sampleRate; for (auto& f : flt) f.reset(); set (f1, f2); }
    void set (float lowMid, float midHigh)
    {
        f1 = std::clamp (lowMid, 30.f, 1000.f);
        f2 = std::clamp (midHigh, f1 * 1.5f, 16000.f);
        for (int i = 0; i < 2; ++i) flt[i].set (f1, 0.70710678f, sr);
        for (int i = 2; i < 4; ++i) flt[i].set (f1, 0.70710678f, sr);
        for (int i = 4; i < 10; ++i) flt[i].set (f2, 0.70710678f, sr);
    }
    void split (float x, float& low, float& mid, float& high)
    {
        float lp, bp, hp, a, b;
        flt[0].tick (x, lp, bp, hp);  flt[1].tick (lp, a, bp, b);   low = a;              // low-pass twice at f1
        flt[2].tick (x, lp, bp, hp);  flt[3].tick (hp, a, bp, b);   const float rest = b; // high-pass twice at f1
        flt[4].tick (rest, lp, bp, hp);  flt[5].tick (lp, a, bp, b);  mid = a;
        flt[6].tick (rest, lp, bp, hp);  flt[7].tick (hp, a, bp, b);  high = b;
        // the low band goes through the same phase turn the upper split gives, so all three line up again
        flt[8].tick (low, lp, bp, hp);
        low = lp - 1.41421356f * bp + hp;                                                 // second-order all-pass at f2 (same phase as the 24 dB split)
    }
    float f1 = 120.f, f2 = 2500.f;
private:
    Svf flt[10];
    double sr = 44100.0;
};

// ------------------------------------------------------------------ MASTER EQ (with a little mid/side)
// Four broad bands on the whole mix, plus two moves on the SIDES only: a low cut (keeps the bass and 808 in the centre) and "side air".
class MasterEq
{
public:
    float lowF = 80.f, lowDb = 0.f, mudF = 300.f, mudDb = 0.f, presF = 3000.f, presDb = 0.f, airF = 12000.f, airDb = 0.f, sideCut = 20.f, sideAir = 0.f;
    void prepare (double sampleRate) { sr = sampleRate; for (auto& c : ch) for (auto& f : c) f.reset(); sideHp.reset(); sideShelf.reset(); update(); }
    void update()
    {
        for (auto& c : ch) { c[0].lowShelf (lowF, lowDb, sr); c[1].bell (mudF, mudDb, 0.8f, sr); c[2].bell (presF, presDb, 0.8f, sr); c[3].highShelf (airF, airDb, sr); }
        sideHp.highPass (std::max (20.f, sideCut), 0.71f, sr);
        sideShelf.highShelf (8000.f, sideAir, sr);
    }
    void process (float* L, float* R, int n)
    {
        const bool cut = sideCut > 20.5f;
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            for (auto& f : ch[0]) l = f.tick (l);
            for (auto& f : ch[1]) r = f.tick (r);
            const float m = 0.5f * (l + r);
            float s = 0.5f * (l - r);
            if (cut) s = sideHp.tick (s);
            s = sideShelf.tick (s);
            L[i] = m + s;
            R[i] = m - s;
        }
    }
    float responseDb (float hz) const { float m = 1.f; for (const auto& f : ch[0]) m *= f.magnitude (hz, sr); return gainToDb (m); }
    float sideResponseDb (float hz) const { return responseDb (hz) + gainToDb (sideShelf.magnitude (hz, sr) * (sideCut > 20.5f ? sideHp.magnitude (hz, sr) : 1.f)); }
private:
    Biquad ch[2][4], sideHp, sideShelf;
    double sr = 44100.0;
};

// ------------------------------------------------------------------ GLUE (bus compressor)
// A gentle, stereo-linked compressor for the whole mix. The side-chain low cut stops the kick and 808 from making everything pump.
class GlueComp
{
public:
    float threshDb = -12.f, ratio = 2.f, attackMs = 30.f, releaseMs = 200.f, makeupDb = 0.f, scHp = 90.f, mix = 1.f;
    float grNow = 0.f;
    void prepare (double sampleRate) { sr = sampleRate; hp[0].reset(); hp[1].reset(); env = 0.f; grDb = 0.f; }
    void process (float* L, float* R, int n)
    {
        hp[0].highPass (std::max (20.f, scHp), 0.71f, sr);
        hp[1].highPass (std::max (20.f, scHp), 0.71f, sr);
        const float att = 1.f - std::exp (-1.f / (0.001f * std::max (0.05f, attackMs) * (float) sr)), rel = 1.f - std::exp (-1.f / (0.001f * std::max (10.f, releaseMs) * (float) sr));
        const float rmsC = 1.f - std::exp (-1.f / (0.01f * (float) sr)), mk = dbToGain (makeupDb), knee = 8.f;
        float worst = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float sl = hp[0].tick (L[i]), sr2 = hp[1].tick (R[i]);
            env += rmsC * (std::max (sl * sl, sr2 * sr2) - env);            // level of the louder side, averaged over 10 ms
            const float in = 10.f * std::log10 (std::max (env, 1.0e-12f)) + 3.f, over = in - threshDb;
            float target = 0.f;
            if (2.f * std::abs (over) <= knee) { const float t = over + knee * 0.5f; target = (1.f / ratio - 1.f) * t * t / (2.f * knee); }
            else if (over > 0.f) target = over / ratio - over;
            grDb += (target < grDb ? att : rel) * (target - grDb);
            worst = std::min (worst, grDb);
            const float g = dbToGain (grDb) * mk;
            L[i] += mix * (L[i] * g - L[i]);
            R[i] += mix * (R[i] * g - R[i]);
        }
        grNow = worst;
    }
private:
    Biquad hp[2];
    double sr = 44100.0;
    float env = 0.f, grDb = 0.f;
};

// ------------------------------------------------------------------ MULTIBAND compressor (low / mid / high)
class Multiband
{
public:
    float xLow = 120.f, xHigh = 2500.f, thresh[3] = { -18.f, -18.f, -18.f }, ratio[3] = { 2.f, 2.f, 2.f }, gain[3] = { 0.f, 0.f, 0.f };
    float grNow[3] {};
    void prepare (double sampleRate) { sr = sampleRate; for (auto& s : split) s.prepare (sr); for (auto& e : env) e = 0.f; for (auto& g : gr) g = 0.f; }
    void process (float* L, float* R, int n)
    {
        for (auto& s : split) s.set (xLow, xHigh);
        static const float attMs[3] = { 30.f, 15.f, 5.f }, relMs[3] = { 220.f, 140.f, 80.f };   // slower on the lows, quicker on the highs
        float att[3], rel[3], mk[3], worst[3] = { 0.f, 0.f, 0.f };
        for (int b = 0; b < 3; ++b)
        {
            att[b] = 1.f - std::exp (-1.f / (0.001f * attMs[b] * (float) sr));
            rel[b] = 1.f - std::exp (-1.f / (0.001f * relMs[b] * (float) sr));
            mk[b] = dbToGain (gain[b]);
        }
        const float rmsC = 1.f - std::exp (-1.f / (0.01f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            float l[3], r[3];
            split[0].split (L[i], l[0], l[1], l[2]);
            split[1].split (R[i], r[0], r[1], r[2]);
            float ol = 0.f, orr = 0.f;
            for (int b = 0; b < 3; ++b)
            {
                env[b] += rmsC * (std::max (l[b] * l[b], r[b] * r[b]) - env[b]);
                const float in = 10.f * std::log10 (std::max (env[b], 1.0e-12f)) + 3.f, over = in - thresh[b], knee = 6.f;
                float target = 0.f;
                if (2.f * std::abs (over) <= knee) { const float t = over + knee * 0.5f; target = (1.f / ratio[b] - 1.f) * t * t / (2.f * knee); }
                else if (over > 0.f) target = over / ratio[b] - over;
                gr[b] += (target < gr[b] ? att[b] : rel[b]) * (target - gr[b]);
                worst[b] = std::min (worst[b], gr[b]);
                const float g = dbToGain (gr[b]) * mk[b];
                ol += l[b] * g;
                orr += r[b] * g;
            }
            L[i] = ol;
            R[i] = orr;
        }
        for (int b = 0; b < 3; ++b) grNow[b] = worst[b];
    }
private:
    ThreeBand split[2];
    double sr = 44100.0;
    float env[3] {}, gr[3] {};
};

// ------------------------------------------------------------------ COLOR (warmth and air)
// Warmth: gentle valve-style saturation on the lows and mids only. Air: new, bright harmonics made from the top of the mix and added back.
class Color
{
public:
    float warmth = 0.f, air = 0.f;                                       // 0..1
    void prepare (double sampleRate) { sr = sampleRate; for (auto& c : ch) c = {}; }
    void process (float* L, float* R, int n)
    {
        const float lpA = 1.f - std::exp (-2.f * pi * 2500.f / (float) sr), hpA = 1.f - std::exp (-2.f * pi * 6000.f / (float) sr);
        const float drive = 1.f + 5.f * warmth, norm = 1.f / drive;       // quiet sound passes at the same level; only the peaks round off
        float* io[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                auto& s = ch[c];
                const float x = io[c][i];
                s.lp += lpA * (x - s.lp);
                const float lows = s.lp, rest = x - lows;
                const float warm = lows + warmth * (std::tanh (lows * drive) * norm - lows);
                s.hp += hpA * (x - s.hp);
                const float top = x - s.hp;
                float ex = std::tanh (top * 6.f);                            // squash the top band to make harmonics...
                s.hp2 += hpA * (ex - s.hp2);
                ex -= s.hp2;                                                 // ...and keep only the bright part of them
                io[c][i] = warm + rest + air * 0.35f * ex;
            }
    }
private:
    struct Ch { float lp = 0.f, hp = 0.f, hp2 = 0.f; };
    Ch ch[2];
    double sr = 44100.0;
};

// ------------------------------------------------------------------ IMAGER (stereo width by band)
class Imager
{
public:
    float xLow = 250.f, xHigh = 4000.f, width[3] = { 1.f, 1.f, 1.f };      // 0 = mono, 1 = unchanged, 2 = extra wide
    void prepare (double sampleRate) { sr = sampleRate; mid.prepare (sr); side.prepare (sr); }
    void process (float* L, float* R, int n)
    {
        mid.set (xLow, xHigh);
        side.set (xLow, xHigh);
        for (int i = 0; i < n; ++i)
        {
            float m[3], s[3];
            mid.split (0.5f * (L[i] + R[i]), m[0], m[1], m[2]);
            side.split (0.5f * (L[i] - R[i]), s[0], s[1], s[2]);
            const float mm = m[0] + m[1] + m[2], ss = s[0] * width[0] + s[1] * width[1] + s[2] * width[2];
            L[i] = mm + ss;
            R[i] = mm - ss;
        }
    }
private:
    ThreeBand mid, side;
    double sr = 44100.0;
};

// ------------------------------------------------------------------ CLIPPER
// Shaves the tips off the loudest hits before the limiter, so the limiter has less to do and the drums stay punchy.
// It runs at twice the sample rate so the clipping does not fold back as harshness.
class Clipper
{
public:
    float ceilingDb = -1.f, soft = 0.5f;
    float clippedDb = 0.f;                                               // how much it is taking off right now (for the screen)
    void prepare (double) { for (auto& o : os) o.prepare(); }
    static constexpr int latency = Oversample2x::latencySamples;
    void process (float* L, float* R, int n)
    {
        const float c = dbToGain (ceilingDb), knee = std::clamp (soft, 0.f, 1.f) * 0.5f, start = c * (1.f - knee);
        float worst = 0.f;
        float* io[2] = { L, R };
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float in = io[ch][i];
                io[ch][i] = os[ch].process (in, [c, start, knee] (float x)
                {
                    const float a = std::abs (x);
                    if (a <= start) return x;
                    if (knee <= 0.f) return x > 0.f ? c : -c;
                    const float y = start + (c - start) * std::tanh ((a - start) / (c - start));   // bends over smoothly into the ceiling
                    return x > 0.f ? y : -y;
                });
                const float a = std::abs (in);
                if (a > c) worst = std::max (worst, gainToDb (a / c));
            }
        clippedDb = worst;
    }
private:
    Oversample2x os[2];
};

// ------------------------------------------------------------------ MASTER LIMITER (true peak, look-ahead)
// The last thing in the chain. It looks 3 ms ahead and works on the true (between-sample) peak, so the finished file stays under the ceiling
// even after it has been turned into an MP3 or AAC.
class MasterLimiter
{
public:
    float gainDb = 0.f, ceilingDb = -1.f, releaseMs = 150.f;
    float grDb = 0.f;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        look = std::max (8, (int) (sr * 0.003));
        for (auto& d : audio) d.prepare (look + TruePeak::latency + 16);
        for (auto& t : tp) t.reset();
        wedgeVal.assign ((size_t) look + 4, 1.f);
        wedgeIdx.assign ((size_t) look + 4, 0);
        head = tail = 0;  count = 0;
        box.assign ((size_t) look, 1.f);
        boxPos = 0;  boxSum = (double) look;  rel = 1.f;
        gIn.setTime (sr, 20.f);
        gIn.snap (dbToGain (gainDb));
    }
    int latency() const { return look + TruePeak::latency; }

    void process (float* L, float* R, int n)
    {
        const float inT = dbToGain (gainDb), ceil = dbToGain (ceilingDb - 0.4f);   // aims 0.4 dB under: moving the gain adds a little between the samples, and this covers it (measured)
        const float relC = 1.f - std::exp (-1.f / (0.001f * std::max (1.f, releaseMs) * (float) sr));
        const int cap = (int) wedgeVal.size();
        float minG = 1.f;
        for (int i = 0; i < n; ++i)
        {
            const float gi = gIn.next (inT), l = L[i] * gi, r = R[i] * gi;
            const float peak = std::max (tp[0].tick (l), tp[1].tick (r));
            const float need = peak > ceil ? ceil / peak : 1.f;
            rel = std::min (need, rel + relC * (1.f - rel));
            while (head != tail && wedgeVal[(size_t) ((tail + cap - 1) % cap)] >= rel) tail = (tail + cap - 1) % cap;
            wedgeVal[(size_t) tail] = rel;
            wedgeIdx[(size_t) tail] = count;
            tail = (tail + 1) % cap;
            if (count - wedgeIdx[(size_t) head] > look) head = (head + 1) % cap;
            const float lowest = wedgeVal[(size_t) head];
            ++count;
            boxSum += (double) lowest - (double) box[(size_t) boxPos];
            box[(size_t) boxPos] = lowest;
            if (++boxPos >= look) boxPos = 0;
            const float g = std::min (1.f, (float) (boxSum / (double) look));
            audio[0].write (l);
            audio[1].write (r);
            L[i] = audio[0].readInt (look + TruePeak::latency) * g;      // the audio waits for the look-ahead and for the peak reading
            R[i] = audio[1].readInt (look + TruePeak::latency) * g;
            minG = std::min (minG, g);
        }
        grDb = gainToDb (minG);
    }
private:
    DelayLine audio[2];
    TruePeak tp[2];
    std::vector<float> wedgeVal, box;
    std::vector<long long> wedgeIdx;
    long long count = 0;
    int head = 0, tail = 0, boxPos = 0, look = 132;
    double boxSum = 0.0, sr = 44100.0;
    float rel = 1.f;
    Smooth gIn;
};

// TPDF dither for 16-bit files: a tiny, even noise that replaces the gritty distortion you get when quiet detail is cut down to 16 bits.
struct Dither
{
    unsigned int seed = 0x1234567u;
    float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) * (1.f / 16777216.f); }
    float tick (float x) { const float lsb = 1.f / 32768.f; return std::round ((x + (rnd() - rnd()) * lsb) * 32768.f) / 32768.f; }
};
}
