#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "EVProcessor.h"

class EVEditor : public juce::AudioProcessorEditor,
                 public juce::DragAndDropContainer
{
public:
    explicit EVEditor(EVProcessor&);
    ~EVEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    EVProcessor& proc;

    // Rotary knob with a Serum-style envelope mod ring.
    // Drag the EnvDisplay onto one to assign; drag the outer ring to set depth.
    class ModKnob : public juce::Slider,
                    public juce::DragAndDropTarget
    {
    public:
        ModKnob();

        void paint(juce::Graphics&) override;
        void mouseMove  (const juce::MouseEvent&) override;
        void mouseDown  (const juce::MouseEvent&) override;
        void mouseDrag  (const juce::MouseEvent&) override;
        void mouseUp    (const juce::MouseEvent&) override;
        void mouseDoubleClick(const juce::MouseEvent&) override;

        bool isInterestedInDragSource(const SourceDetails&) override;
        void itemDropped  (const SourceDetails&) override;
        void itemDragEnter(const SourceDetails&) override;
        void itemDragExit (const SourceDetails&) override;

        std::atomic<float>* modAmt = nullptr;
        juce::Colour        modColour;

    private:
        void getModKnobGeom(float& mx, float& my, float& mr) const;
        bool isOnModKnob(juce::Point<int>) const;

        bool  dropHovered   = false;
        bool  inModDrag     = false;
        float modDragStartY = 0.f;
        float modDragStartVal = 0.f;
    };

    // Modulatable params (have mod ring)
    ModKnob slRoomSize, slDamping, slDiffuse, slHoldSize, slInput;
    juce::Label lbRoomSize, lbDamping, lbDiffuse, lbHoldSize, lbInput;

    juce::TextButton btnFlush { "FLUSH" };

    juce::Slider slDry, slWet;
    juce::Label  lbDry, lbWet;

    ModKnob     slSpread;
    juce::Label lbSpread;

    ModKnob     slHighCut, slCrossover, slLowFreqLevel;
    juce::Label lbHighCut, lbCrossover, lbLowFreqLevel;

    ModKnob     slResonance, slModDepth;
    juce::Slider slModRate;
    juce::Label  lbResonance, lbModDepth, lbModRate;

    juce::Slider slAttack, slDecay, slSustain, slRelease;
    juce::Label  lbAttack, lbDecay, lbSustain, lbRelease;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    SA saRoomSize, saDamping, saDiffuse, saHoldSize, saInput, saDry, saWet, saSpread,
       saHighCut, saCrossover, saLowFreqLevel,
       saResonance, saModRate, saModDepth,
       saAttack, saDecay, saSustain, saRelease;

    // Draggable "ENV" chip — drag onto any ModKnob to assign the envelope
    class EnvSourceLabel : public juce::Component, private juce::Timer
    {
    public:
        explicit EnvSourceLabel(EVProcessor& p) : proc(p)
        {
            startTimerHz(10);
            setInterceptsMouseClicks(true, false);
        }
        ~EnvSourceLabel() override { stopTimer(); }
        void paint(juce::Graphics&) override;
        void mouseDrag  (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit  (const juce::MouseEvent&) override;
    private:
        void timerCallback() override { repaint(); }
        EVProcessor& proc;
    };

    // ADSR envelope display — click to toggle between linear and wheel view
    class EnvDisplay : public juce::Component, private juce::Timer
    {
    public:
        explicit EnvDisplay(EVProcessor& p) : proc(p)
        {
            startTimerHz(30);
            setInterceptsMouseClicks(true, false);
        }
        ~EnvDisplay() override { stopTimer(); }
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override { wheelMode = !wheelMode; repaint(); }
        bool wheelMode = false;
    private:
        void timerCallback() override { repaint(); }
        EVProcessor& proc;
    };

    EnvSourceLabel envLabel;
    EnvDisplay     envDisplay;

    void knob (juce::Slider&, juce::Label&, const juce::String&, juce::Colour);
    void fader(juce::Slider&, juce::Label&, const juce::String&, juce::Colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EVEditor)
};
