#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// The sound of the effects suite. Plain C++ (no JUCE, no GUI), so every effect here can be tested on its own.
// Nothing allocates while processing: all buffers are made in prepare().
namespace fxdsp
{
constexpr float pi = 3.14159265358979f;
inline float dbToGain (float d) { return std::pow (10.f, d * 0.05f); }
inline float gainToDb (float g) { return 20.f * std::log10 (std::max (g, 1.0e-9f)); }

// A value that glides to its target, so turning a knob never clicks.
struct Smooth
{
    float v = 0.f, a = 0.01f;
    void setTime (double sr, float ms) { a = 1.f - std::exp (-1.f / (0.001f * ms * (float) sr)); }
    void snap (float t) { v = t; }
    float next (float t) { v += a * (t - v); return v; }
};

// State-variable filter (TPT form): stays stable and in tune when the cutoff moves quickly.
struct Svf
{
    float ic1 = 0.f, ic2 = 0.f, k = 1.414f, a1 = 0.f, a2 = 0.f, a3 = 0.f;
    void reset() { ic1 = ic2 = 0.f; }
    void set (float fc, float q, double sr)
    {
        const float g = std::tan (pi * std::clamp (fc, 10.f, (float) sr * 0.45f) / (float) sr);
        k = 1.f / std::max (0.05f, q);
        a1 = 1.f / (1.f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void tick (float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        lp = v2;  bp = v1;  hp = x - k * v1 - v2;
    }
};

struct DelayLine
{
    std::vector<float> buf;
    int w = 0;
    void prepare (int capacity) { buf.assign ((size_t) capacity + 8, 0.f); w = 0; }
    void write (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    float readInt (int samples) const { int i = w - 1 - samples; const int n = (int) buf.size(); while (i < 0) i += n; return buf[(size_t) i]; }
    float read (float samples) const                                    // 4-point Hermite: keeps the treble when the delay time is moving
    {
        const int n = (int) buf.size();
        samples = std::clamp (samples, 1.f, (float) (n - 6));
        const int whole = (int) samples;
        const float t = samples - (float) whole;
        auto at = [&] (int back) { int i = w - 1 - back; while (i < 0) i += n; return buf[(size_t) i]; };
        const float y0 = at (whole - 1), y1 = at (whole), y2 = at (whole + 1), y3 = at (whole + 2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
};

// Runs something at twice the sample rate. A 47-tap half-band filter on the way up and on the way down:
// flat to about 18.7 kHz, 80 dB down above. The round trip is exactly 23 samples late (latency()).
class Oversample2x
{
public:
    static constexpr int taps = 47, latencySamples = 23;
    void prepare()
    {
        auto bessel = [] (double x) { double sum = 1.0, term = 1.0; for (int k = 1; k < 40; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; } return sum; };
        const double beta = 7.86;
        for (int i = 0; i < taps; ++i)
        {
            const double m = i - (taps - 1) / 2.0;
            const double sinc = m == 0.0 ? 0.5 : std::sin (0.5 * 3.14159265358979 * m) / (3.14159265358979 * m);
            const double r = 2.0 * i / (taps - 1) - 1.0;
            h[(size_t) i] = (float) (sinc * bessel (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel (beta));
        }
        up.fill (0.f);  down.fill (0.f);
        upPos = downPos = 0;
    }
    // call with one input sample; fn is run on each of the two oversampled samples
    template <typename Fn> float process (float x, Fn&& fn)
    {
        float out = 0.f;
        for (int phase = 0; phase < 2; ++phase)
        {
            up[(size_t) upPos] = phase == 0 ? 2.f * x : 0.f;             // zero-stuffing halves the level, so the sample goes in doubled
            const float y = fn (fir (up, upPos));
            upPos = (upPos + 1) % taps;
            down[(size_t) downPos] = y;
            if (phase == 1) out = fir (down, downPos);
            downPos = (downPos + 1) % taps;
        }
        return out;
    }
private:
    float fir (const std::array<float, taps>& buf, int newest) const
    {
        float acc = 0.f;
        int idx = newest;
        for (int k = 0; k < taps; ++k) { acc += h[(size_t) k] * buf[(size_t) idx]; if (--idx < 0) idx = taps - 1; }
        return acc;
    }
    std::array<float, taps> h {}, up {}, down {};
    int upPos = 0, downPos = 0;
};

// ------------------------------------------------------------------ DRIVE
// Saturation with three characters. It runs at twice the sample rate and uses antiderivative anti-aliasing on top,
// so heavy drive does not add the harsh, fizzy aliasing that a plain waveshaper does. Type: 0 Tape (smooth, symmetrical), 1 Tube (lopsided: adds even harmonics), 2 Clip (hard).
class Drive
{
public:
    float driveDb = 6.f, tone = 20000.f, mix = 1.f, outDb = 0.f;
    int type = 0;
    float inPeak = 0.f;                                                  // for the display

    static float shape (int type, float x)
    {
        if (type == 0) return std::tanh (x);
        if (type == 1) return std::tanh (x + bias) - tanhBias;
        return std::clamp (x, -1.f, 1.f);
    }
    static float lnCosh (float x) { const float a = std::abs (x); return a + std::log1p (std::exp (-2.f * a)) - 0.693147181f; }
    static float anti (int type, float x)                                // the integral of shape()
    {
        if (type == 0) return lnCosh (x);
        if (type == 1) return lnCosh (x + bias) - x * tanhBias;
        return std::abs (x) <= 1.f ? 0.5f * x * x : std::abs (x) - 0.5f;
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        gPre.setTime (sr, 20.f);  gOut.setTime (sr, 20.f);  gMix.setTime (sr, 20.f);
        gPre.snap (dbToGain (driveDb));  gOut.snap (dbToGain (outDb));  gMix.snap (mix);
        for (auto& c : ch) c = {};
        for (auto& o : os) o.prepare();
        for (auto& d : dryDelay) d.prepare (latency + 8);
    }

    static constexpr int latency = 23;                                   // measured: the processed sound comes out 23.25 samples late

    void process (float* L, float* R, int n)
    {
        const float preT = dbToGain (driveDb), outT = dbToGain (outDb);
        const float lpA = tone >= 19900.f ? 1.f : 1.f - std::exp (-2.f * pi * std::min (tone, (float) sr * 0.45f) / (float) sr);   // fully open = no filter at all
        const int ty = type;
        float* io[2] = { L, R };
        float peak = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float pre = gPre.next (preT), out = gOut.next (outT), m = gMix.next (mix);
            const float comp = 1.f / std::sqrt (std::max (1.f, pre));     // keeps the loudness roughly steady as you add drive
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[c];
                const float dry = io[c][i];
                peak = std::max (peak, std::abs (dry));
                float y = os[c].process (dry * pre, [&s, ty] (float x)
                {
                    const float d = x - s.x1;
                    const float v = std::abs (d) > 1.0e-4f ? (anti (ty, x) - s.F1) / d : shape (ty, 0.5f * (x + s.x1));
                    s.F1 = anti (ty, x);
                    s.x1 = x;
                    const float flat = (1.f + 2.f * tilt) * s.p1 - tilt * (v + s.p2);   // puts back the little bit of treble the anti-aliasing takes
                    s.p2 = s.p1;
                    s.p1 = v;
                    return flat;
                });
                s.dc += 0.0004f * (y - s.dc);                              // the Tube shape is lopsided: take out the offset it adds
                y -= s.dc;
                s.lp += lpA * (y - s.lp);
                dryDelay[c].write (dry);                                   // the dry sound waits the same time, so Mix blends cleanly
                const float dryLate = dryDelay[c].readInt (latency);
                io[c][i] = (dryLate + m * (s.lp * comp - dryLate)) * out;
            }
        }
        inPeak = peak;
    }

private:
    static constexpr float bias = 0.35f, tanhBias = 0.336375544f;        // tanh (0.35)
    static constexpr float tilt = 0.1755f;
    struct Ch { float x1 = 0.f, F1 = 0.f, dc = 0.f, lp = 0.f, p1 = 0.f, p2 = 0.f; };
    Ch ch[2];
    Oversample2x os[2];
    DelayLine dryDelay[2];
    Smooth gPre, gOut, gMix;
    double sr = 44100.0;
};

// ------------------------------------------------------------------ FILTER
// Type: 0 low-pass, 1 high-pass, 2 band-pass, 3 notch. Slope: 0 = 12 dB/octave, 1 = 24 dB/octave. A slow LFO can sweep the cutoff.
class Filter
{
public:
    float cutoff = 20000.f, q = 0.71f, lfoRate = 0.5f, lfoDepth = 0.f;   // depth in octaves
    int type = 0, slope = 0;
    float liveCutoff = 20000.f;                                          // for the display

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        cut.setTime (sr, 15.f);
        cut.snap (cutoff);
        for (auto& f : f1) f.reset();
        for (auto& f : f2) f.reset();
        phase = 0.f;
        counter = 0;
    }

    void process (float* L, float* R, int n)
    {
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float c = cut.next (cutoff);
            if ((counter++ & 15) == 0)                                   // 16 samples at a time is far faster than the ear can follow
            {
                phase += 16.f * lfoRate / (float) sr;
                if (phase >= 1.f) phase -= 1.f;
                liveCutoff = std::clamp (c * std::pow (2.f, lfoDepth * std::sin (2.f * pi * phase)), 20.f, 20000.f);
                const float qq = slope == 1 ? std::sqrt (std::max (0.5f, q)) * 0.84f : q;   // two stages share the resonance
                for (int k = 0; k < 2; ++k) { f1[k].set (liveCutoff, qq, sr); f2[k].set (liveCutoff, qq, sr); }
            }
            for (int k = 0; k < 2; ++k)
            {
                float x = io[k][i], lp, bp, hp;
                f1[k].tick (x, lp, bp, hp);
                x = pick (x, lp, bp, hp, f1[k].k);
                if (slope == 1) { f2[k].tick (x, lp, bp, hp); x = pick (x, lp, bp, hp, f2[k].k); }
                io[k][i] = x;
            }
        }
    }

private:
    float pick (float x, float lp, float bp, float hp, float k) const
    {
        return type == 0 ? lp : type == 1 ? hp : type == 2 ? bp * k : x - bp * k;
    }
    Svf f1[2], f2[2];
    Smooth cut;
    double sr = 44100.0;
    float phase = 0.f;
    int counter = 0;
};

// ------------------------------------------------------------------ CRUSH (old sampler sound)
// Lowers the sample rate (the gritty, ringing top end of 12-bit samplers) and the bit depth, then a low-pass like the output
// filter those machines had. rate >= the real sample rate and bits >= 16 leave the sound untouched.
class Crush
{
public:
    float rate = 26040.f, bits = 12.f, cutoff = 12000.f, mix = 1.f;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& f : lp) f.reset();
        held[0] = held[1] = 0.f;
        phase = 1.f;
        gMix.setTime (sr, 20.f);
        gMix.snap (mix);
    }

    void process (float* L, float* R, int n)
    {
        const float inc = std::min (1.f, rate / (float) sr);
        const float levels = std::pow (2.f, std::clamp (bits, 2.f, 16.f) - 1.f);
        const bool quant = bits < 15.99f;
        for (auto& f : lp) f.set (cutoff, 0.71f, sr);
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            phase += inc;
            const bool take = phase >= 1.f;
            if (take) phase -= 1.f;
            const float m = gMix.next (mix);
            for (int c = 0; c < 2; ++c)
            {
                const float dry = io[c][i];
                if (take) held[c] = quant ? std::round (dry * levels) / levels : dry;
                float l, b, h;
                lp[c].tick (held[c], l, b, h);
                io[c][i] = dry + m * (l - dry);
            }
        }
    }

private:
    Svf lp[2];
    Smooth gMix;
    float held[2] {}, phase = 1.f;
    double sr = 44100.0;
};

// ------------------------------------------------------------------ TRANSIENT (punch and tail)
// attack > 0 makes the hit of a drum harder, < 0 softens it. sustain > 0 brings up the tail and room, < 0 tightens it.
class Transient
{
public:
    float attack = 0.f, sustain = 0.f, outDb = 0.f;                      // attack and sustain are -1..1
    float gainDbNow = 0.f, levelNow = 0.f;                               // for the display

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        auto c = [this] (float ms) { return std::exp (-1.f / (0.001f * ms * (float) sr)); };
        aFast = c (1.f);  aSlow = c (25.f);  rA = c (150.f);  rFast = c (80.f);  rSlow = c (600.f);
        eFa = eSa = eFr = eSr = floorV;
        g.setTime (sr, 0.5f);                                            // quick enough to get out of the way of the next hit
        g.snap (1.f);
        gOut.setTime (sr, 20.f);
        gOut.snap (dbToGain (outDb));
    }

    void process (float* L, float* R, int n)
    {
        const float outT = dbToGain (outDb);
        float peak = 0.f, gdb = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float x = std::max (floorV, std::max (std::abs (L[i]), std::abs (R[i])));
            peak = std::max (peak, x);
            eFa = x > eFa ? aFast * eFa + (1.f - aFast) * x : rA * eFa + (1.f - rA) * x;      // quick to rise
            eSa = eFa > eSa ? aSlow * eSa + (1.f - aSlow) * eFa : eFa;                        // lags behind on the way up only: the gap between them is the hit
            eFr = x > eFr ? aFast * eFr + (1.f - aFast) * x : rFast * eFr + (1.f - rFast) * x;
            eSr = x > eSr ? aFast * eSr + (1.f - aFast) * x : rSlow * eSr + (1.f - rSlow) * x; // slow to fall: the gap is the tail
            const float hit = std::clamp (gainToDb (eFa) - gainToDb (eSa), 0.f, 12.f);
            const float tail = std::clamp (gainToDb (eSr) - gainToDb (eFr), 0.f, 12.f);
            const float tailNow = tail * std::clamp (1.f - hit / 6.f, 0.f, 1.f);   // the tail control lets go while a new hit lands
            gdb = std::clamp (attack * hit + sustain * tailNow, -18.f, 18.f);
            const float gain = g.next (dbToGain (gdb)) * gOut.next (outT);
            L[i] *= gain;
            R[i] *= gain;
        }
        gainDbNow = gdb;
        levelNow = peak;
    }

private:
    static constexpr float floorV = 1.0e-4f;
    float aFast = 0.f, aSlow = 0.f, rA = 0.f, rFast = 0.f, rSlow = 0.f, eFa = floorV, eSa = floorV, eFr = floorV, eSr = floorV;
    Smooth g, gOut;
    double sr = 44100.0;
};

// ------------------------------------------------------------------ CHORUS
// Two moving delays per side, a quarter turn apart left to right, so the sound widens as well as thickens.
// Short delay + feedback gives a flanger.
class Chorus
{
public:
    float rate = 0.6f, depthMs = 3.f, delayMs = 12.f, feedback = 0.f, mix = 0.5f;
    float phaseNow = 0.f;                                                // for the display

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : line) d.prepare ((int) (sr * 0.06));
        phase = 0.f;
        fb[0] = fb[1] = 0.f;
        gMix.setTime (sr, 20.f);   gMix.snap (mix);
        gDelay.setTime (sr, 60.f); gDelay.snap (delayMs);
        gDepth.setTime (sr, 60.f); gDepth.snap (depthMs);
    }

    void process (float* L, float* R, int n)
    {
        float* io[2] = { L, R };
        const float ms = (float) sr * 0.001f;
        for (int i = 0; i < n; ++i)
        {
            phase += rate / (float) sr;
            if (phase >= 1.f) phase -= 1.f;
            const float m = gMix.next (mix), base = gDelay.next (delayMs), dep = std::min (gDepth.next (depthMs), base - 0.5f);
            for (int c = 0; c < 2; ++c)
            {
                const float dry = io[c][i];
                const float p = phase + 0.25f * (float) c;
                const float d1 = (base + dep * std::sin (2.f * pi * p)) * ms, d2 = (base + dep * std::sin (2.f * pi * (p + 0.5f))) * ms;
                const float wet = 0.5f * (line[c].read (d1) + line[c].read (d2));
                line[c].write (dry + feedback * fb[c]);
                fb[c] = std::tanh (wet);                                 // keeps high feedback from running away
                io[c][i] = dry + m * (wet - dry);
            }
        }
        phaseNow = phase;
    }

private:
    DelayLine line[2];
    Smooth gMix, gDelay, gDepth;
    float phase = 0.f, fb[2] {};
    double sr = 44100.0;
};

// ------------------------------------------------------------------ DELAY (echo)
// Stereo echo or ping-pong (left, right, left...). The repeats pass through a low cut and a high cut each time round,
// so they get thinner and darker like tape and analog delays. Changing the time glides, it does not click.
class Delay
{
public:
    float timeMs = 375.f, feedback = 0.35f, lowCut = 120.f, highCut = 6000.f, mix = 0.25f;
    bool pingPong = false;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : line) d.prepare ((int) (sr * 2.6));
        t.setTime (sr, 120.f);
        t.snap (timeMs);
        gMix.setTime (sr, 20.f);
        gMix.snap (mix);
        for (auto& s : lp) s = 0.f;
        for (auto& s : hp) s = 0.f;
    }

    void process (float* L, float* R, int n)
    {
        const float lpA = 1.f - std::exp (-2.f * pi * std::min (highCut, (float) sr * 0.45f) / (float) sr);
        const float hpA = 1.f - std::exp (-2.f * pi * lowCut / (float) sr);
        const float fbk = std::clamp (feedback, 0.f, 0.97f);
        for (int i = 0; i < n; ++i)
        {
            const float d = std::clamp (t.next (timeMs), 1.f, 2500.f) * 0.001f * (float) sr, m = gMix.next (mix);
            float wl = line[0].read (d), wr = line[1].read (d);
            float fl = wl, fr = wr;                                      // the repeats, filtered
            lp[0] += lpA * (fl - lp[0]);  fl = lp[0];   hp[0] += hpA * (fl - hp[0]);  fl -= hp[0];
            lp[1] += lpA * (fr - lp[1]);  fr = lp[1];   hp[1] += hpA * (fr - hp[1]);  fr -= hp[1];
            fl = std::tanh (fl);  fr = std::tanh (fr);
            if (pingPong)
            {
                line[0].write (0.5f * (L[i] + R[i]) + fbk * fr);
                line[1].write (fbk * fl);
            }
            else
            {
                line[0].write (L[i] + fbk * fl);
                line[1].write (R[i] + fbk * fr);
            }
            L[i] += m * wl;
            R[i] += m * wr;
        }
    }

private:
    DelayLine line[2];
    Smooth t, gMix;
    float lp[2] {}, hp[2] {};
    double sr = 44100.0;
};

// ------------------------------------------------------------------ WIDTH (stereo)
// width: 0 = mono, 1 = unchanged, 2 = extra wide. monoBelow keeps everything under that frequency in the centre
// (tight 808s and kicks that still work on one speaker). 20 Hz or less switches that off.
class Width
{
public:
    float width = 1.f, monoBelow = 20.f, outDb = 0.f;
    float correlation = 1.f;                                             // +1 = mono-safe, 0 = very wide, below 0 = will thin out in mono

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        hp.reset();
        w.setTime (sr, 20.f);     w.snap (width);
        gOut.setTime (sr, 20.f);  gOut.snap (dbToGain (outDb));
        sLR = 0.f;  sLL = sRR = 1.0e-9f;
        avg = 1.f - std::exp (-1.f / (0.3f * (float) sr));
    }

    void process (float* L, float* R, int n)
    {
        const float outT = dbToGain (outDb);
        const bool useHp = monoBelow > 20.5f;
        if (useHp) hp.set (monoBelow, 0.71f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float mid = 0.5f * (L[i] + R[i]);
            float side = 0.5f * (L[i] - R[i]);
            if (useHp) { float l, b, h; hp.tick (side, l, b, h); side = h; }
            side *= w.next (width);
            const float g = gOut.next (outT);
            L[i] = (mid + side) * g;
            R[i] = (mid - side) * g;
            sLR += avg * (L[i] * R[i] - sLR);
            sLL += avg * (L[i] * L[i] - sLL);
            sRR += avg * (R[i] * R[i] - sRR);
        }
        correlation = sLR / std::sqrt (std::max (1.0e-12f, sLL * sRR));
    }

private:
    Svf hp;
    Smooth w, gOut;
    float sLR = 0.f, sLL = 1.0e-9f, sRR = 1.0e-9f, avg = 0.001f;
    double sr = 44100.0;
};

// ------------------------------------------------------------------ LIMITER (mastering, look-ahead)
// It reads the signal 3 ms ahead, so the gain is already down when a peak arrives: no clipping and no click.
// The output never goes over the ceiling. gainDb pushes the level up into it (louder).
class Limiter
{
public:
    float gainDb = 0.f, ceilingDb = -0.3f, releaseMs = 120.f;
    float grDb = 0.f, inPeak = 0.f, outPeak = 0.f;                       // for the display
    float preClampPeak = 0.f;                                            // for testing: the highest level before the safety clamp

    int latency() const { return look; }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        look = std::max (8, (int) (sr * 0.003));
        for (auto& d : audio) d.prepare (look + 8);
        wedgeVal.assign ((size_t) look + 4, 1.f);
        wedgeIdx.assign ((size_t) look + 4, 0);
        head = tail = 0;
        count = 0;
        box.assign ((size_t) look, 1.f);
        boxPos = 0;
        boxSum = (double) look;
        rel = 1.f;
        gIn.setTime (sr, 20.f);
        gIn.snap (dbToGain (gainDb));
    }

    void process (float* L, float* R, int n)
    {
        const float inT = dbToGain (gainDb), ceil = dbToGain (ceilingDb);
        const float relC = 1.f - std::exp (-1.f / (0.001f * std::max (1.f, releaseMs) * (float) sr));
        const int cap = (int) wedgeVal.size();
        float pin = 0.f, pout = 0.f, minG = 1.f;
        for (int i = 0; i < n; ++i)
        {
            const float gi = gIn.next (inT);
            const float l = L[i] * gi, r = R[i] * gi;
            const float peak = std::max (std::abs (l), std::abs (r));
            pin = std::max (pin, peak);
            const float need = peak > ceil ? ceil / peak : 1.f;
            rel = std::min (need, rel + relC * (1.f - rel));              // drops at once, recovers at the release speed

            // lowest gain needed over the next `look` samples (sliding minimum)
            while (head != tail && wedgeVal[(size_t) ((tail + cap - 1) % cap)] >= rel) tail = (tail + cap - 1) % cap;
            wedgeVal[(size_t) tail] = rel;
            wedgeIdx[(size_t) tail] = count;
            tail = (tail + 1) % cap;
            if (count - wedgeIdx[(size_t) head] > look) head = (head + 1) % cap;
            const float lowest = wedgeVal[(size_t) head];
            ++count;

            // averaged over the same length: the gain ramps down smoothly and is fully there when the peak comes out
            boxSum += (double) lowest - (double) box[(size_t) boxPos];
            box[(size_t) boxPos] = lowest;
            if (++boxPos >= look) boxPos = 0;
            const float g = std::min (1.f, (float) (boxSum / (double) look));

            audio[0].write (l);
            audio[1].write (r);
            float ol = audio[0].readInt (look) * g, orr = audio[1].readInt (look) * g;
            preClampPeak = std::max (preClampPeak, std::max (std::abs (ol), std::abs (orr)));
            ol = std::clamp (ol, -ceil, ceil);                           // a safety net only: the look-ahead has already done the work
            orr = std::clamp (orr, -ceil, ceil);
            L[i] = ol;
            R[i] = orr;
            minG = std::min (minG, g);
            pout = std::max (pout, std::max (std::abs (ol), std::abs (orr)));
        }
        grDb = gainToDb (minG);
        inPeak = pin;
        outPeak = pout;
    }

private:
    DelayLine audio[2];
    std::vector<float> wedgeVal, box;
    std::vector<long long> wedgeIdx;
    long long count = 0;                                                 // 64-bit: never wraps, however long the session
    int head = 0, tail = 0, boxPos = 0, look = 132;
    double boxSum = 0.0, sr = 44100.0;
    float rel = 1.f;
    Smooth gIn;
};

// ================================================================== VOCAL BOOTH
// Standard two-pole EQ filter (RBJ cookbook). magnitude() gives the exact curve for the screen.
struct Biquad
{
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f, z1 = 0.f, z2 = 0.f;
    void reset() { z1 = z2 = 0.f; }
    float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void set (float B0, float B1, float B2, float A0, float A1, float A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
    void bell (float f, float db, float q, double sr)
    {
        const float A = std::pow (10.f, db / 40.f), w = 2.f * pi * std::min (f, (float) sr * 0.45f) / (float) sr, al = std::sin (w) / (2.f * q), c = std::cos (w);
        set (1.f + al * A, -2.f * c, 1.f - al * A, 1.f + al / A, -2.f * c, 1.f - al / A);
    }
    void lowShelf (float f, float db, double sr)
    {
        const float A = std::pow (10.f, db / 40.f), w = 2.f * pi * f / (float) sr, c = std::cos (w), al = std::sin (w) / 2.f * std::sqrt (2.f), r = 2.f * std::sqrt (A) * al;
        set (A * ((A + 1.f) - (A - 1.f) * c + r), 2.f * A * ((A - 1.f) - (A + 1.f) * c), A * ((A + 1.f) - (A - 1.f) * c - r),
             (A + 1.f) + (A - 1.f) * c + r, -2.f * ((A - 1.f) + (A + 1.f) * c), (A + 1.f) + (A - 1.f) * c - r);
    }
    void highShelf (float f, float db, double sr)
    {
        const float A = std::pow (10.f, db / 40.f), w = 2.f * pi * std::min (f, (float) sr * 0.45f) / (float) sr, c = std::cos (w), al = std::sin (w) / 2.f * std::sqrt (2.f), r = 2.f * std::sqrt (A) * al;
        set (A * ((A + 1.f) + (A - 1.f) * c + r), -2.f * A * ((A - 1.f) + (A + 1.f) * c), A * ((A + 1.f) + (A - 1.f) * c - r),
             (A + 1.f) - (A - 1.f) * c + r, 2.f * ((A - 1.f) - (A + 1.f) * c), (A + 1.f) - (A - 1.f) * c - r);
    }
    void highPass (float f, float q, double sr)
    {
        const float w = 2.f * pi * f / (float) sr, c = std::cos (w), al = std::sin (w) / (2.f * q);
        set ((1.f + c) * 0.5f, -(1.f + c), (1.f + c) * 0.5f, 1.f + al, -2.f * c, 1.f - al);
    }
    float magnitude (float f, double sr) const
    {
        const float w = 2.f * pi * f / (float) sr, c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2.f * w), s2 = std::sin (2.f * w);
        const float nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2), dr = 1.f + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        return std::sqrt ((nr * nr + ni * ni) / std::max (1.0e-20f, dr * dr + di * di));
    }
};

// ------------------------------------------------------------------ MIC (microphone character)
// Ten tonal characters modelled on the published descriptions of classic studio vocal microphones: how much low-end weight,
// where the presence lift sits, how much air on top, and how much valve or transformer colour. They shape a reasonably neutral
// recording towards that character. They are not measurements of the real microphones.
struct MicProfile { const char* name; const char* inspiredBy; const char* note; float hp, lowF, lowDb, midF, midDb, midQ, presF, presDb, presQ, airF, airDb, drive; };

inline const std::array<MicProfile, 11>& micProfiles()
{
    static const std::array<MicProfile, 11> p {{
        { "Off (as recorded)",  "",                                   "No emulation.",                                                        20,  100, 0.0f,  1000, 0.0f, 1.0f, 4000, 0.0f, 1.0f, 12000, 0.0f, 0.0f },
        { "800 Air",            "Sony C-800G (tube)",                 "Ultra clear, forward, airy top. The modern rap and R&B lead sound.",   60,  120, 0.5f,  320, -1.5f, 1.0f, 5200, 2.5f, 0.9f, 12000, 5.0f, 0.10f },
        { "87 Studio",          "Neumann U 87 (FET)",                 "Even and forward. The studio standard that sits in any mix.",          50,  110, 0.0f,  450, -0.8f, 0.9f, 4200, 1.8f, 1.0f,  9500, 2.0f, 0.03f },
        { "47 Tube",            "Neumann U 47 (tube)",                "Big lows, solid mids, air near 10 kHz. Weight for deeper voices.",     35,  130, 2.8f, 1500, 1.0f, 0.8f, 5000, 2.0f, 1.0f, 10000, 1.8f, 0.28f },
        { "67 Tube",            "Neumann U 67 (tube)",                "Forward, detailed mids and a smooth top that is never harsh.",         40,  120, 1.0f, 2500, 1.8f, 0.8f, 6500, -0.8f, 1.2f, 10000, -1.5f, 0.24f },
        { "251 Silk",           "Telefunken ELA M 251 (tube)",        "Deep lows, detailed mids, silky highs without mud.",                   35,   85, 1.6f,  300, -1.0f, 1.0f, 3200, 1.2f, 0.9f, 10000, 3.6f, 0.16f },
        { "C12 Open",           "AKG C12 (tube)",                     "Clear, natural, open top end with a lean low end.",                    55,  140, -1.0f, 1200, 0.5f, 0.8f, 8000, 3.0f, 0.7f, 13000, 2.2f, 0.12f },
        { "49 Velvet",          "Neumann M 49 (tube)",                "Silky, smooth, full and rich. Flatters bright or thin voices.",        35,  150, 2.0f,  800, 0.6f, 0.8f, 6000, -1.2f, 1.0f, 11000, -1.0f, 0.22f },
        { "7B Broadcast",       "Shure SM7B (dynamic)",               "Tight and dry with a presence lift and little air. Aggressive rap.",   85,  200, -1.5f, 600, -0.5f, 1.0f, 4500, 2.8f, 0.9f, 12000, -4.0f, 0.0f },
        { "44 Ribbon",          "RCA 44 (ribbon)",                    "Warm, huge low end, dark top. Old-school crooner weight.",             30,  100, 4.0f,  500, 0.8f, 0.8f, 4000, -2.0f, 0.9f,  9000, -6.0f, 0.08f },
        { "Reference Modern",   "Manley Reference Cardioid (tube)",   "Clarity and presence up front. A modern hip-hop and pop sound.",       50,  110, 0.5f,  350, -1.0f, 1.0f, 3000, 2.0f, 0.9f,  8500, 3.2f, 0.14f },
    }};
    return p;
}

class MicModel
{
public:
    void prepare (double sampleRate) { sr = sampleRate; for (auto& f : flt) f.reset(); set (current); }
    void set (int index)
    {
        current = std::clamp (index, 0, (int) micProfiles().size() - 1);
        const auto& m = micProfiles()[(size_t) current];
        flt[0].highPass (m.hp, 0.71f, sr);
        flt[1].lowShelf (m.lowF, m.lowDb, sr);
        flt[2].bell (m.midF, m.midDb, m.midQ, sr);
        flt[3].bell (m.presF, m.presDb, m.presQ, sr);
        flt[4].highShelf (m.airF, m.airDb, sr);
        drive = m.drive;
        dg = 1.f + 6.f * drive;
        dt = std::tanh (dg);
        norm = (1.f - drive) + drive * dg / dt;
    }
    int index() const { return current; }
    float tick (float x)
    {
        if (current == 0) return x;
        for (auto& f : flt) x = f.tick (x);
        if (drive > 0.f) x = ((1.f - drive) * x + drive * std::tanh (x * dg) / dt) / norm;   // valve-style rounding on the loud peaks; quiet sound passes at the same level
        return x;
    }
    float responseDb (float hz) const                                   // for the screen
    {
        if (current == 0) return 0.f;
        float m = 1.f;
        for (const auto& f : flt) m *= f.magnitude (hz, sr);
        return gainToDb (m);
    }
private:
    Biquad flt[5];
    double sr = 44100.0;
    float drive = 0.f, dg = 1.f, dt = 0.76f, norm = 1.f;
    int current = 0;
};

// ------------------------------------------------------------------ vocal chain pieces (mono)
// GATE: shuts the mic between phrases so breaths, headphone bleed and room noise drop away.
class Gate
{
public:
    float threshDb = -50.f, releaseMs = 150.f, rangeDb = -30.f;
    float gainNow = 1.f;
    void prepare (double sampleRate) { sr = sampleRate; env = 0.f; gDb = 0.f; hold = 0; update(); }
    float tick (float x)
    {
        const float a = std::abs (x);
        env = a > env ? a : env * envRel;
        const float th = dbToGain (threshDb);
        if (env > th) hold = (int) (0.03 * sr);                           // stays open 30 ms after the level drops, so word endings are not chopped
        else if (hold > 0) --hold;
        const float target = (env > th || hold > 0) ? 0.f : rangeDb;    // in dB, so it closes evenly
        gDb += (target > gDb ? openC : closeC) * (target - gDb);
        gainNow = dbToGain (gDb);
        return x * gainNow;
    }
    void update()
    {
        envRel = std::exp (-1.f / (0.01f * (float) sr));
        openC = 1.f - std::exp (-1.f / (0.002f * (float) sr));
        closeC = 1.f - std::exp (-1.f / (0.001f * std::max (5.f, releaseMs) * (float) sr));
    }
private:
    double sr = 44100.0;
    float env = 0.f, gDb = 0.f, envRel = 0.999f, openC = 0.01f, closeC = 0.001f;
    int hold = 0;
};

// DE-ESSER: a high shelf that only turns down when the "s" and "t" band gets loud. The body of the voice is not touched.
class DeEsser
{
public:
    float freq = 6500.f, threshDb = -30.f, maxCutDb = 8.f;
    float cutNow = 0.f;                                                  // dB being taken off right now (for the screen)
    void prepare (double sampleRate) { sr = sampleRate; detect.reset(); shelf.reset(); env = 0.f; cut = 0.f; counter = 0; update(); }
    void update() { detect.set (freq, 0.71f, sr); att = 1.f - std::exp (-1.f / (0.0005f * (float) sr)); rel = 1.f - std::exp (-1.f / (0.06f * (float) sr)); }
    float tick (float x)
    {
        float lp, bp, hp;
        detect.tick (x, lp, bp, hp);                                     // listens to the top band only
        const float a = std::abs (hp);
        env += (a > env ? att : rel) * (a - env);
        if ((counter++ & 15) == 0)
        {
            const float over = gainToDb (env) - threshDb;
            cut = over > 0.f ? std::min (maxCutDb, over * 0.7f) : 0.f;
            shelf.highShelf (freq, -cut, sr);
            cutNow = cut;
        }
        return shelf.tick (x);
    }
private:
    Svf detect;
    Biquad shelf;
    double sr = 44100.0;
    float env = 0.f, att = 0.1f, rel = 0.001f, cut = 0.f;
    int counter = 0;
};

// VOCAL COMPRESSOR: soft knee, feed-forward. Evens out the level so every word is heard.
class VocalComp
{
public:
    float threshDb = -20.f, ratio = 4.f, attackMs = 3.f, releaseMs = 60.f, makeupDb = 4.f;
    float grNow = 0.f;                                                   // dB of gain reduction (negative)
    void prepare (double sampleRate) { sr = sampleRate; grDb = 0.f; update(); }
    void update() { att = 1.f - std::exp (-1.f / (0.001f * std::max (0.1f, attackMs) * (float) sr)); rel = 1.f - std::exp (-1.f / (0.001f * std::max (5.f, releaseMs) * (float) sr)); mk = dbToGain (makeupDb); }
    float tick (float x)
    {
        const float in = gainToDb (std::abs (x)), knee = 6.f, over = in - threshDb;
        float out = in;
        if (2.f * std::abs (over) <= knee) { const float t = over + knee * 0.5f; out = in + (1.f / ratio - 1.f) * t * t / (2.f * knee); }
        else if (over > 0.f) out = threshDb + over / ratio;
        const float target = out - in;                                   // 0 or negative
        grDb += (target < grDb ? att : rel) * (target - grDb);
        grNow = grDb;
        return x * dbToGain (grDb) * mk;
    }
private:
    double sr = 44100.0;
    float grDb = 0.f, att = 0.1f, rel = 0.001f, mk = 1.f;
};

// VOCAL EQ: the four moves made on nearly every vocal. Low cut, a dip in the boxy area, a presence lift for the words, air on top.
class VocalEq
{
public:
    float lowCut = 90.f, mudDb = -3.f, mudF = 320.f, presDb = 2.5f, presF = 4000.f, airDb = 2.f;
    void prepare (double sampleRate) { sr = sampleRate; for (auto& f : flt) f.reset(); update(); }
    void update()
    {
        flt[0].highPass (lowCut, 0.71f, sr);
        flt[1].bell (mudF, mudDb, 1.2f, sr);
        flt[2].bell (presF, presDb, 1.0f, sr);
        flt[3].highShelf (11000.f, airDb, sr);
    }
    float tick (float x) { for (auto& f : flt) x = f.tick (x); return x; }
    float responseDb (float hz) const { float m = 1.f; for (const auto& f : flt) m *= f.magnitude (hz, sr); return gainToDb (m); }
private:
    Biquad flt[4];
    double sr = 44100.0;
};

// ------------------------------------------------------------------ TUNE (pitch correction)
// Listens to the note being sung, finds the nearest note of the chosen key and scale, and moves the voice onto it.
// The pitch is found with the McLeod method (a normalised autocorrelation). The voice is re-pitched one pitch period at a time
// (pitch-synchronous overlap-add), which keeps the character of the voice: only the note changes, not the vowel sound.
// speedMs = how fast it pulls to the note: 0 is the hard, robotic snap; 100-300 is natural. One voice at a time.
namespace tune
{
    inline const char* const scaleNames[6] = { "Chromatic", "Major", "Minor", "Major pentatonic", "Minor pentatonic", "Blues" };
    inline const int scaleMasks[6] = { 0xfff, 0xab5, 0x5ad, 0x295, 0x4a9, 0x4e9 };    // bit n = that many semitones above the key note is allowed
    inline bool allowed (int midiNote, int key, int scale) { return ((scaleMasks[scale] >> ((((midiNote - key) % 12) + 12) % 12)) & 1) != 0; }
    inline float noteToHz (float note) { return 440.f * std::pow (2.f, (note - 69.f) / 12.f); }
    inline float hzToNote (float hz) { return 69.f + 12.f * std::log2 (hz / 440.f); }
}

class Tuner
{
public:
    int key = 0, scale = 0;                                              // key: 0 = C ... 11 = B
    float speedMs = 20.f, amount = 1.f;
    float detectedNote = 0.f, targetNote = 0.f;                          // for the screen (0 = no clear note right now)

    int latency() const { return delay; }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        maxP = (int) (sr / 75.0);                                        // lowest note it follows: about 75 Hz
        delay = 2 * maxP + 4;
        in.assign (size, 0.f);
        acc.assign (size, 0.f);
        dec.assign (decSize, 0.f);
        t = 0;  decW = 0;  hopCount = 0;  decToggle = false;  decHold = 0.f;
        P = (float) sr / 200.f;  logRatio = 0.f;  voiced = false;  heldNote = -1;
        srcPos = 0.0;  nextCentre = (double) delay;
        detectedNote = targetNote = 0.f;
    }

    float tick (float x)
    {
        in[(size_t) (t & mask)] = x;
        decHold += x;                                                    // the detector runs at half the sample rate
        if (decToggle) { dec[(size_t) (decW++ % decSize)] = 0.5f * decHold; decHold = 0.f; }
        decToggle = ! decToggle;
        if (++hopCount >= hop) { hopCount = 0; detect(); }

        while ((double) t >= nextCentre - (double) P) placeGrain();
        const float y = acc[(size_t) (t & mask)];
        acc[(size_t) (t & mask)] = 0.f;
        ++t;
        return y;
    }

private:
    void detect()
    {
        const int W = juce_free_min (decSize, (int) (sr * 0.5 * 0.034)), minLag = (int) (sr * 0.5 / 800.0), maxLag = juce_free_min (W / 2, (int) (sr * 0.5 / 75.0));
        float* w = work;
        double energy = 0.0;
        for (int i = 0; i < W; ++i) { w[i] = dec[(size_t) ((decW - W + i + decSize * 4) % decSize)]; energy += w[i] * w[i]; }
        float best = 0.f, bestLag = 0.f;
        if (energy / W > 1.0e-7)
        {
            // normalised square difference function; take the first strong peak after the first dip (the McLeod "key maximum")
            float prev2 = 1.f, prev1 = 1.f, top = 0.f;
            bool seenDip = false;
            static thread_local float nsdf[1024];
            for (int lag = minLag; lag <= maxLag + 1 && lag < 1024; ++lag)
            {
                float ac = 0.f, m = 0.f;
                for (int j = 0; j < W - lag; ++j) { ac += w[j] * w[j + lag]; m += w[j] * w[j] + w[j + lag] * w[j + lag]; }
                nsdf[lag] = m > 0.f ? 2.f * ac / m : 0.f;
                top = std::max (top, nsdf[lag]);
            }
            const float need = 0.88f * top;
            for (int lag = minLag; lag <= maxLag + 1 && lag < 1024; ++lag)
            {
                const float v = nsdf[lag];
                if (v < 0.f) seenDip = true;
                if (seenDip && lag >= minLag + 2 && prev1 > prev2 && prev1 >= v && prev1 >= need)
                {
                    const float a = prev2, b = prev1, c = v, den = a - 2.f * b + c;       // parabola through the three points round the peak
                    bestLag = (float) (lag - 1) + (std::abs (den) > 1.0e-9f ? 0.5f * (a - c) / den : 0.f);
                    best = b;
                    break;
                }
                prev2 = prev1;
                prev1 = v;
            }
        }
        voiced = best > 0.6f && bestLag > 0.f;
        const float coef = speedMs <= 0.5f ? 1.f : 1.f - std::exp (-(float) hop / (0.001f * speedMs * (float) sr));
        if (voiced)
        {
            P = std::clamp (bestLag * 2.f, (float) sr / 800.f, (float) maxP);
            const float note = tune::hzToNote ((float) sr / P);
            if (heldNote < 0 || std::abs (note - (float) heldNote) > 0.62f || ! tune::allowed (heldNote, key, scale))   // stays on a note until the voice clearly moves off it
            {
                const int centre = (int) std::lround (note);
                float bestDist = 99.f;
                for (int n = centre - 6; n <= centre + 6; ++n)
                    if (tune::allowed (n, key, scale) && std::abs ((float) n - note) < bestDist) { bestDist = std::abs ((float) n - note); heldNote = n; }
            }
            const float want = amount * ((float) heldNote - note) / 12.f;                   // in octaves
            logRatio += coef * (want - logRatio);
            detectedNote = note;
            targetNote = (float) heldNote;
        }
        else
        {
            logRatio += 0.3f * (0.f - logRatio);                                             // nothing clear to follow: ease back to no change
            heldNote = -1;
            detectedNote = targetNote = 0.f;
        }
    }

    void placeGrain()
    {
        const float ratio = std::pow (2.f, logRatio);
        const int half = std::max (8, (int) std::lround (P));
        const double ideal = nextCentre - (double) delay;                // where in the input this grain should come from
        if (voiced && std::abs (srcPos - ideal) < 3.0 * (double) P)
            srcPos += (double) P * std::max (0.0, std::round ((ideal - srcPos) / (double) P));   // step a whole number of periods so the waves line up
        else
            srcPos = ideal;
        if (srcPos + (double) half + 3.0 > (double) t) srcPos = (double) t - (double) half - 3.0;   // never read ahead of what has arrived
        const long long c = (long long) std::llround (nextCentre);
        const float gain = 1.f / ratio;                                   // closer grains would be louder; this evens it out
        for (int j = -half - 1; j <= half + 1; ++j)
        {
            const double d = (double) (c + j) - nextCentre;               // how far this output sample is from the true centre of the grain
            if (std::abs (d) >= (double) P) continue;
            const float win = 0.5f + 0.5f * std::cos (pi * (float) (d / (double) P));
            const double sp = srcPos + d;                                 // the matching point in the input, between samples if need be
            const long long i1 = (long long) std::floor (sp);
            const float fr = (float) (sp - (double) i1);
            const float y0 = in[(size_t) ((i1 - 1) & mask)], y1 = in[(size_t) (i1 & mask)], y2 = in[(size_t) ((i1 + 1) & mask)], y3 = in[(size_t) ((i1 + 2) & mask)];
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            acc[(size_t) ((c + j) & mask)] += win * gain * (((c3 * fr + c2) * fr + c1) * fr + y1);
        }
        nextCentre += (double) P / (double) ratio;
    }

    static int juce_free_min (int a, int b) { return a < b ? a : b; }
    static constexpr int size = 16384, mask = size - 1, decSize = 1024, hop = 384;
    std::vector<float> in, acc, dec;
    float work[decSize] {};
    double sr = 44100.0, srcPos = 0.0, nextCentre = 0.0;
    long long t = 0;
    int decW = 0, hopCount = 0, maxP = 640, delay = 1280, heldNote = -1;
    float P = 220.f, logRatio = 0.f, decHold = 0.f;
    bool voiced = false, decToggle = false;
};
}
