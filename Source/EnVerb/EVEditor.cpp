#include "EVEditor.h"

static const juce::Colour C_BG    { 0xff000000 };
static const juce::Colour C_PANEL { 0xff0d0d0d };
static const juce::Colour C_TEAL  { 0xff00e5c8 };
static const juce::Colour C_RED   { 0xffff3c3c };
static const juce::Colour C_GOLD  { 0xffffa500 };
static const juce::Colour C_GREEN { 0xff39ff14 };
static const juce::Colour C_TEXT  { 0xff777777 };

EVEditor::EVEditor(EVProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      saRoomSize  (p.apvts, "roomsize",   slRoomSize),
      saDamping   (p.apvts, "damping",    slDamping),
      saDiffuse   (p.apvts, "diffuse",    slDiffuse),
      saHoldSize  (p.apvts, "holdsize",   slHoldSize),
      saInput     (p.apvts, "inputamt",   slInput),
      saDry       (p.apvts, "dry",        slDry),
      saWet       (p.apvts, "wet",        slWet),
      saSpread       (p.apvts, "spread",       slSpread),
      saHighCut      (p.apvts, "highcut",      slHighCut),
      saCrossover    (p.apvts, "crossover",    slCrossover),
      saLowFreqLevel (p.apvts, "lowfreqlevel", slLowFreqLevel),
      saResonance    (p.apvts, "resonance",    slResonance),
      saModRate      (p.apvts, "modrate",       slModRate),
      saModDepth     (p.apvts, "moddepth",     slModDepth),
      saAttack       (p.apvts, "envattack",    slAttack),
      saDecay        (p.apvts, "envdecay",     slDecay),
      saSustain      (p.apvts, "envsustain",   slSustain),
      saRelease      (p.apvts, "envrelease",   slRelease),
      envDisplay     (p)
{
    knob(slRoomSize, lbRoomSize, "Room Size", C_TEAL);
    knob(slDamping,  lbDamping,  "Damping",   C_TEAL);
    knob(slDiffuse,  lbDiffuse,  "Diffuse",   C_GREEN);
    knob(slHoldSize, lbHoldSize, "Hold Size", C_RED);
    knob(slInput,    lbInput,    "Input",     C_RED);

    // Flush: one-shot action, not a toggle
    btnFlush.setColour(juce::TextButton::buttonColourId,  C_PANEL);
    btnFlush.setColour(juce::TextButton::textColourOffId, C_RED.withAlpha(0.7f));
    btnFlush.onClick = [this]() { proc.pendingFlush.store(true); };
    addAndMakeVisible(btnFlush);

    fader(slDry,  lbDry,  "Dry", C_TEXT);
    fader(slWet,  lbWet,  "Wet", C_TEAL);
    knob (slSpread, lbSpread, "Spread", C_GOLD);

    knob(slHighCut,      lbHighCut,      "High Cut",     C_TEAL);
    knob(slCrossover,    lbCrossover,    "Crossover",    C_TEAL);
    knob(slLowFreqLevel, lbLowFreqLevel, "Low Freq Lvl", C_GOLD);

    knob(slResonance, lbResonance, "Resonance", C_RED);
    knob(slModRate,   lbModRate,   "Mod Rate",  C_RED);
    knob(slModDepth,  lbModDepth,  "Mod Depth", C_RED);

    knob(slAttack,  lbAttack,  "Attack",  C_GOLD);
    knob(slDecay,   lbDecay,   "Decay",   C_GOLD);
    knob(slSustain, lbSustain, "Sustain", C_TEAL);
    knob(slRelease, lbRelease, "Release", C_GOLD);

    addAndMakeVisible(envDisplay);

    setSize(1514, 220);
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

void EVEditor::modeBtn(juce::TextButton& b, juce::Colour col)
{
    b.setClickingTogglesState(true);
    b.setColour(juce::TextButton::buttonColourId,   C_PANEL);
    b.setColour(juce::TextButton::buttonOnColourId, col.withAlpha(0.85f));
    b.setColour(juce::TextButton::textColourOffId,  col.withAlpha(0.6f));
    b.setColour(juce::TextButton::textColourOnId,   C_BG);
    addAndMakeVisible(b);
}

void EVEditor::paint(juce::Graphics& g)
{
    g.fillAll(C_BG);

    g.setFont(juce::Font(15.f, juce::Font::bold));
    g.setColour(C_TEAL);
    g.drawText("ANDROID LIKES ENVERB", 0, 5, getWidth(), 20, juce::Justification::centred);

    auto panel = [&](float x, float y, float w, float h, juce::Colour col)
    {
        g.setColour(C_PANEL);
        g.fillRoundedRectangle(x, y, w, h, 6.f);
        g.setColour(col.withAlpha(0.18f));
        g.drawRoundedRectangle(x, y, w, h, 6.f, 1.f);
    };

    panel(  6.f, 28.f, 520.f, 184.f, C_TEAL);   // Delay knobs
    panel(532.f, 28.f, 112.f, 184.f, C_RED);     // Flush
    panel(650.f, 28.f, 244.f, 184.f, C_TEAL);   // EQ
    panel( 900.f, 28.f, 226.f, 184.f, C_RED);    // Resonance + mod
    panel(1132.f, 28.f, 280.f, 184.f, C_GOLD);   // ADSR envelope
    panel(1418.f, 28.f,  88.f, 184.f, C_TEAL);   // Mix
}

void EVEditor::resized()
{
    const int LH = 13;
    const int ky = 40;

    // 6 knobs in 520px panel: span = 6*65+5*19 = 485, start = 6+(520-485)/2 = 24
    const int KSZ = 65, sp = 84;
    const int kx1 = 24, kx2 = kx1+sp, kx3 = kx2+sp, kx4 = kx3+sp, kx5 = kx4+sp, kx6 = kx5+sp;
    slRoomSize.setBounds(kx1, ky, KSZ, KSZ);  lbRoomSize.setBounds(kx1, ky+KSZ+2, KSZ, LH);
    slDamping .setBounds(kx2, ky, KSZ, KSZ);  lbDamping .setBounds(kx2, ky+KSZ+2, KSZ, LH);
    slDiffuse .setBounds(kx3, ky, KSZ, KSZ);  lbDiffuse .setBounds(kx3, ky+KSZ+2, KSZ, LH);
    slHoldSize.setBounds(kx4, ky, KSZ, KSZ);  lbHoldSize.setBounds(kx4, ky+KSZ+2, KSZ, LH);
    slInput   .setBounds(kx5, ky, KSZ, KSZ);  lbInput   .setBounds(kx5, ky+KSZ+2, KSZ, LH);
    slSpread  .setBounds(kx6, ky, KSZ, KSZ);  lbSpread  .setBounds(kx6, ky+KSZ+2, KSZ, LH);

    // Flush centred in 112px panel at x=532
    btnFlush.setBounds(540, 107, 96, 26);

    // EQ knobs in 244px panel at x=650
    const int kx7 = 658, kx8 = 739, kx9 = 820;
    slHighCut     .setBounds(kx7, ky, KSZ, KSZ); lbHighCut     .setBounds(kx7, ky+KSZ+2, KSZ, LH);
    slCrossover   .setBounds(kx8, ky, KSZ, KSZ); lbCrossover   .setBounds(kx8, ky+KSZ+2, KSZ, LH);
    slLowFreqLevel.setBounds(kx9, ky, KSZ, KSZ); lbLowFreqLevel.setBounds(kx9, ky+KSZ+2, KSZ, LH);

    // Resonance + mod knobs in 226px panel at x=900
    const int kxm1 = 908, kxm2 = kxm1 + 76, kxm3 = kxm2 + 76;
    slResonance.setBounds(kxm1, ky, KSZ, KSZ); lbResonance.setBounds(kxm1, ky+KSZ+2, KSZ, LH);
    slModRate  .setBounds(kxm2, ky, KSZ, KSZ); lbModRate  .setBounds(kxm2, ky+KSZ+2, KSZ, LH);
    slModDepth .setBounds(kxm3, ky, KSZ, KSZ); lbModDepth .setBounds(kxm3, ky+KSZ+2, KSZ, LH);

    // ADSR knobs in 280px panel at x=1132
    const int kxa1 = 1140, kxsp = 68;
    const int kxa2 = kxa1+kxsp, kxa3 = kxa2+kxsp, kxa4 = kxa3+kxsp;
    slAttack .setBounds(kxa1, ky, KSZ, KSZ); lbAttack .setBounds(kxa1, ky+KSZ+2, KSZ, LH);
    slDecay  .setBounds(kxa2, ky, KSZ, KSZ); lbDecay  .setBounds(kxa2, ky+KSZ+2, KSZ, LH);
    slSustain.setBounds(kxa3, ky, KSZ, KSZ); lbSustain.setBounds(kxa3, ky+KSZ+2, KSZ, LH);
    slRelease.setBounds(kxa4, ky, KSZ, KSZ); lbRelease.setBounds(kxa4, ky+KSZ+2, KSZ, LH);

    // Envelope display below the ADSR knobs
    envDisplay.setBounds(1140, 124, 264, 80);

    // 2 faders in mix panel at x=1418
    slDry.setBounds(1424, 38, 38, 108);  lbDry.setBounds(1424, 148, 38, LH);
    slWet.setBounds(1468, 38, 38, 108);  lbWet.setBounds(1468, 148, 38, LH);
}

void EVEditor::EnvDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(2.f);

    g.setColour(juce::Colour(0xff0d0d0d));
    g.fillRoundedRectangle(b, 3.f);

    float atk = *proc.apvts.getRawParameterValue("envattack")  / 100.f;
    float dec = *proc.apvts.getRawParameterValue("envdecay")   / 100.f;
    const float sus = *proc.apvts.getRawParameterValue("envsustain") / 100.f;
    float rel = *proc.apvts.getRawParameterValue("envrelease") / 100.f;
    { const float sum = atk + dec + rel; if (sum > 1.f) { float sc = 1.f/sum; atk*=sc; dec*=sc; rel*=sc; } }

    const float susTime = juce::jmax(0.01f, 1.f - atk - dec - rel);
    const float total   = atk + dec + susTime + rel; // always 1.0
    if (total < 0.001f) return;

    const float w  = b.getWidth();
    const float h  = b.getHeight();
    const float x0 = b.getX();
    const float y0 = b.getY();

    auto tx = [&](float t) { return x0 + (t / total) * w; };
    auto ty = [&](float lv) { return y0 + h * (1.f - lv); };

    const float xa = tx(atk);
    const float xd = tx(atk + dec);
    const float xs = tx(atk + dec + susTime);
    const float xr = tx(total);

    // Filled shape
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

    // Outline
    juce::Path line;
    line.startNewSubPath(x0, ty(0.f));
    line.lineTo(xa, ty(1.f));
    line.lineTo(xd, ty(sus));
    line.lineTo(xs, ty(sus));
    line.lineTo(xr, ty(0.f));
    g.setColour(C_GOLD.withAlpha(0.85f));
    g.strokePath(line, juce::PathStrokeType(1.5f));

    // Segment labels
    g.setFont(juce::Font(8.f));
    g.setColour(C_GOLD.withAlpha(0.5f));
    g.drawText("A", juce::Rectangle<float>(x0, y0, xa - x0, 10.f), juce::Justification::centred);
    g.drawText("D", juce::Rectangle<float>(xa, y0, xd - xa, 10.f), juce::Justification::centred);
    g.drawText("S", juce::Rectangle<float>(xd, y0, xs - xd, 10.f), juce::Justification::centred);
    g.drawText("R", juce::Rectangle<float>(xs, y0, xr - xs, 10.f), juce::Justification::centred);

    // Current level — horizontal glow line
    const float curLvl = proc.envLevelAtomic.load();
    if (curLvl > 0.001f)
    {
        const float cy = ty(curLvl);
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawLine(x0, cy, x0 + w, cy, 1.5f);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawLine(x0, cy - 1.f, x0 + w, cy - 1.f, 2.f);
    }
}

juce::AudioProcessorEditor* EVProcessor::createEditor()
{
    return new EVEditor(*this);
}
