#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "EVProcessor.h"

class EVEditor : public juce::AudioProcessorEditor
{
public:
    explicit EVEditor(EVProcessor&);
    ~EVEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    EVProcessor& proc;

    juce::Slider     slRoomSize, slDamping, slDiffuse, slHoldSize, slInput;
    juce::Label      lbRoomSize, lbDamping, lbDiffuse, lbHoldSize, lbInput;

    juce::TextButton btnFlush { "FLUSH" };

    juce::Slider slDry, slWet;
    juce::Label  lbDry, lbWet;

    juce::Slider slSpread;
    juce::Label  lbSpread;

    juce::Slider slHighCut, slCrossover, slLowFreqLevel;
    juce::Label  lbHighCut, lbCrossover, lbLowFreqLevel;

    juce::Slider slModRate, slModDepth, slResonance;
    juce::Label  lbModRate, lbModDepth, lbResonance;

    juce::Slider slAttack, slDecay, slSustain, slRelease;
    juce::Label  lbAttack, lbDecay, lbSustain, lbRelease;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    SA saRoomSize, saDamping, saDiffuse, saHoldSize, saInput, saDry, saWet, saSpread,
       saHighCut, saCrossover, saLowFreqLevel,
       saResonance, saModRate, saModDepth,
       saAttack, saDecay, saSustain, saRelease;

    class EnvDisplay : public juce::Component, private juce::Timer
    {
    public:
        explicit EnvDisplay(EVProcessor& p) : proc(p) { startTimerHz(30); }
        ~EnvDisplay() override { stopTimer(); }
        void paint(juce::Graphics&) override;
    private:
        void timerCallback() override { repaint(); }
        EVProcessor& proc;
    };

    EnvDisplay envDisplay;

    void knob (juce::Slider&, juce::Label&, const juce::String&, juce::Colour);
    void fader(juce::Slider&, juce::Label&, const juce::String&, juce::Colour);
    void modeBtn(juce::TextButton&, juce::Colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EVEditor)
};
