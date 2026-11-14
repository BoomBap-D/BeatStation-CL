#pragma once
#include "MasterFx.h"
#include "Theme.h"

// Pro-Q style graph: live spectrum, summed response curve, draggable band nodes.
// Drag a node = frequency + gain, mouse wheel = Q, double-click = reset gain.
class MasterEqView : public juce::Component, private juce::Timer
{
public:
    explicit MasterEqView (MasterFx& p) : proc (p) { startTimerHz (30); }

    std::function<void (int)> onSelect;
    int selected = 2;

    void paint (juce::Graphics& g) override
    {
        const auto full = getLocalBounds().toFloat();
        g.setColour (Theme::graphBg);
        g.fillRoundedRectangle (full, 8.f);
        g.setColour (Theme::grid.brighter (0.1f));
        g.drawRoundedRectangle (full.reduced (0.5f), 8.f, 1.f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (full.toNearestInt());
        const auto b = plot();

        // Grid
        g.setFont (11.f);
        for (float f : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
        {
            const float x = freqToX (f);
            g.setColour (Theme::grid);
            g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
            g.setColour (Theme::dim);
            g.drawText (f >= 1000.f ? juce::String ((int) (f / 1000.f)) + "k" : juce::String ((int) f),
                        (int) x + 3, (int) b.getBottom() - 16, 40, 14, juce::Justification::left);
        }
        for (int db : { -12, -6, 0, 6, 12 })
        {
            const float y = gainToY ((float) db);
            g.setColour (db == 0 ? Theme::grid.brighter (0.3f) : Theme::grid);
            g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
            g.setColour (Theme::dim);
            g.drawText (juce::String (db), (int) b.getX() + 4, (int) y - 14, 30, 14, juce::Justification::left);
        }

        // Spectrum
        if ((int) smooth.size() == (int) b.getWidth() && ! smooth.empty())
        {
            juce::Path sp;
            sp.startNewSubPath (b.getX(), b.getBottom());
            for (size_t i = 0; i < smooth.size(); ++i)
                sp.lineTo (b.getX() + (float) i, b.getBottom() - juce::jmap (smooth[i], -100.f, -10.f, 0.f, b.getHeight()));
            sp.lineTo (b.getRight(), b.getBottom());
            sp.closeSubPath();
            g.setColour (Theme::amber.withAlpha (0.14f));
            g.fillPath (sp);
        }

        // Response curve
        const double sr = proc.getSR();
        std::vector<MasterFx::Coeffs> co;
        for (int i = 0; i < MfxID::numBands; ++i) co.push_back (proc.makeBandCoeffs (i, sr));

        juce::Path curve;
        for (float x = b.getX(); x <= b.getRight(); x += 2.f)
        {
            double mag = 1.0;
            for (auto& c : co) mag *= c->getMagnitudeForFrequency (xToFreq (x), sr);
            const float y = gainToY (juce::Decibels::gainToDecibels ((float) mag));
            if (x == b.getX()) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        juce::Path fill (curve);
        fill.lineTo (b.getRight(), gainToY (0.f));
        fill.lineTo (b.getX(), gainToY (0.f));
        fill.closeSubPath();
        g.setColour (Theme::amber.withAlpha (0.10f));
        g.fillPath (fill);
        g.setColour (Theme::text);
        g.strokePath (curve, juce::PathStrokeType (2.2f));

        // Nodes
        for (int i = 0; i < MfxID::numBands; ++i)
        {
            const auto p = nodePos (i);
            const bool sel = (i == selected);
            if (sel) { g.setColour (Theme::bandColours[i].withAlpha (0.25f)); g.fillEllipse (p.x - 15.f, p.y - 15.f, 30.f, 30.f); }
            g.setColour (Theme::bandColours[i]);
            g.fillEllipse (p.x - 8.f, p.y - 8.f, 16.f, 16.f);
            g.setColour (Theme::graphBg);
            g.drawEllipse (p.x - 8.f, p.y - 8.f, 16.f, 16.f, 2.f);
            g.setFont (juce::Font (10.f, juce::Font::bold));
            g.drawText (juce::String (i + 1), (int) p.x - 8, (int) p.y - 7, 16, 14, juce::Justification::centred);
        }

        // Readout for the selected band
        g.setColour (Theme::text);
        g.setFont (12.f);
        g.drawText ("BAND " + juce::String (selected + 1) + "   " + juce::String (val (MfxID::freq (selected)), 0) + " Hz   "
                      + juce::String (val (MfxID::gain (selected)), 1) + " dB   Q " + juce::String (val (MfxID::q (selected)), 2),
                    (int) b.getX() + 40, (int) b.getY() + 6, 400, 16, juce::Justification::left);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int h = hit (e.position);
        if (h < 0) return;
        selected = dragBand = h;
        if (onSelect) onSelect (h);
        param (MfxID::freq (h))->beginChangeGesture();
        param (MfxID::gain (h))->beginChangeGesture();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragBand < 0) return;
        const auto b = plot();
        set (MfxID::freq (dragBand), juce::jlimit (20.f, 20000.f, xToFreq (e.position.x)));
        set (MfxID::gain (dragBand), juce::jlimit (-18.f, 18.f, (b.getCentreY() - e.position.y) / (b.getHeight() * 0.5f) * dbRange));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragBand < 0) return;
        param (MfxID::freq (dragBand))->endChangeGesture();
        param (MfxID::gain (dragBand))->endChangeGesture();
        dragBand = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int h = hit (e.position);
        if (h < 0) return;
        auto* p = param (MfxID::gain (h));
        p->beginChangeGesture(); set (MfxID::gain (h), 0.f); p->endChangeGesture();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int h = hit (e.position) >= 0 ? hit (e.position) : selected;
        auto* p = param (MfxID::q (h));
        p->beginChangeGesture();
        set (MfxID::q (h), juce::jlimit (0.1f, 18.f, val (MfxID::q (h)) * std::pow (1.15f, w.deltaY * 4.f)));
        p->endChangeGesture();
    }

private:
    static constexpr float dbRange = 20.f;   // vertical axis shows +/- 20 dB

    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (2.f); }
    float freqToX (float f) const { const auto b = plot(); return b.getX() + b.getWidth() * std::log10 (f / 20.f) / 3.f; }
    float xToFreq (float x) const { const auto b = plot(); return 20.f * std::pow (10.f, 3.f * (x - b.getX()) / b.getWidth()); }
    float gainToY (float db) const { const auto b = plot(); return b.getCentreY() - db * (b.getHeight() * 0.5f) / dbRange; }
    juce::Point<float> nodePos (int i) const { return { freqToX (val (MfxID::freq (i))), gainToY (val (MfxID::gain (i))) }; }
    float val (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    juce::RangedAudioParameter* param (const juce::String& id) const { return proc.apvts.getParameter (id); }
    void set (const juce::String& id, float v) { auto* p = param (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); }

    int hit (juce::Point<float> pt) const
    {
        int best = -1; float bestD = 14.f;
        for (int i = 0; i < MfxID::numBands; ++i)
            if (const float d = nodePos (i).getDistanceFrom (pt); d < bestD) { bestD = d; best = i; }
        return best;
    }

    void timerCallback() override
    {
        const auto b = plot();
        const int w = (int) b.getWidth();
        if ((int) smooth.size() != w) smooth.assign ((size_t) juce::jmax (0, w), -100.f);

        if (proc.fftReady.load())
        {
            scratch = proc.fftData;
            proc.fftReady = false;
            window.multiplyWithWindowingTable (scratch.data(), (size_t) MasterFx::fftSize);
            fft.performFrequencyOnlyForwardTransform (scratch.data());
            const float norm = juce::Decibels::gainToDecibels ((float) MasterFx::fftSize);
            for (int x = 0; x < w; ++x)
            {
                const float f = xToFreq (b.getX() + (float) x);
                const int bin = juce::jlimit (1, MasterFx::fftSize / 2 - 1, (int) (f / (float) (proc.getSR() * 0.5) * (MasterFx::fftSize / 2)));
                const float db = juce::Decibels::gainToDecibels (scratch[(size_t) bin]) - norm + 4.5f * std::log2 (f / 1000.f);   // 4.5 dB/oct tilt
                smooth[(size_t) x] = 0.75f * smooth[(size_t) x] + 0.25f * juce::jmax (db, -100.f);
            }
        }
        repaint();
    }

    MasterFx& proc;
    juce::dsp::FFT fft { MasterFx::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) MasterFx::fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, MasterFx::fftSize * 2> scratch {};
    std::vector<float> smooth;
    int dragBand = -1;
};
