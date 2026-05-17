#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class MGV5Processor : public juce::AudioProcessor
{
public:
    MGV5Processor();
    ~MGV5Processor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Metallic Gate V4"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> currentGateLevel { 0.0f };
    std::atomic<bool>  pendingFlush     { false };

    void flushReverb();

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── Plate reverb (Dattorro-inspired) ──────────────────────────────────
    // NA=4 allpass lines per channel are repurposed:
    //   [0,1] = input diffusion stages
    //   [2,3] = tank allpass (AP1 modulated by LFO, AP2 fixed)
    // Two cross-coupled tank loops complete the late reverb.
    static constexpr int NA = 4;   // allpass per channel
    static constexpr int NE = 6;   // early reflections

    // Input diffusion times [0,1] and tank allpass times [2,3] (ms)
    const std::array<float,NA> apL { 4.77f, 12.73f, 22.58f, 60.48f };
    const std::array<float,NA> apR { 3.60f,  9.31f, 30.51f, 89.24f };

    // Early reflection times (ms) and gains
    const std::array<float,NE> erL { 7.1f,14.3f,21.7f,30.2f,39.8f,51.4f };
    const std::array<float,NE> erR { 8.3f,16.7f,24.1f,33.6f,43.2f,55.8f };
    const std::array<float,NE> erG { 0.80f,0.60f,0.45f,0.34f,0.25f,0.18f };

    std::array<juce::dsp::DelayLine<float>,NA> allpL, allpR;
    std::array<juce::dsp::DelayLine<float>,NE> earlyL, earlyR;

    // Tank main and secondary delays (L and R are the two cross-coupled loops)
    juce::dsp::DelayLine<float> tankD1L, tankD1R;  // AP1 → D1 → LP
    juce::dsp::DelayLine<float> tankD2L, tankD2R;  // AP2 → D2

    float plateLpL = 0.f, plateLpR = 0.f;  // LP filter state per tank
    float plateFbL = 0.f, plateFbR = 0.f;  // cross-coupled feedback state

    juce::dsp::DelayLine<float> pdL, pdR;       // wet pre-delay
    juce::dsp::DelayLine<float> pdDryL, pdDryR; // dry signal delay

    // ── Gate ──────────────────────────────────────────────────────────────
    enum Phase { IDLE, ATTACK, HOLD, DECAY };
    Phase  phase      = IDLE;
    float  gateLevel  = 0.f;
    float  phTimer    = 0.f;
    float  beatTimer  = 0.f;
    float  beatPeriod = 0.f;
    bool   wasPlaying = false;

    void   triggerGate(bool flush, float sustainFloor = 0.f);

    float  lfL = 0.f, lfR = 0.f;   // low-freq tilt filter state
    float  xfL = 0.f, xfR = 0.f;   // crossover LP state
    float  lfoPhase = 0.f;          // LFO phase for shimmer modulation

    // ── Granular input engine ─────────────────────────────────────────────
    static constexpr int MAX_GRAINS = 16;
    // readPos advances by 'rate' per sample — fractional for pitch shift.
    // phase counts real samples elapsed (wall-clock grain length).
    struct GrainVoice { float readPos = 0.f; float phase = 0.f; bool active = false; };
    GrainVoice grains[MAX_GRAINS];
    float voiceTimers[8] = {};   // per-spray-voice spawn timers

    std::vector<float> ringBufL, ringBufR;
    int ringWritePos = 0;
    int ringBufSize  = 0;
    juce::Random grainRng;

    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MGV5Processor)
};
