#include "PluginEditor.h"

static const juce::Colour C_BG    { 0xff0d1117 };
static const juce::Colour C_PANEL { 0xff161b22 };
static const juce::Colour C_TEAL  { 0xff00e5c8 };
static const juce::Colour C_PURP  { 0xffb060ff };
static const juce::Colour C_ORAN  { 0xffff8c00 };
static const juce::Colour C_GREEN { 0xff39ff14 };
static const juce::Colour C_GOLD  { 0xffffd700 };
static const juce::Colour C_TEXT  { 0xffaabbcc };

// ── Factory presets ───────────────────────────────────────────────────────
void MGV5Editor::buildFactory()
{
    //           name                 pdly size dens sprd hicut lfreq metal  res  atk  hld  dcy  dry  wet  div  gate  rst
    factoryPresets.push_back({"Classic 80s Gate", 10, 50,  50, 100, 5000,  0,  70, 1.0f, 10, 200, 150, 50,  80,  2, true, true });
    factoryPresets.push_back({"Kick Metallic",     5, 40,  20,  90, 8000,  0,  90, 1.1f,  3,  80, 100, 50,  70,  2, true, true });
    factoryPresets.push_back({"Tight Snap",        0, 25,  10,  80, 9000,-10,  85, 1.2f,  2,  50,  60, 50,  60,  2, true, true });
    factoryPresets.push_back({"Wide Shimmer",     20, 70, 100, 100, 4000,  5,  70, 0.9f, 20, 300, 200, 40,  90,  2, true,false });
    factoryPresets.push_back({"Dark Hall",        30, 80,  80,  70, 2500,  8,  40, 0.8f, 40, 400, 300, 50,  70,  1, true,false });
    factoryPresets.push_back({"8th Note Chop",     5, 35,  30,  85, 7000,  0,  80, 1.0f,  5,  80,  80, 50,  75,  3, true, true });
    factoryPresets.push_back({"No Gate",          15, 60,  60, 100, 6000,  0,  60, 1.0f,  0,   0,   0, 50,  80,  2,false,false });
    factoryPresets.push_back({"Snare Room",        8, 45,  40,  95, 6500, -5,  65, 1.0f,  8, 150, 120, 50,  65,  2, true, true });
    factoryPresets.push_back({"Industrial Ring",   0, 55,  15,  80,10000, -8,  95, 1.3f,  2, 120,  80, 40,  85,  2, true, true });
    factoryPresets.push_back({"Slow Gate",        25, 65,  70, 100, 4500,  3,  55, 0.9f, 80, 500, 400, 50,  75,  0, true,false });
    factoryPresets.push_back({"High Pitch Ring",   5, 35,  10,  85,10000,  0,  95, 0.6f,  2,  60,  80, 50,  75,  2, true, true });
    factoryPresets.push_back({"Low Pitch Metal",  10, 60,  30,  80, 6000,  0,  80, 1.8f,  5, 100, 120, 50,  70,  2, true, true });
}

juce::File MGV5Editor::presetsFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("MetallicGateV5");
    dir.createDirectory();
    return dir.getChildFile("UserPresets.xml");
}

void MGV5Editor::refreshPresetBox()
{
    cbPreset.clear(juce::dontSendNotification);
    cbPreset.addSectionHeading("-- Factory --");
    int id = 1;
    for (auto& p : factoryPresets) cbPreset.addItem(p.name, id++);
    if (!userPresets.empty())
    {
        cbPreset.addSeparator();
        cbPreset.addSectionHeading("-- My Presets --");
        for (auto& p : userPresets) cbPreset.addItem(p.name, id++);
    }
}

void MGV5Editor::loadPreset(const Preset& p)
{
    auto setF = [&](const char* id, float v)
    {
        if (auto* pr = proc.apvts.getParameter(id))
            pr->setValueNotifyingHost(pr->convertTo0to1(v));
    };
    auto setB = [&](const char* id, bool v)
    {
        if (auto* pr = proc.apvts.getParameter(id))
            pr->setValueNotifyingHost(v ? 1.f : 0.f);
    };
    auto setI = [&](const char* id, int v)
    {
        if (auto* pr = proc.apvts.getParameter(id))
            pr->setValueNotifyingHost(pr->convertTo0to1((float)v));
    };

    setF("predelay",     p.predelay);
    setF("size",         p.size);
    setF("density",      p.density);
    setF("spread",       p.spread);
    setF("highcut",      p.highcut);
    setF("lowfreq",      p.lowfreq);
    setF("metal",        p.metal);
    setF("resonance",    p.resonance);
    setF("attack",       p.attack);
    setF("hold",         p.hold);
    setF("decay",        p.decay);
    setF("dry",          p.dry);
    setF("wet",          p.wet);
    setI("division",     p.division);
    setB("gate_on",      p.gateOn);
    setB("reset_on_beat",p.resetOnBeat);
}

void MGV5Editor::applyByIndex(int id)
{
    int idx = id - 1;
    if (idx < (int)factoryPresets.size())
        loadPreset(factoryPresets[idx]);
    else
    {
        int ui = idx - (int)factoryPresets.size();
        if (ui >= 0 && ui < (int)userPresets.size())
            loadPreset(userPresets[ui]);
    }
}

void MGV5Editor::savePreset()
{
    auto* dlg = new juce::AlertWindow("Save Preset",
        "Enter a name:", juce::AlertWindow::NoIcon);
    dlg->addTextEditor("name","My Preset","Name:");
    dlg->addButton("Save",1);
    dlg->addButton("Cancel",0);
    juce::Component::SafePointer<MGV5Editor> safeThis(this);
    dlg->enterModalState(true,
        juce::ModalCallbackFunction::create([safeThis,dlg](int r)
        {
            if (r == 1 && safeThis != nullptr)
            {
                juce::String name = dlg->getTextEditorContents("name").trim();
                if (name.isEmpty()) name = "My Preset";
                auto g = [&safeThis](const char* id){ return safeThis->proc.apvts.getRawParameterValue(id)->load(); };
                Preset p;
                p.name=name; p.predelay=g("predelay"); p.size=g("size");
                p.density=g("density"); p.spread=g("spread");
                p.highcut=g("highcut"); p.lowfreq=g("lowfreq");
                p.metal=g("metal"); p.resonance=g("resonance");
                p.attack=g("attack"); p.hold=g("hold"); p.decay=g("decay");
                p.dry=g("dry"); p.wet=g("wet");
                p.division=(int)g("division");
                p.gateOn=g("gate_on")>0.5f;
                p.resetOnBeat=g("reset_on_beat")>0.5f;

                bool found=false;
                for (auto& e:safeThis->userPresets) if(e.name==name){e=p;found=true;break;}
                if (!found) safeThis->userPresets.push_back(p);

                // Write XML
                auto xml=std::make_unique<juce::XmlElement>("UserPresets");
                for (auto& u:safeThis->userPresets)
                {
                    auto* e=xml->createNewChildElement("Preset");
                    e->setAttribute("name",u.name);
                    e->setAttribute("predelay",u.predelay); e->setAttribute("size",u.size);
                    e->setAttribute("density",u.density);   e->setAttribute("spread",u.spread);
                    e->setAttribute("highcut",u.highcut);   e->setAttribute("lowfreq",u.lowfreq);
                    e->setAttribute("metal",u.metal);       e->setAttribute("resonance",u.resonance);
                    e->setAttribute("attack",u.attack);     e->setAttribute("hold",u.hold);
                    e->setAttribute("decay",u.decay);       e->setAttribute("dry",u.dry);
                    e->setAttribute("wet",u.wet);           e->setAttribute("division",u.division);
                    e->setAttribute("gateOn",u.gateOn?1:0); e->setAttribute("resetOnBeat",u.resetOnBeat?1:0);
                }
                xml->writeTo(safeThis->presetsFile());
                safeThis->refreshPresetBox();
            }
            delete dlg;
        }), true);
}

void MGV5Editor::deletePreset()
{
    int id  = cbPreset.getSelectedId();
    int idx = id - 1 - (int)factoryPresets.size();
    if (idx < 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon, "Cannot Delete",
            "Factory presets cannot be deleted.");
        return;
    }
    if (idx >= (int)userPresets.size()) return;
    userPresets.erase(userPresets.begin()+idx);

    auto xml=std::make_unique<juce::XmlElement>("UserPresets");
    for (auto& u:userPresets)
    {
        auto* e=xml->createNewChildElement("Preset");
        e->setAttribute("name",u.name); e->setAttribute("predelay",u.predelay);
        e->setAttribute("size",u.size); e->setAttribute("density",u.density);
        e->setAttribute("spread",u.spread); e->setAttribute("highcut",u.highcut);
        e->setAttribute("lowfreq",u.lowfreq); e->setAttribute("metal",u.metal);
        e->setAttribute("resonance",u.resonance); e->setAttribute("attack",u.attack);
        e->setAttribute("hold",u.hold); e->setAttribute("decay",u.decay);
        e->setAttribute("dry",u.dry); e->setAttribute("wet",u.wet);
        e->setAttribute("division",u.division);
        e->setAttribute("gateOn",u.gateOn?1:0);
        e->setAttribute("resetOnBeat",u.resetOnBeat?1:0);
    }
    xml->writeTo(presetsFile());
    refreshPresetBox();
}

// ── Constructor ────────────────────────────────────────────────────────────
MGV5Editor::MGV5Editor(MGV5Processor& p)
    : AudioProcessorEditor(&p), proc(p),
      saPreDelay(p.apvts,"predelay",  slPreDelay),
      saSize     (p.apvts,"size",      slSize),
      saDensity  (p.apvts,"density",   slDensity),
      saSpread   (p.apvts,"spread",    slSpread),
      saShimmer  (p.apvts,"shimmer",   slShimmer),
      saDryDel   (p.apvts,"drydel",   slDryDel),
      saHighCut  (p.apvts,"highcut",   slHighCut),
      saLowFreq  (p.apvts,"lowfreq",   slLowFreq),
      saMetal    (p.apvts,"metal",     slMetal),
      saResonance(p.apvts,"resonance", slResonance),
      saCrossover(p.apvts,"rate",      slCrossover),
      saSample   (p.apvts,"sample",    slSample),
      saAttack   (p.apvts,"attack",    slAttack),
      saHold     (p.apvts,"hold",      slHold),
      saSustain  (p.apvts,"sustain",   slSustain),
      saDecay    (p.apvts,"decay",     slDecay),
      saSpray    (p.apvts,"position",   slSpray),
      saOverlap  (p.apvts,"overlap",   slOverlap),
      saDry      (p.apvts,"dry",       slDry),
      saWet      (p.apvts,"wet",       slWet),
      caDivision (p.apvts,"division",  cbDivision),
      baGate     (p.apvts,"gate_on",       btnGate),
      baReset    (p.apvts,"reset_on_beat", btnReset)
{
    // Reverb row 1 — space
    knob(slPreDelay, lbPreDelay, "Pre-Delay", C_TEAL);
    knob(slSize,     lbSize,     "Rev Time",  C_TEAL);
    knob(slDensity,  lbDensity,  "Scatter",   C_TEAL);
    knob(slSpread,   lbSpread,   "Spread",    C_TEAL);
    knob(slShimmer,  lbShimmer,  "Shimmer",   C_TEAL);
    knob(slDryDel,   lbDryDel,   "Dry Delay", C_TEAL);

    // Reverb row 2 — character
    knob(slHighCut,   lbHighCut,   "High Cut",  C_PURP);
    knob(slLowFreq,   lbLowFreq,   "Low Freq",  C_PURP);
    knob(slMetal,     lbMetal,     "Metallic",  C_PURP);
    knob(slResonance, lbResonance, "Resonance", C_PURP);
    knob(slCrossover, lbCrossover, "Rate",      C_PURP);
    knob(slSample,    lbSample,    "Sample",    C_PURP);

    // Gate knobs
    knob(slAttack,  lbAttack,  "Attack",   C_ORAN);
    knob(slHold,    lbHold,    "Hold",     C_ORAN);
    knob(slSustain, lbSustain, "Sustain",  C_ORAN);
    knob(slDecay,   lbDecay,   "Release",  C_ORAN);
    knob(slSpray,   lbSpray,   "Position", C_ORAN);
    knob(slOverlap, lbOverlap, "Overlap",  C_ORAN);

    // Mix faders
    fader(slDry, lbDry, "Dry", C_TEXT);
    fader(slWet, lbWet, "Wet", C_GREEN);

    // Division
    cbDivision.addItem("1/1  — 1x per bar",  1);
    cbDivision.addItem("1/2  — 2x per bar",  2);
    cbDivision.addItem("1/4  — 4x per bar",  3);
    cbDivision.addItem("1/8  — 8x per bar",  4);
    cbDivision.addItem("1/16 — 16x per bar", 5);
    cbDivision.setSelectedId(3);
    cbDivision.setColour(juce::ComboBox::backgroundColourId, C_PANEL);
    cbDivision.setColour(juce::ComboBox::textColourId,       C_GREEN);
    cbDivision.setColour(juce::ComboBox::outlineColourId,    C_GREEN.withAlpha(0.4f));
    cbDivision.setColour(juce::ComboBox::arrowColourId,      C_GREEN);
    addAndMakeVisible(cbDivision);

    lbDivision.setText("BPM Division", juce::dontSendNotification);
    lbDivision.setFont(juce::Font(10.f,juce::Font::bold));
    lbDivision.setColour(juce::Label::textColourId, C_GREEN);
    lbDivision.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lbDivision);

    styleButton(btnGate,  C_GREEN);
    styleButton(btnReset, C_TEAL);
    btnGate.setClickingTogglesState(true);
    btnReset.setClickingTogglesState(true);
    btnReset.onClick = [this]() { proc.pendingFlush.store(true); };

    // Presets
    buildFactory();

    // Load user presets
    if (presetsFile().existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse(presetsFile()))
        {
            for (auto* e : xml->getChildIterator())
            {
                Preset u;
                u.name       = e->getStringAttribute("name");
                u.predelay   = (float)e->getDoubleAttribute("predelay");
                u.size       = (float)e->getDoubleAttribute("size");
                u.density    = (float)e->getDoubleAttribute("density");
                u.spread     = (float)e->getDoubleAttribute("spread");
                u.highcut    = (float)e->getDoubleAttribute("highcut");
                u.lowfreq    = (float)e->getDoubleAttribute("lowfreq");
                u.metal      = (float)e->getDoubleAttribute("metal");
                u.resonance  = (float)e->getDoubleAttribute("resonance", 1.0);
                u.attack     = (float)e->getDoubleAttribute("attack");
                u.hold       = (float)e->getDoubleAttribute("hold");
                u.decay      = (float)e->getDoubleAttribute("decay");
                u.dry        = (float)e->getDoubleAttribute("dry");
                u.wet        = (float)e->getDoubleAttribute("wet");
                u.division   = e->getIntAttribute("division");
                u.gateOn     = e->getIntAttribute("gateOn") != 0;
                u.resetOnBeat= e->getIntAttribute("resetOnBeat") != 0;
                userPresets.push_back(u);
            }
        }
    }

    refreshPresetBox();
    cbPreset.setColour(juce::ComboBox::backgroundColourId, C_PANEL);
    cbPreset.setColour(juce::ComboBox::textColourId,       C_GOLD);
    cbPreset.setColour(juce::ComboBox::outlineColourId,    C_GOLD.withAlpha(0.4f));
    cbPreset.setColour(juce::ComboBox::arrowColourId,      C_GOLD);
    cbPreset.onChange = [this](){ applyByIndex(cbPreset.getSelectedId()); };
    addAndMakeVisible(cbPreset);

    lbPreset.setText("PRESETS", juce::dontSendNotification);
    lbPreset.setFont(juce::Font(11.f,juce::Font::bold));
    lbPreset.setColour(juce::Label::textColourId, C_GOLD);
    lbPreset.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(lbPreset);

    btnSave.setColour(juce::TextButton::buttonColourId,  C_PANEL);
    btnSave.setColour(juce::TextButton::textColourOffId, C_GOLD);
    btnSave.onClick = [this](){ savePreset(); };
    addAndMakeVisible(btnSave);

    btnDelete.setColour(juce::TextButton::buttonColourId,  C_PANEL);
    btnDelete.setColour(juce::TextButton::textColourOffId, juce::Colours::tomato);
    btnDelete.onClick = [this](){ deletePreset(); };
    addAndMakeVisible(btnDelete);

    setSize(860, 500);
    startTimerHz(30);
}

MGV5Editor::~MGV5Editor() { stopTimer(); }

void MGV5Editor::knob(juce::Slider& s, juce::Label& l,
                       const juce::String& txt, juce::Colour col)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,74,16);
    s.setColour(juce::Slider::rotarySliderFillColourId,    col);
    s.setColour(juce::Slider::rotarySliderOutlineColourId, col.darker(0.7f));
    s.setColour(juce::Slider::thumbColourId,               col.brighter(0.3f));
    s.setColour(juce::Slider::textBoxTextColourId,         C_TEXT);
    s.setColour(juce::Slider::textBoxBackgroundColourId,   C_BG);
    s.setColour(juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    addAndMakeVisible(s);
    l.setText(txt,juce::dontSendNotification);
    l.setFont(juce::Font(10.f,juce::Font::bold));
    l.setColour(juce::Label::textColourId,col);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

void MGV5Editor::fader(juce::Slider& s, juce::Label& l,
                        const juce::String& txt, juce::Colour col)
{
    s.setSliderStyle(juce::Slider::LinearVertical);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,50,16);
    s.setColour(juce::Slider::trackColourId,             col.withAlpha(0.3f));
    s.setColour(juce::Slider::thumbColourId,             col);
    s.setColour(juce::Slider::textBoxTextColourId,       C_TEXT);
    s.setColour(juce::Slider::textBoxBackgroundColourId, C_BG);
    s.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible(s);
    l.setText(txt,juce::dontSendNotification);
    l.setFont(juce::Font(11.f,juce::Font::bold));
    l.setColour(juce::Label::textColourId,col);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

void MGV5Editor::styleButton(juce::TextButton& b, juce::Colour col)
{
    b.setColour(juce::TextButton::buttonColourId,   C_PANEL);
    b.setColour(juce::TextButton::buttonOnColourId, col.withAlpha(0.55f));
    b.setColour(juce::TextButton::textColourOffId,  col.withAlpha(0.5f));
    b.setColour(juce::TextButton::textColourOnId,   C_BG);
    addAndMakeVisible(b);
}

void MGV5Editor::timerCallback()
{
    meterLevel = proc.currentGateLevel.load();
    repaint();
}

void MGV5Editor::paint(juce::Graphics& g)
{
    g.fillAll(C_BG);

    auto panel = [&](float x,float y,float w,float h,
                     juce::Colour col,const juce::String& title)
    {
        g.setColour(C_PANEL);
        g.fillRoundedRectangle(x,y,w,h,7.f);
        g.setColour(col.withAlpha(0.22f));
        g.drawRoundedRectangle(x,y,w,h,7.f,1.f);
        g.setFont(juce::Font(9.f,juce::Font::bold));
        g.setColour(col.withAlpha(0.55f));
        g.drawText(title,(int)x,(int)y+2,(int)w,14,juce::Justification::centred);
    };

    panel(8,  36, 640, 130, C_TEAL, "REVERB — SPACE");
    panel(8, 176, 640, 130, C_PURP, "REVERB — CHARACTER");
    panel(8, 316, 640, 114, C_ORAN, "GATE ENVELOPE");
    panel(8, 440,  640, 48, C_GOLD, "PRESETS");
    panel(658, 36, 192, 452, C_GREEN, "MIX");

    // Title
    g.setFont(juce::Font(21.f,juce::Font::bold));
    g.setColour(C_TEAL);
    g.drawText("METALLIC  GATE  V4", 0, 6, getWidth(), 24,
               juce::Justification::centred);

    // Gate meter
    juce::Rectangle<float> meter(666.f, 362.f, 176.f, 16.f);
    g.setColour(C_BG); g.fillRoundedRectangle(meter,3.f);
    g.setColour(C_GREEN.withAlpha(0.15f)); g.drawRoundedRectangle(meter,3.f,1.f);
    if (meterLevel > 0.001f)
    {
        juce::ColourGradient gr(C_GREEN,meter.getX(),0,C_TEAL,meter.getRight(),0,false);
        g.setGradientFill(gr);
        g.fillRoundedRectangle(meter.getX(),meter.getY(),
                               meter.getWidth()*meterLevel,meter.getHeight(),3.f);
    }
    g.setFont(juce::Font(8.f,juce::Font::bold));
    g.setColour(meterLevel>0.05f ? C_BG : C_GREEN.withAlpha(0.4f));
    g.drawText("GATE",meter.toNearestInt(),juce::Justification::centred);

    // Envelope shape
    juce::Rectangle<float> env(666.f,386.f,176.f,48.f);
    g.setColour(C_BG); g.fillRoundedRectangle(env,4.f);
    g.setColour(C_ORAN.withAlpha(0.15f)); g.drawRoundedRectangle(env,4.f,1.f);

    float atk=*proc.apvts.getRawParameterValue("attack");
    float hld=*proc.apvts.getRawParameterValue("hold");
    float dcy=*proc.apvts.getRawParameterValue("decay");
    float sust=*proc.apvts.getRawParameterValue("sustain")/100.f;
    float tot=atk+hld+dcy;
    if (tot>0.f)
    {
        float x0=env.getX()+5.f, bY=env.getBottom()-4.f,
              tY=env.getY()+4.f, w=env.getWidth()-10.f;
        float sY = bY - (bY - tY) * sust;  // sustain resting level
        float ax=x0+(atk/tot)*w, hx=ax+(hld/tot)*w, ex=hx+(dcy/tot)*w;
        juce::Path path;
        path.startNewSubPath(x0, sY);           // start at sustain floor
        path.lineTo(ax, tY);                    // attack to top
        path.lineTo(hx, tY);                    // hold at top
        path.lineTo(ex, sY);                    // decay back to sustain floor
        g.setColour(C_ORAN.withAlpha(0.9f));
        g.strokePath(path,juce::PathStrokeType(1.5f));
        path.lineTo(x0, sY); path.closeSubPath();
        g.setColour(C_ORAN.withAlpha(0.1f));
        g.fillPath(path);
    }
}

void MGV5Editor::resized()
{
    const int kSz=74, lH=14, sp=82;
    auto pk=[&](juce::Slider& s, juce::Label& l, int x, int y)
    { s.setBounds(x,y,kSz,kSz); l.setBounds(x,y+kSz,kSz,lH); };

    // Row 1 — space: predelay, size, density, spread, shimmer
    int ry=52, rx=16;
    pk(slPreDelay, lbPreDelay, rx,      ry);
    pk(slSize,     lbSize,     rx+sp,   ry);
    pk(slDensity,  lbDensity,  rx+sp*2, ry);
    pk(slSpread,   lbSpread,   rx+sp*3, ry);
    pk(slShimmer,  lbShimmer,  rx+sp*4, ry);
    pk(slDryDel,   lbDryDel,   rx+sp*5, ry);

    // Row 2 — character: highcut, lowfreq, metal, resonance, crossover
    int r2y=192;
    pk(slHighCut,   lbHighCut,   rx,      r2y);
    pk(slLowFreq,   lbLowFreq,   rx+sp,   r2y);
    pk(slMetal,     lbMetal,     rx+sp*2, r2y);
    pk(slResonance, lbResonance, rx+sp*3, r2y);
    pk(slCrossover, lbCrossover, rx+sp*4, r2y);
    pk(slSample,    lbSample,    rx+sp*5, r2y);

    // Gate row: attack, hold, sustain, release, spray, overlap + buttons + division
    int gy=332;
    pk(slAttack,  lbAttack,  rx,      gy);
    pk(slHold,    lbHold,    rx+sp,   gy);
    pk(slSustain, lbSustain, rx+sp*2, gy);
    pk(slDecay,   lbDecay,   rx+sp*3, gy);
    pk(slSpray,   lbSpray,   rx+sp*4, gy);
    pk(slOverlap, lbOverlap, rx+sp*5, gy);
    // Buttons and division fit in the remaining space (x≈506 to 640)
    btnGate.setBounds (506, gy+10, 62, 24);
    btnReset.setBounds(574, gy+10, 62, 24);
    lbDivision.setBounds(506, gy+40, 130, 14);
    cbDivision.setBounds(506, gy+56, 130, 22);

    // Preset bar
    lbPreset.setBounds  (16,  452,  70,  24);
    cbPreset.setBounds  (90,  452, 350,  26);
    btnSave.setBounds   (448, 452,  60,  26);
    btnDelete.setBounds (514, 452,  72,  26);

    // Mix faders
    slDry.setBounds(672,  55, 50, 200); lbDry.setBounds(672,258,50,14);
    slWet.setBounds(798,  55, 50, 200); lbWet.setBounds(798,258,50,14);
}

juce::AudioProcessorEditor* MGV5Processor::createEditor()
{
    return new MGV5Editor(*this);
}
