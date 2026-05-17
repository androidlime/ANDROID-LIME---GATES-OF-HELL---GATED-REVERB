#include "EVProcessor.h"
#include "EVEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout
EVProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    using F  = juce::AudioParameterFloat;
    using B  = juce::AudioParameterBool;
    using ID = juce::ParameterID;
    using NR = juce::NormalisableRange<float>;
    using A  = juce::AudioParameterFloatAttributes;

    p.push_back(std::make_unique<F>(ID{"roomsize",1}, "Room Size",
        NR(0.f, 100.f, 0.5f), 100.f, A().withLabel("%")));
    p.push_back(std::make_unique<F>(ID{"damping",1}, "Damping",
        NR(0.f, 100.f, 0.5f),   0.f, A().withLabel("%")));
    p.push_back(std::make_unique<F>(ID{"diffuse",1}, "Diffuse",
        NR(0.f, 200.f, 0.5f),  64.f, A().withLabel("%")));

    p.push_back(std::make_unique<F>(ID{"dry",1}, "Dry",
        NR(0.f, 100.f, 0.5f), 100.f, A().withLabel("%")));
    p.push_back(std::make_unique<F>(ID{"wet",1}, "Wet",
        NR(0.f, 100.f, 0.5f),  53.5f, A().withLabel("%")));
    p.push_back(std::make_unique<F>(ID{"spread",1}, "Spread",
        NR(0.f, 200.f, 0.5f), 200.f, A().withLabel("%")));

    p.push_back(std::make_unique<F>(ID{"highcut",1}, "High Cut",
        NR(1000.f, 12000.f, 1.f, 0.4f), 12000.f, A().withLabel("Hz")));
    p.push_back(std::make_unique<F>(ID{"crossover",1}, "Crossover",
        NR(100.f, 1000.f, 1.f, 0.4f), 1000.f, A().withLabel("Hz")));
    p.push_back(std::make_unique<F>(ID{"lowfreqlevel",1}, "Low Freq Lvl",
        NR(-100.f, 12.f, 0.1f), -1.4f, A().withLabel("dB")));

    // Input: how much live audio enters the loop each cycle (0=frozen, 100=full)
    p.push_back(std::make_unique<F>(ID{"inputamt",1}, "Input",
        NR(0.f, 100.f, 0.1f, 0.4f), 100.f, A().withLabel("%")));
    p.push_back(std::make_unique<F>(ID{"holdsize",1}, "Hold Size",
        NR(1.f, 2600.f, 1.f, 0.3f), 139.f, A().withLabel("ms")));

    // Metallic resonator blend
    p.push_back(std::make_unique<F>(ID{"resonance",1}, "Resonance",
        NR(0.f, 100.f, 0.5f), 0.f, A().withLabel("%")));

    // Spring/plate modulation
    p.push_back(std::make_unique<F>(ID{"modrate",1},  "Mod Rate",
        NR(0.05f, 12.f, 0.01f, 0.4f), 2.5f, A().withLabel("Hz")));
    p.push_back(std::make_unique<F>(ID{"moddepth",1}, "Mod Depth",
        NR(0.f, 100.f, 0.5f), 20.f, A().withLabel("%")));

    // ADSR envelope — A/D/R as % of one beat, S as level %
    p.push_back(std::make_unique<F>(ID{"envattack",1},  "Attack",
        NR(0.f, 100.f, 0.5f), 100.f, A().withLabel("%")));
    p.push_back(std::make_unique<F>(ID{"envdecay",1},   "Decay",
        NR(0.f, 100.f, 0.5f), 100.f, A().withLabel("%")));
    p.push_back(std::make_unique<F>(ID{"envsustain",1}, "Sustain",
        NR(0.f, 100.f, 0.5f), 100.f, A().withLabel("%")));
    p.push_back(std::make_unique<F>(ID{"envrelease",1}, "Release",
        NR(0.f, 100.f, 0.5f),   0.f, A().withLabel("%")));

    return { p.begin(), p.end() };
}

EVProcessor::EVProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{}

EVProcessor::~EVProcessor() {}

void EVProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    sr = sampleRate;

    const float srScale = (float)(sampleRate / 44100.0);
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)samplesPerBlock, 1 };

    // Prime-spaced, shorter delays → inharmonic metallic resonances
    static const int kCombL[NCOMB] = {  521,  599,  683,  769,  839,  919,  997, 1069 };
    static const int kCombR[NCOMB] = {  544,  622,  706,  792,  862,  942, 1020, 1092 };
    for (int i = 0; i < NCOMB; ++i)
    {
        int szL = juce::jmax(4, (int)(kCombL[i] * srScale));
        int szR = juce::jmax(4, (int)(kCombR[i] * srScale));
        combL[i].setMaximumDelayInSamples(szL + 2); combL[i].prepare(spec);
        combL[i].reset(); combL[i].setDelay((float)szL);
        combR[i].setMaximumDelayInSamples(szR + 2); combR[i].prepare(spec);
        combR[i].reset(); combR[i].setDelay((float)szR);
    }
    combDampL.fill(0.f);
    combDampR.fill(0.f);

    // Shorter input diffusion → harder, more percussive metallic attack
    static const int kIapL[NIAP] = {  67,  97 };
    static const int kIapR[NIAP] = {  83, 113 };
    for (int i = 0; i < NIAP; ++i)
    {
        int szL = juce::jmax(4, (int)(kIapL[i] * srScale));
        int szR = juce::jmax(4, (int)(kIapR[i] * srScale));
        iapL[i].setMaximumDelayInSamples(szL + 2); iapL[i].prepare(spec);
        iapL[i].reset(); iapL[i].setDelay((float)szL);
        iapR[i].setMaximumDelayInSamples(szR + 2); iapR[i].prepare(spec);
        iapR[i].reset(); iapR[i].setDelay((float)szR);
    }

    // Shorter output diffusion → tighter, brighter metallic tail
    static const int kApL[NAP] = { 389, 307, 223, 149 };
    static const int kApR[NAP] = { 412, 330, 246, 172 };
    const int maxMod = 30; // max modulation headroom in samples
    for (int i = 0; i < NAP; ++i)
    {
        int szL = juce::jmax(4, (int)(kApL[i] * srScale));
        int szR = juce::jmax(4, (int)(kApR[i] * srScale));
        apBaseL[i] = (float)szL;
        apBaseR[i] = (float)szR;
        apL[i].setMaximumDelayInSamples(szL + maxMod + 2); apL[i].prepare(spec);
        apL[i].reset(); apL[i].setDelay((float)szL);
        apR[i].setMaximumDelayInSamples(szR + maxMod + 2); apR[i].prepare(spec);
        apR[i].reset(); apR[i].setDelay((float)szR);
    }
    lfoPhase = 0.f;

    // Metallic resonators: golden-ratio-spaced frequencies (inharmonic = metallic)
    static const float resoFreqs[NRESO] = { 350.f, 566.f, 915.f, 1480.f, 2394.f, 3873.f };
    const float resoQ = 8.f;
    juce::dsp::ProcessSpec resoSpec { sampleRate, (juce::uint32)samplesPerBlock, 1 };
    for (int i = 0; i < NRESO; ++i)
    {
        resoL[i].prepare(resoSpec); resoL[i].reset();
        resoR[i].prepare(resoSpec); resoR[i].reset();
        *resoL[i].coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, resoFreqs[i], resoQ);
        *resoR[i].coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, resoFreqs[i], resoQ);
    }

    hcStateL = hcStateR = xoStateL = xoStateR = 0.f;
    envLevel = 0.f;

    // Loop buffer: large enough for slow tempos (bar up to ~8 s at 30 BPM + holdSize)
    loopBufSize = (int)(sampleRate * 11.0);
    loopBufL.assign(loopBufSize, 0.f);
    loopBufR.assign(loopBufSize, 0.f);
    loopWritePos = 0;
}

void EVProcessor::releaseResources() {}

void EVProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals ndn;
    const int NS = buffer.getNumSamples();
    const int CH = juce::jmin(buffer.getNumChannels(), 2);

    if (pendingFlush.exchange(false))
    {
        for (int i = 0; i < NCOMB; ++i) { combL[i].reset(); combR[i].reset(); }
        for (int i = 0; i < NIAP;  ++i) { iapL[i].reset();  iapR[i].reset(); }
        for (int i = 0; i < NAP;   ++i) { apL[i].reset();   apR[i].reset();  }
        combDampL.fill(0.f); combDampR.fill(0.f);
        hcStateL = hcStateR = xoStateL = xoStateR = 0.f;
        envLevel = 0.f; envLevelAtomic.store(0.f); lfoPhase = 0.f;
        for (int i = 0; i < NRESO; ++i) { resoL[i].reset(); resoR[i].reset(); }
        std::fill(loopBufL.begin(), loopBufL.end(), 0.f);
        std::fill(loopBufR.begin(), loopBufR.end(), 0.f);
        loopWritePos = 0;
    }

    const float roomSize    = *apvts.getRawParameterValue("roomsize")    / 100.f;
    const float damping     = *apvts.getRawParameterValue("damping")     / 100.f;
    const float diffuse     = *apvts.getRawParameterValue("diffuse")     / 100.f;
    const float dryG        = *apvts.getRawParameterValue("dry")         / 100.f;
    const float wetG        = *apvts.getRawParameterValue("wet")         / 100.f;
    const float spreadN     = *apvts.getRawParameterValue("spread")      / 100.f;
    const float highCutHz   = *apvts.getRawParameterValue("highcut");
    const float crossoverHz = *apvts.getRawParameterValue("crossover");
    const float lowFreqGain = juce::Decibels::decibelsToGain(
                                  apvts.getRawParameterValue("lowfreqlevel")->load());
    const float inputAmount = *apvts.getRawParameterValue("inputamt") / 100.f;
    const float holdSizeMs  = *apvts.getRawParameterValue("holdsize");
    const int   loopLen     = juce::jmax(1,
        juce::jmin((int)(holdSizeMs * 0.001f * (float)sr), loopBufSize - 1));

    const float combFeedback = 0.75f + roomSize * 0.24f; // [0.75, 0.99]
    const float dampCoeff    = damping * 0.9f;
    const float dampInv      = 1.f - dampCoeff;

    const float apGain = diffuse <= 1.f
        ? diffuse * 0.7f
        : 0.7f + (diffuse - 1.f) * 0.22f;
    constexpr float iapGain = 0.5f;

    const float w1 = 0.5f + spreadN * 0.5f;
    const float w2 = (1.f - spreadN) * 0.5f;

    const float resoBlend = *apvts.getRawParameterValue("resonance") / 100.f;

    // Spring/plate LFO — modulate allpass delay times per block
    const float modRate  = *apvts.getRawParameterValue("modrate");
    const float modDepth = *apvts.getRawParameterValue("moddepth") / 100.f;
    lfoPhase = std::fmod(lfoPhase + modRate * (float)NS / (float)sr, 1.f);
    for (int i = 0; i < NAP; ++i)
    {
        // Each stage gets a different LFO phase offset (25% apart) for richness
        const float phi = lfoPhase + (float)i * 0.25f;
        const float lv  = std::sin(phi * juce::MathConstants<float>::twoPi);
        const float dev = modDepth * 24.f; // ±24 samples max deviation
        apL[i].setDelay(apBaseL[i] + lv * dev);
        apR[i].setDelay(apBaseR[i] - lv * dev); // opposite phase = stereo width
    }

    const float twoPiOverSr = juce::MathConstants<float>::twoPi / (float)sr;
    const float hcCoeff = std::exp(-twoPiOverSr * highCutHz);
    const float xoCoeff = std::exp(-twoPiOverSr * crossoverHz);

    // ADSR as fractions of one quarter-note beat (fires 4x per bar in 4/4)
    float atkF   = *apvts.getRawParameterValue("envattack")  / 100.f;
    float decF   = *apvts.getRawParameterValue("envdecay")   / 100.f;
    const float susLvl = *apvts.getRawParameterValue("envsustain") / 100.f;
    float relF   = *apvts.getRawParameterValue("envrelease") / 100.f;
    {   // Proportionally scale if A+D+R > 1 beat
        const float sum = atkF + decF + relF;
        if (sum > 1.f) { const float sc = 1.f / sum; atkF *= sc; decF *= sc; relF *= sc; }
    }

    float  bpmVal   = 120.f;
    bool   isPlaying = false;
    double beatPhase = 0.0; // 0.0–1.0 within current quarter-note beat

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            isPlaying = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpmVal = (float)*b;
            if (isPlaying)
                if (auto ppqOpt = pos->getPpqPosition())
                    beatPhase = std::fmod(*ppqOpt, 1.0);
        }
    }

    const double phaseStep = 1.0 / (60.0 / bpmVal * sr); // phase advance per sample
    // ~3ms smoothing to prevent clicks at beat boundaries
    const float envCoeff = 1.f - std::exp(-1.f / (0.003f * (float)sr));

    auto apProc = [](APLine& dl, float input, float g) -> float
    {
        float d  = dl.popSample(0);
        float yn = d - g * input;
        dl.pushSample(0, input + g * yn);
        return yn;
    };

    auto softClip = [](float x) -> float
    {
        constexpr float knee = 0.85f;
        const float ax = std::abs(x);
        if (ax < knee) return x;
        const float sign = x > 0.f ? 1.f : -1.f;
        return sign * (knee + (1.f - knee) * std::tanh((ax - knee) / (1.f - knee)));
    };

    for (int s = 0; s < NS; ++s)
    {
        const float inL = CH > 0 ? buffer.getSample(0, s) : 0.f;
        const float inR = CH > 1 ? buffer.getSample(1, s) : inL;

        // 1. Input diffusion
        float diffL = inL, diffR = inR;
        for (int i = 0; i < NIAP; ++i)
        {
            diffL = apProc(iapL[i], diffL, iapGain);
            diffR = apProc(iapR[i], diffR, iapGain);
        }

        // 2. Loop buffer: read loopLen samples behind write head,
        //    then write a blend of live input and existing loop content.
        //    inputAmount=0 → loop preserves itself; inputAmount=1 → full live input.
        const int readPos = (loopWritePos - loopLen + loopBufSize) % loopBufSize;
        const float srcL  = loopBufL[readPos];
        const float srcR  = loopBufR[readPos];
        loopBufL[loopWritePos] = diffL * inputAmount + srcL * (1.f - inputAmount);
        loopBufR[loopWritePos] = diffR * inputAmount + srcR * (1.f - inputAmount);
        loopWritePos = (loopWritePos + 1) % loopBufSize;

        // 3. Parallel comb filters with HF damping (fed by loop output or live input)
        float combSumL = 0.f, combSumR = 0.f;
        for (int i = 0; i < NCOMB; ++i)
        {
            float cL = combL[i].popSample(0);
            combDampL[i] = cL * dampInv + combDampL[i] * dampCoeff;
            combL[i].pushSample(0, srcL + combDampL[i] * combFeedback);
            combSumL += cL;

            float cR = combR[i].popSample(0);
            combDampR[i] = cR * dampInv + combDampR[i] * dampCoeff;
            combR[i].pushSample(0, srcR + combDampR[i] * combFeedback);
            combSumR += cR;
        }
        combSumL /= (float)NCOMB;
        combSumR /= (float)NCOMB;

        // 3b. Metallic resonators — excited by comb output, added back before allpass
        {
            float rL = 0.f, rR = 0.f;
            for (int i = 0; i < NRESO; ++i)
            {
                rL += resoL[i].processSample(combSumL);
                rR += resoR[i].processSample(combSumR);
            }
            combSumL += rL * resoBlend * 3.f;
            combSumR += rR * resoBlend * 3.f;
        }

        // 4. Output allpass diffusion
        float outL = combSumL, outR = combSumR;
        if (apGain > 0.001f)
        {
            for (int i = 0; i < NAP; ++i)
            {
                outL = apProc(apL[i], outL, apGain);
                outR = apProc(apR[i], outR, apGain);
            }
        }

        // 5. Tempo-synced ADSR — phase-driven, no state machine
        float targetEnv;
        if (!isPlaying)
        {
            targetEnv = 1.f;
        }
        else
        {
            const float p = (float)std::fmod(beatPhase, 1.0);
            if (atkF > 0.f && p < atkF)
                targetEnv = p / atkF;
            else if (decF > 0.f && p < atkF + decF)
                targetEnv = 1.f - ((p - atkF) / decF) * (1.f - susLvl);
            else if (p < 1.f - relF)
                targetEnv = susLvl;
            else if (relF > 0.f)
                targetEnv = susLvl * juce::jmax(0.f, 1.f - (p - (1.f - relF)) / relF);
            else
                targetEnv = 0.f;
        }
        envLevel += (targetEnv - envLevel) * envCoeff;
        const float env = envLevel;
        beatPhase = std::fmod(beatPhase + phaseStep, 1.0);

        // 6. EQ + output mix
        xoStateL = (1.f - xoCoeff) * outL + xoCoeff * xoStateL;
        xoStateR = (1.f - xoCoeff) * outR + xoCoeff * xoStateR;
        const float xL = xoStateL * lowFreqGain + (outL - xoStateL);
        const float xR = xoStateR * lowFreqGain + (outR - xoStateR);
        hcStateL = (1.f - hcCoeff) * xL + hcCoeff * hcStateL;
        hcStateR = (1.f - hcCoeff) * xR + hcCoeff * hcStateR;

        if (CH > 0) buffer.setSample(0, s, softClip(inL * dryG + (hcStateL * w1 + hcStateR * w2) * wetG * env));
        if (CH > 1) buffer.setSample(1, s, softClip(inR * dryG + (hcStateR * w1 + hcStateL * w2) * wetG * env));
    }

    envLevelAtomic.store(envLevel);
}

static constexpr int STATE_VERSION = 1;

void EVProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty("stateVersion", STATE_VERSION, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void EVProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if ((int)state.getProperty("stateVersion", 0) >= STATE_VERSION)
            apvts.replaceState(state);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EVProcessor();
}
