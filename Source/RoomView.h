#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include "Widgets.h"
#include "RoomFigures.h"

// The picture of the room: drag the source (the performer) or the listener around the floor.
// The dotted lines are the first wall reflections the engine is actually calculating.
class RoomScene : public juce::Component
{
public:
    explicit RoomScene (RoomFx& r) : fx (r), rapper (RoomFigures::make (RoomFigures::rapperSvg)), singer (RoomFigures::make (RoomFigures::singerSvg)) {}

    void setPlan (bool v) { plan = v; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto full = getLocalBounds().toFloat();
        g.setColour (Theme::graphBg);
        g.fillRoundedRectangle (full, 8.f);
        g.setColour (Theme::grid.brighter (0.1f));
        g.drawRoundedRectangle (full.reduced (0.5f), 8.f, 1.f);

        const auto p = fx.settings();
        const auto a = project ({ 0, 0, 0 }), b = project ({ 1, 0, 0 }), c = project ({ 1, 1, 0 }), d = project ({ 0, 1, 0 });
        const auto at = project ({ 0, 0, 1 }), bt = project ({ 1, 0, 1 }), ct = project ({ 1, 1, 1 }), dt = project ({ 0, 1, 1 });
        const auto edge = Theme::tickOff;
        if (! plan) { polygon (g, { a, b, bt, at }, juce::Colour (0xff23262d), edge); polygon (g, { a, d, dt, at }, juce::Colour (0xff1c1f25), edge); }
        polygon (g, { a, b, c, d }, juce::Colour (0xff2a2d34), edge);
        for (int i = 1; i < 10; ++i)
        {
            const float q = (float) i / 10.f;
            g.setColour (edge.withAlpha (0.55f));
            g.drawLine ({ project ({ q, 0, 0 }), project ({ q, 1, 0 }) });
            g.drawLine ({ project ({ 0, q, 0 }), project ({ 1, q, 0 }) });
        }
        if (! plan)
        {
            g.setColour (edge.brighter (0.3f));
            g.drawLine ({ bt, ct });  g.drawLine ({ ct, dt });  g.drawLine ({ c, ct });
            for (float x : { 0.18f, 0.48f, 0.78f })                     // wall panels: the more absorption, the more solid they look
                polygon (g, { project ({ x, 0, 0.25f }), project ({ x + 0.12f, 0, 0.25f }), project ({ x + 0.12f, 0, 0.78f }), project ({ x, 0, 0.78f }) },
                         Theme::amber.withAlpha (0.06f + p.absorption * 0.28f), edge);
        }

        const room::Vec source { p.sx, p.sy, p.sz / p.height }, listener { p.lx, p.ly, p.lz / p.height };
        const auto s = project (source), l = project (listener), sf = project ({ p.sx, p.sy, 0 }), lf = project ({ p.lx, p.ly, 0 });
        const room::Vec sm { p.sx * p.width, p.sy * p.depth, p.sz }, lm { p.lx * p.width, p.ly * p.depth, p.lz };

        const float dash[] = { 4.f, 5.f };
        for (const auto& tap : room::reflections (p))                   // the six first-order bounces
        {
            if (std::abs (tap.mx) + std::abs (tap.my) + std::abs (tap.mz) != 1) continue;
            const room::Vec im { room::imageCoordinate (sm.x, p.width, tap.mx), room::imageCoordinate (sm.y, p.depth, tap.my), room::imageCoordinate (sm.z, p.height, tap.mz) };
            const int axis = tap.mx ? 0 : tap.my ? 1 : 2;
            const float plane = axis == 0 ? (tap.mx < 0 ? 0.f : p.width) : axis == 1 ? (tap.my < 0 ? 0.f : p.depth) : (tap.mz < 0 ? 0.f : p.height);
            const float origin = axis == 0 ? im.x : axis == 1 ? im.y : im.z, end = axis == 0 ? lm.x : axis == 1 ? lm.y : lm.z;
            if (std::abs (end - origin) < 0.001f) continue;
            const float t = (plane - origin) / (end - origin);
            const auto point = project ({ (im.x + t * (lm.x - im.x)) / p.width, (im.y + t * (lm.y - im.y)) / p.depth, (im.z + t * (lm.z - im.z)) / p.height });
            g.setColour (Theme::amber.withAlpha (0.22f));
            g.drawDashedLine ({ s, point }, dash, 2);
            g.drawDashedLine ({ point, l }, dash, 2);
            g.fillEllipse (point.x - 3.f, point.y - 3.f, 6.f, 6.f);
        }
        const float dash2[] = { 5.f, 5.f };
        g.setColour (Theme::cream.withAlpha (0.6f));
        g.drawDashedLine ({ s, l }, dash2, 2);

        drawSubject (g, sf, p.subject);
        drawListener (g, lf);

        const bool directional = p.subject == room::Rapper || p.subject == room::Singer || p.subject == room::AcousticGuitar || p.subject == room::ElectricGuitar;
        if (directional)                                                // which way the performer faces
        {
            const float ang = p.orientation * juce::MathConstants<float>::pi / 180.f;
            const auto facing = project ({ p.sx + std::sin (ang) * 1.5f / p.width, p.sy + std::cos (ang) * 1.5f / p.depth, p.sz / p.height });
            g.setColour (Theme::amber);
            g.drawArrow ({ s, facing }, 1.5f, 6.f, 7.f);
        }

        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.setColour (Theme::amber);
        g.drawText ("SOURCE", (int) sf.x - 42, (int) sf.y + 12, 84, 14, juce::Justification::centred);
        g.setColour (Theme::cream);
        g.drawText ("LISTENER", (int) lf.x - 45, (int) lf.y + 12, 90, 14, juce::Justification::centred);
        g.setColour (Theme::text);
        g.setFont (14.f);
        g.drawText (juce::String (p.width, 1) + " x " + juce::String (p.depth, 1) + " x " + juce::String (p.height, 1) + " m", 14, 8, 300, 18, juce::Justification::left);
        g.setColour (Theme::dim);
        g.setFont (11.f);
        g.drawText ("24 wall reflections + diffuse tail.  Drag the performer or the listener.", 14, getHeight() - 22, 500, 16, juce::Justification::left);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = fx.settings();
        const auto s = project ({ p.sx, p.sy, 0 }), l = project ({ p.lx, p.ly, 0 });
        const float sd = e.position.getDistanceFrom (s.translated (0.f, -35.f)), ld = e.position.getDistanceFrom (l.translated (0.f, -20.f));
        dragging = juce::jmin (sd, ld) < 70.f ? (sd < ld ? 1 : 2) : 0;
        if (dragging == 0) return;
        dragOffset = (dragging == 1 ? s : l) - e.position;
        for (auto* id : ids()) if (auto* prm = fx.apvts.getParameter (id)) prm->beginChangeGesture();
        move (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragging != 0) move (e); }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging != 0) for (auto* id : ids()) if (auto* prm = fx.apvts.getParameter (id)) prm->endChangeGesture();
        dragging = 0;
    }

private:
    std::array<const char*, 2> ids() const { return dragging == 1 ? std::array<const char*, 2> { "room_sx", "room_sy" } : std::array<const char*, 2> { "room_lx", "room_ly" }; }

    void move (const juce::MouseEvent& e)
    {
        const auto v = unproject (e.position + dragOffset);
        const auto id = ids();
        if (auto* px = fx.apvts.getParameter (id[0])) px->setValueNotifyingHost (px->convertTo0to1 (juce::jlimit (0.05f, 0.95f, v.x)));
        if (auto* py = fx.apvts.getParameter (id[1])) py->setValueNotifyingHost (py->convertTo0to1 (juce::jlimit (0.05f, 0.95f, v.y)));
        repaint();
    }

    struct Basis { float ox, oy, a, b, c, d, z; };
    Basis basis() const
    {
        const auto p = fx.settings();
        const float w = (float) getWidth(), h = (float) getHeight();
        Basis k {};
        if (plan)
        {
            const float scale = juce::jmin (w * 0.76f / p.width, h * 0.66f / p.depth);
            k.a = p.width * scale;  k.d = p.depth * scale;
            k.ox = (w - k.a) * 0.5f;  k.oy = (h - k.d) * 0.5f;
            return k;
        }
        const float scale = juce::jmin (w * 0.84f / (p.width + 0.72f * p.depth), h * 0.74f / (0.14f * p.width + 0.20f * p.depth + 0.65f * p.height));
        k.a = p.width * scale;         k.b = -0.72f * p.depth * scale;
        k.c = 0.14f * p.width * scale; k.d = 0.20f * p.depth * scale;
        k.z = 0.65f * p.height * scale;
        k.ox = w * 0.5f - (k.a + k.b) * 0.5f;
        k.oy = h * 0.08f + k.z;
        return k;
    }
    juce::Point<float> project (room::Vec p) const { const auto k = basis(); return { k.ox + p.x * k.a + p.y * k.b, k.oy + p.x * k.c + p.y * k.d - p.z * k.z }; }
    room::Vec unproject (juce::Point<float> p) const
    {
        const auto k = basis();
        const float px = p.x - k.ox, py = p.y - k.oy, det = k.a * k.d - k.b * k.c;
        return { (px * k.d - k.b * py) / det, (k.a * py - px * k.c) / det, 0.f };
    }

    static void polygon (juce::Graphics& g, std::initializer_list<juce::Point<float>> pts, juce::Colour fill, juce::Colour edge)
    {
        juce::Path path;
        bool first = true;
        for (auto p : pts) { if (first) { path.startNewSubPath (p); first = false; } else path.lineTo (p); }
        path.closeSubPath();
        g.setColour (fill);
        g.fillPath (path);
        g.setColour (edge);
        g.strokePath (path, juce::PathStrokeType (1.f));
    }

    void drawSubject (juce::Graphics& g, juce::Point<float> anchor, int type)
    {
        const auto col = Theme::amber;
        g.setColour (juce::Colours::black.withAlpha (0.35f));           // shadow on the floor
        g.fillEllipse (anchor.x - 30.f, anchor.y - 6.f, 60.f, 14.f);
        g.setColour (col.withAlpha (0.8f));
        g.drawEllipse (anchor.x - 30.f, anchor.y - 6.f, 60.f, 14.f, 1.4f);

        if (type == room::Rapper || type == room::Singer)               // the drawn figures: feet on the floor mark
        {
            if (auto* fig = (type == room::Rapper ? rapper.get() : singer.get()))
            {
                const float h = 96.f, w = h * 0.6f;
                fig->drawWithin (g, { anchor.x - w * 0.5f, anchor.y - h + 5.f, w, h }, juce::RectanglePlacement::centred, 1.f);
            }
            return;
        }

        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (juce::AffineTransform::translation (anchor.x, anchor.y));
        const auto dark = juce::Colour (0xff17191d), light = Theme::cream;
        g.setColour (col);
        if (type == room::Drums)
        {
            g.fillEllipse (-21.f, -31.f, 42.f, 40.f);
            g.setColour (dark);  g.fillEllipse (-16.f, -26.f, 32.f, 30.f);
            g.setColour (col);   g.drawEllipse (-21.f, -31.f, 42.f, 40.f, 2.f);
            for (int side : { -1, 1 })
            {
                const float x = (float) side * 24.f;
                g.setColour (col);    g.fillRoundedRectangle (x - 11.f, -42.f, 22.f, 16.f, 3.f);
                g.setColour (light);  g.drawEllipse (x - 11.f, -45.f, 22.f, 8.f, 1.f);
                g.setColour (Theme::dim);
                g.drawLine (x * 1.5f, -52.f, x * 1.5f, 7.f, 2.f);
                g.setColour (Theme::amber.brighter (0.4f));
                g.fillEllipse (x * 1.5f - 18.f, -57.f, 36.f, 8.f);
            }
        }
        else if (type == room::Tuba)
        {
            g.drawRoundedRectangle (-18.f, -37.f, 33.f, 43.f, 13.f, 6.f);
            g.drawLine (12.f, -20.f, 12.f, -53.f, 7.f);
            polygon (g, { { 0.f, -57.f }, { 25.f, -57.f }, { 18.f, -32.f }, { 8.f, -32.f } }, col, col.darker (0.4f));
            g.setColour (col.brighter (0.5f));
            g.fillEllipse (-2.f, -62.f, 29.f, 10.f);
            g.drawLine (-7.f, -25.f, -7.f, -51.f, 4.f);
            g.drawLine (-7.f, -51.f, -22.f, -51.f, 4.f);
            for (int i = 0; i < 3; ++i) g.fillRoundedRectangle (-12.f + (float) i * 7.f, -31.f, 4.f, 19.f, 1.f);
        }
        else if (type == room::AcousticGuitar || type == room::ElectricGuitar)
        {
            g.fillEllipse (-20.f, -30.f, 35.f, 36.f);
            g.fillEllipse (-16.f, -45.f, 27.f, 28.f);
            juce::Path neck;
            neck.startNewSubPath (-2.f, -35.f);
            neck.lineTo (22.f, -72.f);
            g.setColour (col.darker (0.5f));
            g.strokePath (neck, juce::PathStrokeType (8.f));
            g.fillRoundedRectangle (18.f, -78.f, 12.f, 17.f, 3.f);
            g.setColour (dark);
            if (type == room::AcousticGuitar) g.fillEllipse (-7.f, -29.f, 9.f, 9.f); else g.fillRect (-9.f, -30.f, 13.f, 5.f);
            g.setColour (light);
            g.drawLine (-3.f, -9.f, 23.f, -70.f, 1.f);
        }
        else                                                            // piano and synth
        {
            g.setColour (juce::Colour (0xff3b3f48));
            g.fillRoundedRectangle (-40.f, -38.f, 80.f, 30.f, 5.f);
            g.setColour (col);
            g.drawRoundedRectangle (-40.f, -38.f, 80.f, 30.f, 5.f, 1.4f);
            g.drawLine (-32.f, -8.f, -32.f, 8.f, 3.f);
            g.drawLine (32.f, -8.f, 32.f, 8.f, 3.f);
            g.setColour (light);
            g.fillRect (-35.f, -26.f, 70.f, 15.f);
            g.setColour (dark);
            for (int i = 1; i < 10; ++i) g.drawVerticalLine (-35 + i * 7, -26.f, -11.f);
            for (int i : { 0, 1, 3, 4, 5, 7, 8 }) g.fillRect (-31.f + (float) i * 7.f, -26.f, 4.f, 8.f);
            if (type == room::Piano) polygon (g, { { -40.f, -40.f }, { -25.f, -65.f }, { 40.f, -45.f }, { 40.f, -38.f } }, juce::Colour (0xff2c3038), col);
        }
    }

    void drawListener (juce::Graphics& g, juce::Point<float> lf)
    {
        const auto col = Theme::cream;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (lf.x - 26.f, lf.y - 6.f, 52.f, 14.f);
        g.setColour (col.withAlpha (0.8f));
        g.drawEllipse (lf.x - 26.f, lf.y - 6.f, 52.f, 14.f, 1.4f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff737a88), lf.x - 14.f, lf.y - 30.f, juce::Colour (0xff1c1e23), lf.x + 14.f, lf.y, false));
        g.fillRoundedRectangle (lf.x - 14.f, lf.y - 28.f, 28.f, 28.f, 8.f);        // shoulders
        g.setColour (col.withAlpha (0.6f));
        g.drawRoundedRectangle (lf.x - 14.f, lf.y - 28.f, 28.f, 28.f, 8.f, 0.8f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6cf96), lf.x - 8.f, lf.y - 48.f, juce::Colour (0xffa86b2c), lf.x + 8.f, lf.y - 28.f, false));
        g.fillEllipse (lf.x - 9.f, lf.y - 48.f, 18.f, 21.f);                        // head
        juce::Path band;
        band.addCentredArc (lf.x, lf.y - 38.f, 12.f, 13.f, 0.f, -1.6f, 1.6f, true);
        g.setColour (col);
        g.strokePath (band, juce::PathStrokeType (2.6f));
        g.fillRoundedRectangle (lf.x - 14.f, lf.y - 42.f, 5.f, 12.f, 2.f);
        g.fillRoundedRectangle (lf.x + 9.f, lf.y - 42.f, 5.f, 12.f, 2.f);
    }

    RoomFx& fx;
    std::unique_ptr<juce::Drawable> rapper, singer;
    bool plan = false;
    int dragging = 0;
    juce::Point<float> dragOffset;
};

// The ROOM tab of the effects window: 104 rooms to start from on the left, the room picture, and every control as a knob.
class RoomView : public juce::Component, private juce::Timer, private juce::ListBoxModel
{
public:
    explicit RoomView (BeatStationProcessor& p) : proc (p), fx (p.roomFx), scene (p.roomFx)
    {
        static const char* titles[numKnobs] = { "WIDTH", "DEPTH", "HEIGHT", "SRC HEIGHT", "EAR HEIGHT", "FACING", "LOW CUT",
                                                "DECAY", "PRE-DELAY", "TAIL TONE", "ABSORB", "DIFFUSION", "ST WIDTH", "DUCKING",
                                                "ROOM AMT", "DIRECT", "EARLY", "LATE TAIL", "OUTPUT" };
        static const char* suffix[numKnobs] = { " m", " m", " m", " m", " m", " deg", " Hz",  " s", " ms", " Hz", "", "", "", "",  "", "", "", "", " dB" };
        for (int i = 0; i < numKnobs; ++i)
        {
            knobs[(size_t) i] = std::make_unique<LabeledKnob> (titles[i], suffix[i], i < 7 ? Theme::amber : (i < 14 ? Theme::cream : Theme::green));
            knobs[(size_t) i]->attach (p.apvts, knobIds[i]);
            addAndMakeVisible (*knobs[(size_t) i]);
        }
        for (auto* c : std::initializer_list<juce::Component*> { &scene, &onBtn, &soloBtn, &subjects, &materials, &view, &search, &category, &list })
            addAndMakeVisible (*c);

        onBtn.setClickingTogglesState (true);
        soloBtn.setClickingTogglesState (true);
        onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, RoomID::on, onBtn);
        soloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, RoomID::wetSolo, soloBtn);

        subjects.addItemList ({ "Rapper", "Singer", "Drum Kit", "Tuba", "Acoustic Guitar", "Electric Guitar", "Piano", "Synth / Keys" }, 1);
        subjectAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, RoomID::subject, subjects);
        materials.addItemList ({ "Treated / absorbent", "Soft furnishings", "Wood / lively", "Tile / reflective", "Studio / balanced" }, 1);
        materials.setTextWhenNothingSelected ("Surface shortcut");
        materials.onChange = [this] { if (materials.getSelectedId() > 0) fx.selectMaterial (materials.getSelectedId() - 1); };
        view.addItem ("3D room view", 1);
        view.addItem ("Top-down plan", 2);
        view.setSelectedId (1, juce::dontSendNotification);
        view.onChange = [this] { scene.setPlan (view.getSelectedId() == 2); };

        search.setTextToShowWhenEmpty ("Search the rooms...", Theme::dim);
        search.setColour (juce::TextEditor::backgroundColourId, Theme::graphBg);
        search.setColour (juce::TextEditor::textColourId, Theme::text);
        search.setColour (juce::TextEditor::outlineColourId, Theme::grid);
        search.onTextChange = [this] { rebuild(); };
        category.addItem ("All categories", 1);
        juce::StringArray cats;
        for (const auto& preset : room::factoryPresets()) cats.addIfNotAlreadyThere (preset.category);
        category.addItemList (cats, 2);
        category.setSelectedId (1, juce::dontSendNotification);
        category.onChange = [this] { rebuild(); };
        for (auto* box : { &subjects, &materials, &view, &category })
        {
            box->setColour (juce::ComboBox::backgroundColourId, Theme::panel);
            box->setColour (juce::ComboBox::textColourId, Theme::text);
            box->setColour (juce::ComboBox::outlineColourId, Theme::grid);
            box->setColour (juce::ComboBox::arrowColourId, Theme::amber);
        }
        list.setModel (this);
        list.setRowHeight (38);
        list.setColour (juce::ListBox::backgroundColourId, Theme::graphBg);
        list.setOutlineThickness (0);
        rebuild();
        startTimerHz (20);
    }

    ~RoomView() override { list.setModel (nullptr); }

    void setLearnCallback (std::function<void (const juce::String& target)> cb)
    {
        for (auto& k : knobs) k->onLearnMenu = [cb] (const juce::String& id) { cb ("param:" + id); };
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto left = r.removeFromLeft (212);
        r.removeFromLeft (10);
        listPanel = left;
        left = left.reduced (8);
        left.removeFromTop (20);                                        // "ROOMS" title
        search.setBounds (left.removeFromTop (26));    left.removeFromTop (4);
        category.setBounds (left.removeFromTop (26));  left.removeFromTop (6);
        infoRect = left.removeFromBottom (120);
        left.removeFromBottom (6);
        list.setBounds (left);

        auto top = r.removeFromTop (30);
        onBtn.setBounds (top.removeFromLeft (90));       top.removeFromLeft (4);
        soloBtn.setBounds (top.removeFromLeft (90));     top.removeFromLeft (12);
        subjects.setBounds (top.removeFromLeft (150));   top.removeFromLeft (6);
        materials.setBounds (top.removeFromLeft (180));  top.removeFromLeft (6);
        view.setBounds (top.removeFromLeft (140));
        r.removeFromTop (6);

        knobPanel = r.removeFromBottom (3 * 94 + 8);
        r.removeFromBottom (6);
        scene.setBounds (r);
        auto k = knobPanel.reduced (6, 4);
        const int cw = k.getWidth() / 7;
        for (int i = 0; i < numKnobs; ++i)
            knobs[(size_t) i]->setBounds (k.getX() + (i % 7) * cw, k.getY() + (i / 7) * 94, cw, 92);
    }

    void paint (juce::Graphics& g) override
    {
        for (auto rect : { listPanel, knobPanel })
        {
            g.setColour (Theme::panel);
            g.fillRoundedRectangle (rect.toFloat(), 8.f);
            g.setColour (Theme::grid.brighter (0.1f));
            g.drawRoundedRectangle (rect.toFloat().reduced (0.5f), 8.f, 1.f);
        }
        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("ROOMS   " + juce::String ((int) filtered.size()) + " of " + juce::String ((int) room::factoryPresets().size()),
                    listPanel.getX() + 10, listPanel.getY() + 6, 190, 16, juce::Justification::left);

        auto info = infoRect;
        const float wet = fx.wetLevel.load();
        g.setColour (Theme::amber);
        g.drawText (wet > 1.0e-6f ? "REVERB LEVEL  " + juce::String (juce::Decibels::gainToDecibels (wet), 1) + " dB" : juce::String ("REVERB LEVEL  silent"),
                    info.removeFromTop (16), juce::Justification::left);
        const int index = fx.selectedPreset();
        const auto& factory = room::factoryPresets();
        if (index >= 0 && index < (int) factory.size())
        {
            const auto& preset = factory[(size_t) index];
            g.setColour (Theme::text);
            g.setFont (13.f);
            g.drawFittedText (preset.name, info.removeFromTop (32), juce::Justification::topLeft, 2);
            g.setColour (Theme::dim);
            g.setFont (11.f);
            g.drawFittedText (juce::String ("Listen for: ") + preset.hint, info, juce::Justification::topLeft, 5);
        }
        else
        {
            g.setColour (Theme::dim);
            g.setFont (11.f);
            g.drawFittedText ("Pick a room to start. It switches the reverb on. Then drag the performer in the picture.", info, juce::Justification::topLeft, 5);
        }
    }

private:
    static constexpr int numKnobs = 19;
    static constexpr const char* knobIds[numKnobs] = { "room_width", "room_depth", "room_height", "room_sz", "room_lz", "room_orientation", "room_lowcut",
                                                       "room_decay", "room_predelay", "room_damping", "room_absorption", "room_diffusion", "room_spread", "room_duck",
                                                       "room_wet", "room_direct", "room_early", "room_late", "room_output" };

    int getNumRows() override { return (int) filtered.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row < 0 || row >= (int) filtered.size()) return;
        const auto& p = room::factoryPresets()[(size_t) filtered[(size_t) row]];
        if (selected)
        {
            g.setColour (Theme::amber.withAlpha (0.16f));
            g.fillRoundedRectangle (1.f, 1.f, (float) width - 2.f, (float) height - 2.f, 5.f);
            g.setColour (Theme::amber);
            g.fillRect (1, 4, 3, height - 8);
        }
        g.setColour (selected ? Theme::amber : Theme::text);
        g.setFont (12.f);
        g.drawText (p.name, 9, 2, width - 14, 18, juce::Justification::left);
        g.setColour (Theme::dim);
        g.setFont (10.f);
        g.drawText (p.category, 9, 20, width - 14, 14, juce::Justification::left);
    }
    void selectedRowsChanged (int row) override
    {
        if (rebuilding || row < 0 || row >= (int) filtered.size()) return;
        fx.loadPreset (filtered[(size_t) row]);
        materials.setSelectedId (0, juce::dontSendNotification);
        repaint();
    }

    void rebuild()
    {
        rebuilding = true;
        filtered.clear();
        const auto words = juce::StringArray::fromTokens (search.getText().trim(), " ", "");
        const auto cat = category.getText();
        const auto& presets = room::factoryPresets();
        for (int i = 0; i < (int) presets.size(); ++i)
        {
            const auto& p = presets[(size_t) i];
            const juce::String all = juce::String (p.name) + " " + p.category + " " + p.hint + " " + subjects.getItemText (p.values.subject);
            bool matches = true;
            for (const auto& w : words) if (! all.containsIgnoreCase (w)) matches = false;
            if ((category.getSelectedId() <= 1 || cat == p.category) && matches) filtered.push_back (i);
        }
        list.updateContent();
        list.deselectAllRows();
        const int sel = fx.selectedPreset();
        for (size_t row = 0; row < filtered.size(); ++row) if (filtered[row] == sel) list.selectRow ((int) row);
        rebuilding = false;
        repaint();
    }

    void timerCallback() override
    {
        if (! isShowing()) return;
        const int s = fx.settings().subject;
        knobs[5]->setEnabled (s == room::Rapper || s == room::Singer || s == room::AcousticGuitar || s == room::ElectricGuitar);   // facing only matters for these
        scene.repaint();
        repaint (infoRect);
    }

    BeatStationProcessor& proc;
    RoomFx& fx;
    RoomScene scene;
    std::array<std::unique_ptr<LabeledKnob>, numKnobs> knobs;
    juce::TextButton onBtn { "ROOM ON" }, soloBtn { "HEAR WET" };
    juce::ComboBox subjects, materials, view, category;
    juce::TextEditor search;
    juce::ListBox list;
    std::vector<int> filtered;
    bool rebuilding = false;
    juce::Rectangle<int> listPanel, knobPanel, infoRect;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt, soloAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> subjectAtt;
};
