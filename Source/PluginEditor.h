#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

struct Preset
{
    juce::String name;
    float predelay, size, density, spread, highcut, lowfreq, metal, resonance;
    float attack, hold, decay, dry, wet;
    int   division;
    bool  gateOn, resetOnBeat;
};

class MGV5Editor : public juce::AudioProcessorEditor,
                   public juce::Timer
{
public:
    MGV5Editor(MGV5Processor&);
    ~MGV5Editor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    MGV5Processor& proc;

    // ── Reverb knobs ──────────────────────────────────────────────────────
    juce::Slider slPreDelay, slSize, slDensity, slSpread, slShimmer, slDryDel;
    juce::Label  lbPreDelay, lbSize, lbDensity, lbSpread, lbShimmer, lbDryDel;
    juce::Slider slHighCut, slLowFreq, slMetal, slResonance, slCrossover, slSample;
    juce::Label  lbHighCut, lbLowFreq, lbMetal, lbResonance, lbCrossover, lbSample;

    // ── Gate knobs ────────────────────────────────────────────────────────
    juce::Slider slAttack, slHold, slSustain, slDecay, slSpray, slOverlap;
    juce::Label  lbAttack, lbHold, lbSustain, lbDecay, lbSpray, lbOverlap;

    // ── Mix faders ────────────────────────────────────────────────────────
    juce::Slider slDry, slWet;
    juce::Label  lbDry, lbWet;

    // ── Controls ──────────────────────────────────────────────────────────
    juce::ComboBox  cbDivision;
    juce::Label     lbDivision;
    juce::TextButton btnGate  { "GATE"  };
    juce::TextButton btnReset { "RESET" };

    // ── Presets ───────────────────────────────────────────────────────────
    juce::ComboBox   cbPreset;
    juce::Label      lbPreset;
    juce::TextButton btnSave   { "SAVE"   };
    juce::TextButton btnDelete { "DELETE" };

    std::vector<Preset> factoryPresets;
    std::vector<Preset> userPresets;

    void buildFactory();
    void refreshPresetBox();
    void loadPreset(const Preset&);
    void applyByIndex(int comboId);
    void savePreset();
    void deletePreset();
    juce::File presetsFile();

    float meterLevel = 0.f;

    // ── Attachments ───────────────────────────────────────────────────────
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    SA saPreDelay, saSize, saDensity, saSpread, saShimmer, saDryDel;
    SA saHighCut,  saLowFreq, saMetal, saResonance, saCrossover, saSample;
    SA saAttack,   saHold,   saSustain, saDecay, saSpray, saOverlap;
    SA saDry,      saWet;
    CA caDivision;
    BA baGate, baReset;

    void knob(juce::Slider&, juce::Label&, const juce::String&, juce::Colour);
    void fader(juce::Slider&, juce::Label&, const juce::String&, juce::Colour);
    void styleButton(juce::TextButton&, juce::Colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MGV5Editor)
};
