#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>

class EVProcessor : public juce::AudioProcessor
{
public:
    EVProcessor();
    ~EVProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Android Likes EnVerb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    using CombLine = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None>;
    using APLine   = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;

    // Input diffusion allpass (2 per channel, pre-comb)
    static constexpr int NIAP = 2;
    std::array<APLine, NIAP> iapL, iapR;

    // Parallel comb filters with HF damping (8 per channel, Freeverb)
    static constexpr int NCOMB = 8;
    std::array<CombLine, NCOMB> combL, combR;
    std::array<float, NCOMB> combDampL {}, combDampR {};

    // Output allpass diffusion (4 per channel, post-comb)
    static constexpr int NAP = 4;
    std::array<APLine, NAP> apL, apR;

    // Output filter states (High Cut + Crossover)
    float hcStateL = 0.f, hcStateR = 0.f;
    float xoStateL = 0.f, xoStateR = 0.f;

    // Tempo-synced ADSR envelope (shapes wet signal)
    float envLevel = 0.f;

    // Metallic resonator bank (6 high-Q bandpass filters per channel)
    static constexpr int NRESO = 6;
    std::array<juce::dsp::IIR::Filter<float>, NRESO> resoL, resoR;

    // Spring/plate modulation LFO
    float lfoPhase = 0.f;
    // Per-allpass base delay times (set in prepareToPlay, modulated at runtime)
    float apBaseL[4] {}, apBaseR[4] {};

    // Loop buffer (freeze/blend loop)
    std::vector<float> loopBufL, loopBufR;
    int   loopBufSize  = 0;
    int   loopWritePos = 0;

    double sr = 44100.0;

public:
    std::atomic<bool>  pendingFlush    { false };
    std::atomic<float> envLevelAtomic { 0.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EVProcessor)
};
