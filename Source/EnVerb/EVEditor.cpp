#include "EVEditor.h"
#include <BinaryData.h>

static const juce::Colour C_BG    { 0xff000000 };
static const juce::Colour C_PANEL { 0xff0d0d0d };
static const juce::Colour C_TEAL  { 0xffff3c3c };
static const juce::Colour C_RED   { 0xffff3c3c };
static const juce::Colour C_GOLD  { 0xffff3c3c };
static const juce::Colour C_GREEN { 0xffff3c3c };
static const juce::Colour C_TEXT  { 0xffcc3333 };

// ── Shared ADSR helpers ───────────────────────────────────────────────────────
static void readADSR(juce::AudioProcessorValueTreeState& apvts,
                     float& atk, float& dec, float& sus, float& rel)
{
    atk = *apvts.getRawParameterValue("envattack")  / 100.f;
    dec = *apvts.getRawParameterValue("envdecay")   / 100.f;
    sus = *apvts.getRawParameterValue("envsustain") / 100.f;
    rel = *apvts.getRawParameterValue("envrelease") / 100.f;
    const float sum = atk + dec + rel;
    if (sum > 1.f) { const float sc = 1.f / sum; atk *= sc; dec *= sc; rel *= sc; }
}

static float envAt(float t, float atk, float dec, float sus, float rel)
{
    if (t < atk)        return atk > 0.f ? t / atk : 1.f;
    if (t < atk + dec)  return dec > 0.f ? 1.f - ((t - atk) / dec) * (1.f - sus) : sus;
    if (t < 1.f - rel)  return sus;
    return rel > 0.f ? sus * juce::jmax(0.f, 1.f - (t - (1.f - rel)) / rel) : 0.f;
}

// Draws a polar ADSR wheel. cx/cy = centre, maxR = outer radius.
static void drawEnvWheel(juce::Graphics& g, float cx, float cy, float maxR,
                         float atk, float dec, float sus, float rel,
                         float curLvl, float beatPhase)
{
    const float twoPi  = juce::MathConstants<float>::twoPi;
    const float halfPi = juce::MathConstants<float>::halfPi;

    // Reference rings
    g.setColour(C_GOLD.withAlpha(0.07f));
    for (float f : { 0.33f, 0.66f, 1.f })
        g.drawEllipse(cx - maxR * f, cy - maxR * f, maxR * f * 2.f, maxR * f * 2.f, 0.5f);

    // Segment spokes
    auto spoke = [&](float t) {
        const float a = halfPi + t * twoPi;
        g.drawLine(cx, cy, cx + maxR * std::cos(a), cy + maxR * std::sin(a), 0.5f);
    };
    g.setColour(C_GOLD.withAlpha(0.13f));
    spoke(atk);  spoke(atk + dec);  spoke(1.f - rel);

    // Polar envelope shape (starts at 6 o'clock)
    const int STEPS = 180;
    juce::Path fill, outline;
    fill.startNewSubPath(cx, cy);
    outline.startNewSubPath(cx, cy);
    for (int i = 1; i <= STEPS; ++i)
    {
        const float t  = (float)i / STEPS;
        const float a  = halfPi + t * twoPi;
        const float r  = envAt(t, atk, dec, sus, rel) * maxR;
        const float px = cx + r * std::cos(a), py = cy + r * std::sin(a);
        fill.lineTo(px, py);
        outline.lineTo(px, py);
    }
    fill.closeSubPath();
    g.setColour(C_GOLD.withAlpha(0.12f));
    g.fillPath(fill);
    g.setColour(C_GOLD.withAlpha(0.85f));
    g.strokePath(outline, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));

    // Segment labels
    auto segLbl = [&](float t0, float t1, const char* lbl) {
        if (t1 <= t0) return;
        const float a  = halfPi + (t0 + t1) * 0.5f * twoPi;
        const float lr = maxR + 9.f;
        g.setFont(juce::Font(7.f));
        g.setColour(C_GOLD.withAlpha(0.55f));
        g.drawText(lbl, juce::Rectangle<float>(cx + lr * std::cos(a) - 6.f,
                                               cy + lr * std::sin(a) - 5.f, 12.f, 10.f),
                   juce::Justification::centred);
    };
    segLbl(0.f, atk, "A");
    segLbl(atk, atk + dec, "D");
    segLbl(atk + dec, 1.f - rel, "S");
    segLbl(1.f - rel, 1.f, "R");

    // 6 o'clock dot
    g.setColour(C_GOLD.withAlpha(0.45f));
    g.fillEllipse(cx - 2.f, cy + maxR - 2.f, 4.f, 4.f);

    // Animated hand
    if (beatPhase >= 0.f)
    {
        const float a  = halfPi + beatPhase * twoPi;
        const float hr = curLvl * maxR;
        const float hx = cx + hr * std::cos(a), hy = cy + hr * std::sin(a);
        g.setColour(juce::Colours::white.withAlpha(0.20f));
        g.drawLine(cx, cy, hx, hy, 4.f);
        g.setColour(juce::Colours::white.withAlpha(0.88f));
        g.drawLine(cx, cy, hx, hy, 1.5f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(hx - 3.f, hy - 3.f, 6.f, 6.f);
    }
    else if (curLvl > 0.001f)
    {
        const float r = curLvl * maxR;
        g.setColour(juce::Colours::white.withAlpha(0.28f));
        g.drawEllipse(cx - r, cy - r, r * 2.f, r * 2.f, 1.f);
    }
}

// ─── ModKnob ─────────────────────────────────────────────────────────────────

EVEditor::ModKnob::ModKnob()
    : juce::Slider(juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow)
{}

// Returns the small mod-knob centre and radius in local coords
void EVEditor::ModKnob::getModKnobGeom(float& mx, float& my, float& mr) const
{
    auto b = getLocalBounds().toFloat();
    b.removeFromBottom(15.f);
    b = b.reduced(4.f);
    const float cx = b.getCentreX(), cy = b.getCentreY();
    const float r  = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
    mx = cx;
    my = cy + r * 0.55f;
    mr = 8.f;
}

bool EVEditor::ModKnob::isOnModKnob(juce::Point<int> pt) const
{
    if (!modAmt || std::abs(modAmt->load()) < 0.0001f) return false;
    float mx, my, mr;
    getModKnobGeom(mx, my, mr);
    return pt.toFloat().getDistanceFrom({ mx, my }) < mr + 2.f;
}

void EVEditor::ModKnob::paint(juce::Graphics& g)
{
    juce::Slider::paint(g);

    auto b = getLocalBounds().toFloat();
    b.removeFromBottom(15.f);
    b = b.reduced(4.f);
    const float cx = b.getCentreX(), cy = b.getCentreY();
    const float r  = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;

    // Drop-hover highlight
    if (dropHovered)
    {
        g.setColour(modColour.withAlpha(0.25f));
        g.fillEllipse(cx - r, cy - r, r * 2.f, r * 2.f);
    }

    if (!modAmt) return;
    const float depth = modAmt->load();
    if (std::abs(depth) < 0.0001f) return;

    const bool inverted = depth < 0.f;
    const juce::Colour arcCol = inverted
        ? modColour.withMultipliedSaturation(0.55f).withAlpha(0.80f)
        : modColour.withAlpha(0.75f);

    // ── Mod arc on the main knob track ──────────────────────────────────────
    {
        const float kStart = juce::MathConstants<float>::pi * 1.2f;
        const float kEnd   = juce::MathConstants<float>::pi * 2.8f;
        const float kRange = kEnd - kStart;
        const float halfPi = juce::MathConstants<float>::halfPi;

        const float normVal  = (float)valueToProportionOfLength(getValue());
        const float valAngle = kStart + normVal * kRange;
        const float fromArc  = valAngle - halfPi;
        // Clamp sweep so it doesn't go past the physical knob limits
        const float sweep = inverted
            ? juce::jmax(depth * kRange, (kStart - valAngle))
            : juce::jmin(depth * kRange, (kEnd   - valAngle));

        if (std::abs(sweep) > 0.01f)
        {
            const float arcR = r * 0.80f;
            juce::Path arc;
            for (int i = 0; i <= 32; ++i)
            {
                const float a  = fromArc + ((float)i / 32.f) * sweep;
                const float px = cx + arcR * std::cos(a);
                const float py = cy + arcR * std::sin(a);
                if (i == 0) arc.startNewSubPath(px, py); else arc.lineTo(px, py);
            }
            g.setColour(arcCol);
            g.strokePath(arc, juce::PathStrokeType(3.f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }
    }

    // ── Small overlay mod-depth knob (Serum-style) ───────────────────────────
    float mx, my, mr;
    getModKnobGeom(mx, my, mr);

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(mx - mr - 1.f, my - mr - 1.f, (mr + 1.f) * 2.f, (mr + 1.f) * 2.f);
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillEllipse(mx - mr, my - mr, mr * 2.f, mr * 2.f);
    g.setColour(arcCol.withAlpha(0.9f));
    g.drawEllipse(mx - mr, my - mr, mr * 2.f, mr * 2.f, 1.5f);

    // Arc inside small knob (direction-aware)
    {
        const float kStart  = juce::MathConstants<float>::pi * 1.2f;
        const float kRange  = juce::MathConstants<float>::pi * 1.6f;
        const float halfPi  = juce::MathConstants<float>::halfPi;
        const float skStart = kStart - halfPi;
        const float skSweep = depth * kRange;
        const float skArcR  = mr - 2.f;
        if (skArcR > 0.f && std::abs(skSweep) > 0.01f)
        {
            juce::Path skArc;
            for (int i = 0; i <= 16; ++i)
            {
                const float a  = skStart + ((float)i / 16.f) * skSweep;
                const float px = mx + skArcR * std::cos(a);
                const float py = my + skArcR * std::sin(a);
                if (i == 0) skArc.startNewSubPath(px, py); else skArc.lineTo(px, py);
            }
            g.setColour(arcCol);
            g.strokePath(skArc, juce::PathStrokeType(2.f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
        }
    }

    // "–" indicator when inverted
    if (inverted)
    {
        g.setFont(juce::Font(9.f, juce::Font::bold));
        g.setColour(arcCol.withAlpha(1.f));
        g.drawText("-", juce::Rectangle<float>(mx - 5.f, my - 5.f, 10.f, 10.f),
                   juce::Justification::centred);
    }
}

void EVEditor::ModKnob::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(isOnModKnob(e.getPosition())
        ? juce::MouseCursor::UpDownResizeCursor
        : juce::MouseCursor::NormalCursor);
}

void EVEditor::ModKnob::mouseDown(const juce::MouseEvent& e)
{
    if (isOnModKnob(e.getPosition()))
    {
        if (e.mods.isRightButtonDown() && modAmt && modAmt->load() != 0.f)
        {
            // Right-click: invert the modulation direction
            modAmt->store(-modAmt->load());
            repaint();
            return;
        }
        inModDrag       = true;
        modDragStartY   = (float)e.getPosition().y;
        modDragStartVal = modAmt->load();
    }
    else
    {
        juce::Slider::mouseDown(e);
    }
}

void EVEditor::ModKnob::mouseDrag(const juce::MouseEvent& e)
{
    if (inModDrag && modAmt)
    {
        const float delta = (modDragStartY - (float)e.getPosition().y) / 80.f;
        modAmt->store(juce::jlimit(-1.f, 1.f, modDragStartVal + delta));
        repaint();
    }
    else
    {
        juce::Slider::mouseDrag(e);
    }
}

void EVEditor::ModKnob::mouseUp(const juce::MouseEvent& e)
{
    inModDrag = false;
    juce::Slider::mouseUp(e);
}

void EVEditor::ModKnob::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (isOnModKnob(e.getPosition()))
    {
        modAmt->store(0.f);
        repaint();
    }
    else
    {
        juce::Slider::mouseDoubleClick(e);
    }
}

bool EVEditor::ModKnob::isInterestedInDragSource(const SourceDetails& d)
{
    return d.description.toString() == "ENV";
}

void EVEditor::ModKnob::itemDragEnter(const SourceDetails&)
{
    dropHovered = true;
    repaint();
}

void EVEditor::ModKnob::itemDragExit(const SourceDetails&)
{
    dropHovered = false;
    repaint();
}

void EVEditor::ModKnob::itemDropped(const SourceDetails&)
{
    dropHovered = false;
    if (modAmt)
        modAmt->store(modAmt->load() > 0.0001f ? 0.f : 0.5f); // toggle
    repaint();
}

// ─── EnvSourceLabel ──────────────────────────────────────────────────────────

void EVEditor::EnvSourceLabel::paint(juce::Graphics& g)
{
    int count = 0;
    for (int i = 0; i < EVProcessor::NMOD; ++i)
        if (proc.modAmts[i].load() > 0.0001f) ++count;

    auto b = getLocalBounds().toFloat();

    // Background
    g.setColour(C_GOLD.withAlpha(count > 0 ? 0.18f : 0.10f));
    g.fillRoundedRectangle(b, 5.f);
    g.setColour(C_GOLD.withAlpha(count > 0 ? 0.80f : 0.35f));
    g.drawRoundedRectangle(b.reduced(0.5f), 5.f, 1.f);

    // Mini wheel (top portion)
    const float labelH = 14.f;
    const float wheelH = b.getHeight() - labelH;
    const float cx  = b.getCentreX();
    const float cy  = b.getY() + wheelH * 0.5f;
    const float maxR = juce::jmin(b.getWidth() * 0.5f, wheelH * 0.5f) - 4.f;

    if (maxR > 4.f)
    {
        float atk, dec, sus, rel;
        readADSR(proc.apvts, atk, dec, sus, rel);
        drawEnvWheel(g, cx, cy, maxR, atk, dec, sus, rel,
                     proc.envLevelAtomic.load(), proc.beatPhaseAtomic.load());
    }

    // "ENV" label at bottom
    g.setFont(juce::Font(9.f, juce::Font::bold));
    g.setColour(C_GOLD);
    auto textRow = b.removeFromBottom(labelH);
    g.drawText("ENV", textRow, juce::Justification::centred);

    // Count badge — top-right corner
    if (count > 0)
    {
        const float bd = 13.f;
        juce::Rectangle<float> badge(b.getRight() - bd - 1.f, b.getY() + 1.f, bd, bd);
        g.setColour(C_GOLD);
        g.fillEllipse(badge);
        g.setFont(juce::Font(7.f, juce::Font::bold));
        g.setColour(juce::Colours::black);
        g.drawText(juce::String(count), badge.toNearestInt(), juce::Justification::centred);
    }
}

void EVEditor::EnvSourceLabel::mouseDrag(const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 6)
        if (auto* dc = findParentComponentOfClass<juce::DragAndDropContainer>())
            if (!dc->isDragAndDropActive())
                dc->startDragging("ENV", this);
}

void EVEditor::EnvSourceLabel::mouseEnter(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}

void EVEditor::EnvSourceLabel::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

// ─── EnvDisplay ──────────────────────────────────────────────────────────────

void EVEditor::EnvDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(2.f);

    float atk, dec, sus, rel;
    readADSR(proc.apvts, atk, dec, sus, rel);

    const float curLvl    = proc.envLevelAtomic .load();
    const float beatPhase = proc.beatPhaseAtomic.load();

    if (wheelMode)
    {
        // ── Clock wheel view ──────────────────────────────────────────────
        const float maxR = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f - 5.f;
        if (maxR > 4.f)
            drawEnvWheel(g, b.getCentreX(), b.getCentreY(), maxR,
                         atk, dec, sus, rel, curLvl, beatPhase);
    }
    else
    {
        // ── Classic linear ADSR view ──────────────────────────────────────
        const float susTime = juce::jmax(0.01f, 1.f - atk - dec - rel);
        const float w = b.getWidth(), h = b.getHeight();
        const float x0 = b.getX(), y0 = b.getY();

        auto tx = [&](float t) { return x0 + t * w; };
        auto ty = [&](float lv) { return y0 + h * (1.f - lv); };

        const float xa = tx(atk), xd = tx(atk + dec);
        const float xs = tx(atk + dec + susTime), xr = tx(1.f);

        juce::Path fill;
        fill.startNewSubPath(x0, ty(0.f));
        fill.lineTo(xa, ty(1.f));
        fill.lineTo(xd, ty(sus));
        fill.lineTo(xs, ty(sus));
        fill.lineTo(xr, ty(0.f));
        fill.lineTo(xr, y0 + h);
        fill.lineTo(x0, y0 + h);
        fill.closeSubPath();
        g.setColour(C_GOLD.withAlpha(0.12f));
        g.fillPath(fill);

        juce::Path line;
        line.startNewSubPath(x0, ty(0.f));
        line.lineTo(xa, ty(1.f));
        line.lineTo(xd, ty(sus));
        line.lineTo(xs, ty(sus));
        line.lineTo(xr, ty(0.f));
        g.setColour(C_GOLD.withAlpha(0.85f));
        g.strokePath(line, juce::PathStrokeType(1.5f));

        g.setFont(juce::Font(8.f));
        g.setColour(C_GOLD.withAlpha(0.5f));
        g.drawText("A", juce::Rectangle<float>(x0, y0, xa - x0, 10.f), juce::Justification::centred);
        g.drawText("D", juce::Rectangle<float>(xa, y0, xd - xa, 10.f), juce::Justification::centred);
        g.drawText("S", juce::Rectangle<float>(xd, y0, xs - xd, 10.f), juce::Justification::centred);
        g.drawText("R", juce::Rectangle<float>(xs, y0, xr - xs, 10.f), juce::Justification::centred);

        // Level indicator line
        if (curLvl > 0.001f)
        {
            const float cy = ty(curLvl);
            g.setColour(juce::Colours::white.withAlpha(0.65f));
            g.drawLine(x0, cy, xr, cy, 1.5f);
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.drawLine(x0, cy - 1.f, xr, cy - 1.f, 2.5f);
        }
    }

    // Small mode toggle hint (bottom-right corner)
    g.setFont(juce::Font(7.f));
    g.setColour(C_GOLD.withAlpha(0.25f));
    g.drawText(wheelMode ? u8"○" : u8"●", b.reduced(2.f),
               juce::Justification::bottomRight);
}

// ─── EVEditor ────────────────────────────────────────────────────────────────

EVEditor::EVEditor(EVProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      saRoomSize  (p.apvts, "roomsize",     slRoomSize),
      saDamping   (p.apvts, "damping",      slDamping),
      saDiffuse   (p.apvts, "diffuse",      slDiffuse),
      saHoldSize  (p.apvts, "holdsize",     slHoldSize),
      saInput     (p.apvts, "inputamt",     slInput),
      saDry       (p.apvts, "dry",          slDry),
      saWet       (p.apvts, "wet",          slWet),
      saSpread    (p.apvts, "spread",       slSpread),
      saHighCut      (p.apvts, "highcut",      slHighCut),
      saCrossover    (p.apvts, "crossover",    slCrossover),
      saLowFreqLevel (p.apvts, "lowfreqlevel", slLowFreqLevel),
      saResonance    (p.apvts, "resonance",    slResonance),
      saModRate      (p.apvts, "modrate",      slModRate),
      saModDepth     (p.apvts, "moddepth",     slModDepth),
      saAttack       (p.apvts, "envattack",    slAttack),
      saDecay        (p.apvts, "envdecay",     slDecay),
      saSustain      (p.apvts, "envsustain",   slSustain),
      saRelease      (p.apvts, "envrelease",   slRelease),
      envLabel       (p),
      envDisplay     (p)
{
    knob(slRoomSize, lbRoomSize, "Room Size", C_TEAL);
    knob(slDamping,  lbDamping,  "Damping",   C_TEAL);
    knob(slDiffuse,  lbDiffuse,  "Diffuse",   C_GREEN);
    knob(slHoldSize, lbHoldSize, "Hold Size", C_RED);
    knob(slInput,    lbInput,    "Input",     C_RED);

    // Wire mod amounts (indices match kModParams in EVProcessor.cpp)
    slRoomSize    .modAmt = &p.modAmts[0];  slRoomSize    .modColour = C_TEAL;
    slDamping     .modAmt = &p.modAmts[1];  slDamping     .modColour = C_TEAL;
    slDiffuse     .modAmt = &p.modAmts[2];  slDiffuse     .modColour = C_GREEN;
    slHoldSize    .modAmt = &p.modAmts[3];  slHoldSize    .modColour = C_RED;
    slInput       .modAmt = &p.modAmts[4];  slInput       .modColour = C_RED;

    btnFlush.setColour(juce::TextButton::buttonColourId,  C_PANEL);
    btnFlush.setColour(juce::TextButton::textColourOffId, C_RED.withAlpha(0.7f));
    btnFlush.onClick = [this]() { proc.pendingFlush.store(true); };
    addAndMakeVisible(btnFlush);

    fader(slDry, lbDry, "Dry", C_TEXT);
    fader(slWet, lbWet, "Wet", C_TEAL);
    knob(slSpread, lbSpread, "Spread", C_GOLD);
    slSpread.modAmt = &p.modAmts[5]; slSpread.modColour = C_GOLD;

    knob(slHighCut,      lbHighCut,      "High Cut",     C_TEAL);
    knob(slCrossover,    lbCrossover,    "Crossover",    C_TEAL);
    knob(slLowFreqLevel, lbLowFreqLevel, "Low Freq Lvl", C_GOLD);
    slHighCut     .modAmt = &p.modAmts[6];  slHighCut     .modColour = C_TEAL;
    slCrossover   .modAmt = &p.modAmts[7];  slCrossover   .modColour = C_TEAL;
    slLowFreqLevel.modAmt = &p.modAmts[8];  slLowFreqLevel.modColour = C_GOLD;

    knob(slResonance, lbResonance, "Resonance", C_RED);
    knob(slModRate,   lbModRate,   "Mod Rate",  C_RED);
    knob(slModDepth,  lbModDepth,  "Mod Depth", C_RED);
    slResonance.modAmt = &p.modAmts[9];  slResonance.modColour = C_RED;
    slModDepth .modAmt = &p.modAmts[10]; slModDepth .modColour = C_RED;

    knob(slAttack,  lbAttack,  "Attack",  C_GOLD);
    knob(slDecay,   lbDecay,   "Decay",   C_GOLD);
    knob(slSustain, lbSustain, "Sustain", C_TEAL);
    knob(slRelease, lbRelease, "Release", C_GOLD);

    addAndMakeVisible(envLabel);
    addAndMakeVisible(envDisplay);

    setSize(1200, 450);
}

EVEditor::~EVEditor() {}

void EVEditor::knob(juce::Slider& s, juce::Label& l,
                    const juce::String& txt, juce::Colour col)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 15);
    s.setColour(juce::Slider::rotarySliderFillColourId,    col);
    s.setColour(juce::Slider::rotarySliderOutlineColourId, col.darker(0.8f));
    s.setColour(juce::Slider::thumbColourId,               col.brighter(0.4f));
    s.setColour(juce::Slider::textBoxTextColourId,         C_TEXT);
    s.setColour(juce::Slider::textBoxBackgroundColourId,   C_BG);
    s.setColour(juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    addAndMakeVisible(s);
    l.setText(txt, juce::dontSendNotification);
    l.setFont(juce::Font(10.f, juce::Font::bold));
    l.setColour(juce::Label::textColourId, col);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

void EVEditor::fader(juce::Slider& s, juce::Label& l,
                     const juce::String& txt, juce::Colour col)
{
    s.setSliderStyle(juce::Slider::LinearVertical);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 15);
    s.setColour(juce::Slider::trackColourId,             col.withAlpha(0.25f));
    s.setColour(juce::Slider::thumbColourId,             col);
    s.setColour(juce::Slider::textBoxTextColourId,       C_TEXT);
    s.setColour(juce::Slider::textBoxBackgroundColourId, C_BG);
    s.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible(s);
    l.setText(txt, juce::dontSendNotification);
    l.setFont(juce::Font(11.f, juce::Font::bold));
    l.setColour(juce::Label::textColourId, col);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

void EVEditor::paint(juce::Graphics& g)
{
    // Background image (falls back to solid black if not loaded)
    auto bg = juce::ImageCache::getFromMemory(BinaryData::background_png,
                                              BinaryData::background_pngSize);
    if (bg.isValid())
        g.drawImage(bg, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    else
        g.fillAll(C_BG);

    g.setFont(juce::Font(15.f, juce::Font::bold));
    g.setColour(C_RED);
    g.drawText("GATES OF HELL", 0, 5, getWidth(), 20, juce::Justification::centred);

    auto panel = [&](float x, float y, float w, float h, juce::Colour col)
    {
        g.setColour(col.withAlpha(0.35f));
        g.drawRoundedRectangle(x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, 6.f, 1.f);
    };

    // Row 1: Envelope | Mix | EQ  (total 1200px)
    panel(  6.f,  28.f, 640.f, 138.f, C_RED);   // Envelope
    panel(652.f,  28.f, 136.f, 138.f, C_RED);   // Mix
    panel(794.f,  28.f, 400.f, 138.f, C_RED);   // EQ

    // Row 2: ENV display | Modulation
    panel(  6.f, 170.f, 640.f, 130.f, C_RED);   // ENV pill + ADSR display
    panel(794.f, 170.f, 400.f, 130.f, C_RED);   // Modulation

    // Row 3: Delay
    panel(  6.f, 304.f,1188.f, 140.f, C_RED);   // Delay

    // Section labels — bigger font, easier to read
    auto lbl = [&](float px, float pw, float py, const char* txt) {
        g.setFont(juce::Font(11.f, juce::Font::bold));
        g.setColour(C_RED.withAlpha(0.55f));
        g.drawText(txt, (int)px, (int)py, (int)pw, 14, juce::Justification::centred);
    };
    lbl(  6.f, 640.f,  30.f, "ENVELOPE");
    lbl(652.f, 136.f,  30.f, "MIX");
    lbl(794.f, 400.f,  30.f, "EQ");
    lbl(794.f, 400.f, 172.f, "MODULATION");
    lbl(  6.f,1188.f, 306.f, "DELAY");
}

void EVEditor::resized()
{
    const int LH  = 13;
    const int KSZ = 65;

    // ── Row 1  (y=28, h=138) ─────────────────────────────────────────────────
    const int ky1 = 48;

    // ENVELOPE: x=6 w=640, 4 knobs, m=20, gap=113
    slAttack .setBounds( 26, ky1, KSZ, KSZ);  lbAttack .setBounds( 26, ky1+KSZ+2, KSZ, LH);
    slDecay  .setBounds(204, ky1, KSZ, KSZ);  lbDecay  .setBounds(204, ky1+KSZ+2, KSZ, LH);
    slSustain.setBounds(382, ky1, KSZ, KSZ);  lbSustain.setBounds(382, ky1+KSZ+2, KSZ, LH);
    slRelease.setBounds(560, ky1, KSZ, KSZ);  lbRelease.setBounds(560, ky1+KSZ+2, KSZ, LH);

    // MIX: x=652 w=136, 2 faders, m=18, gap=36
    slDry.setBounds(670, ky1, 32, 96);  lbDry.setBounds(660, ky1+96+2, 52, LH);
    slWet.setBounds(738, ky1, 32, 96);  lbWet.setBounds(728, ky1+96+2, 52, LH);

    // EQ: x=794 w=400, 3 knobs, m=20, gap=82
    slHighCut     .setBounds(814, ky1, KSZ, KSZ);  lbHighCut     .setBounds(814, ky1+KSZ+2, KSZ, LH);
    slCrossover   .setBounds(961, ky1, KSZ, KSZ);  lbCrossover   .setBounds(961, ky1+KSZ+2, KSZ, LH);
    slLowFreqLevel.setBounds(1108,ky1, KSZ, KSZ);  lbLowFreqLevel.setBounds(1108,ky1+KSZ+2, KSZ, LH);

    // ── Row 2  (y=170, h=130) ─────────────────────────────────────────────────
    envLabel  .setBounds( 10, 174,  80, 122);
    envDisplay.setBounds( 96, 174, 534, 122);
    btnFlush  .setBounds(670, 225, 100,  26);

    // MODULATION: x=794 w=400, 3 knobs
    const int ky2 = 190;
    slResonance.setBounds(814, ky2, KSZ, KSZ);  lbResonance.setBounds(814, ky2+KSZ+2, KSZ, LH);
    slModRate  .setBounds(961, ky2, KSZ, KSZ);  lbModRate  .setBounds(961, ky2+KSZ+2, KSZ, LH);
    slModDepth .setBounds(1108,ky2, KSZ, KSZ);  lbModDepth .setBounds(1108,ky2+KSZ+2, KSZ, LH);

    // ── Row 3  (y=304, h=140) ─────────────────────────────────────────────────
    // DELAY: x=6 w=1188, 6 knobs at KD=110, m=20, gap=98
    const int KD  = 110;
    const int ky3 = 310;
    slRoomSize.setBounds( 26 + 0*(KD+98), ky3, KD, KD);  lbRoomSize.setBounds( 26 + 0*(KD+98), ky3+KD+2, KD, LH);
    slDamping .setBounds( 26 + 1*(KD+98), ky3, KD, KD);  lbDamping .setBounds( 26 + 1*(KD+98), ky3+KD+2, KD, LH);
    slDiffuse .setBounds( 26 + 2*(KD+98), ky3, KD, KD);  lbDiffuse .setBounds( 26 + 2*(KD+98), ky3+KD+2, KD, LH);
    slHoldSize.setBounds( 26 + 3*(KD+98), ky3, KD, KD);  lbHoldSize.setBounds( 26 + 3*(KD+98), ky3+KD+2, KD, LH);
    slInput   .setBounds( 26 + 4*(KD+98), ky3, KD, KD);  lbInput   .setBounds( 26 + 4*(KD+98), ky3+KD+2, KD, LH);
    slSpread  .setBounds( 26 + 5*(KD+98), ky3, KD, KD);  lbSpread  .setBounds( 26 + 5*(KD+98), ky3+KD+2, KD, LH);
}

juce::AudioProcessorEditor* EVProcessor::createEditor()
{
    return new EVEditor(*this);
}
