#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout
MGV5Processor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    using F = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using C = juce::AudioParameterChoice;
    using ID = juce::ParameterID;
    using NR = juce::NormalisableRange<float>;
    using A  = juce::AudioParameterFloatAttributes;

    // ── REVERB ────────────────────────────────────────────────────────────
    p.push_back(std::make_unique<F>(ID{"predelay",1}, "Pre-Delay",
        NR(0.f,1000.f,0.5f,0.4f), 0.f, A().withLabel("ms")));

    p.push_back(std::make_unique<F>(ID{"size",1}, "Size",
        NR(0.f,100.f,0.5f), 99.f, A().withLabel("%")));

    p.push_back(std::make_unique<F>(ID{"density",1}, "Scatter",
        NR(0.f,100.f,0.5f), 0.f, A().withLabel("%")));

    p.push_back(std::make_unique<F>(ID{"spread",1}, "Spread",
        NR(0.f,200.f,0.5f), 200.f, A().withLabel("%")));

    p.push_back(std::make_unique<F>(ID{"highcut",1}, "High Cut",
        NR(1000.f,12000.f,1.f,0.4f), 12000.f, A().withLabel("Hz")));

    p.push_back(std::make_unique<F>(ID{"lowfreq",1}, "Low Freq",
        NR(-24.f,12.f,0.5f), 12.f, A().withLabel("dB")));

    p.push_back(std::make_unique<F>(ID{"metal",1}, "Metallic",
        NR(0.f,100.f,0.5f), 0.f, A().withLabel("%")));

    // Resonance: 0.5 = tighter/less resonant, 2.0 = more resonant/ringy
    p.push_back(std::make_unique<F>(ID{"resonance",1}, "Resonance",
        NR(0.5f,2.0f,0.01f), 2.0f));

    // Rate: read-head speed. 1.0 = normal, <1 = pitch down, >1 = pitch up.
    p.push_back(std::make_unique<F>(ID{"rate",1}, "Rate",
        NR(0.25f,4.0f,0.01f,0.5f), 1.0f, A().withLabel("x")));
    // keep crossover in state for compatibility but it has no audio effect in V4
    p.push_back(std::make_unique<F>(ID{"crossover",1}, "Crossover",
        NR(80.f,4000.f,0.f,0.4f), 80.f, A().withLabel("Hz")));

    p.push_back(std::make_unique<F>(ID{"shimmer",1}, "Shimmer",
        NR(0.f,100.f,0.5f), 0.f, A().withLabel("%")));

    // Grain length: how many ms each grain plays before a hard cut.
    p.push_back(std::make_unique<F>(ID{"sample",1}, "Sample",
        NR(5.f,2000.f,0.5f,0.4f), 100.f, A().withLabel("ms")));

    p.push_back(std::make_unique<F>(ID{"spray",1}, "Spray",
        NR(1.f,8.f,1.f), 1.f));

    // Position: how far back in input history the grain reads from.
    // 0ms = most recent audio (just behind write head); higher = older material.
    p.push_back(std::make_unique<F>(ID{"position",1}, "Position",
        NR(0.f,2000.f,0.5f,0.4f), 0.f, A().withLabel("ms")));

    // Overlap: how much consecutive grains within a voice overlap.
    // 0% = back-to-back hard cuts. 100% = new grain starts immediately (dense).
    p.push_back(std::make_unique<F>(ID{"overlap",1}, "Overlap",
        NR(0.f,100.f,0.5f), 0.f, A().withLabel("%")));

    // ── GATE ──────────────────────────────────────────────────────────────
    p.push_back(std::make_unique<F>(ID{"attack",1}, "Attack",
        NR(0.f,500.f,0.5f,0.4f), 335.f, A().withLabel("ms")));

    p.push_back(std::make_unique<F>(ID{"hold",1}, "Hold",
        NR(0.f,1000.f,0.5f,0.4f), 247.5f, A().withLabel("ms")));

    p.push_back(std::make_unique<F>(ID{"sustain",1}, "Sustain",
        NR(0.f,100.f,0.5f), 71.f, A().withLabel("%")));

    p.push_back(std::make_unique<F>(ID{"decay",1}, "Release",
        NR(0.f,5000.f,0.5f,0.4f), 10.f, A().withLabel("ms")));

    p.push_back(std::make_unique<C>(ID{"division",1}, "Division",
        juce::StringArray{"1/1","1/2","1/4","1/8","1/16"}, 2));

    p.push_back(std::make_unique<B>(ID{"gate_on",1},      "Gate On",      true));
    p.push_back(std::make_unique<B>(ID{"reset_on_beat",1},"Reset On Beat",false));

    // ── MIX ───────────────────────────────────────────────────────────────
    p.push_back(std::make_unique<F>(ID{"drydel",1}, "Dry Delay",
        NR(0.f,500.f,0.5f,0.4f), 0.f, A().withLabel("ms")));

    p.push_back(std::make_unique<F>(ID{"dry",1}, "Dry",
        NR(0.f,100.f,0.5f), 100.f, A().withLabel("%")));

    p.push_back(std::make_unique<F>(ID{"wet",1}, "Wet",
        NR(0.f,100.f,0.5f), 100.f, A().withLabel("%")));

    return { p.begin(), p.end() };
}

MGV5Processor::MGV5Processor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{}

MGV5Processor::~MGV5Processor() {}

void MGV5Processor::flushReverb()
{
    for (int i=0;i<NA;++i) { allpL[i].reset(); allpR[i].reset(); }
    for (int i=0;i<NE;++i) { earlyL[i].reset(); earlyR[i].reset(); }
    tankD1L.reset(); tankD1R.reset();
    tankD2L.reset(); tankD2R.reset();
    plateLpL = 0.f; plateLpR = 0.f;
    plateFbL = 0.f; plateFbR = 0.f;
    std::fill(ringBufL.begin(), ringBufL.end(), 0.f);
    std::fill(ringBufR.begin(), ringBufR.end(), 0.f);
    for (auto& g : grains) g.active = false;
    for (int i = 0; i < 8; ++i) voiceTimers[i] = 0.f;
}

void MGV5Processor::triggerGate(bool flush, float sustainFloor)
{
    phase     = ATTACK;
    phTimer   = 0.f;
    gateLevel = sustainFloor;
    if (flush) flushReverb();
}

void MGV5Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)samplesPerBlock, 1 };

    auto prep = [&](juce::dsp::DelayLine<float>& d, int maxMs)
    {
        d.setMaximumDelayInSamples((int)(sampleRate * maxMs / 1000.0) + 4);
        d.prepare(spec); d.reset();
    };

    prep(pdL,1200); prep(pdR,1200);
    prep(pdDryL,600); prep(pdDryR,600);

    // Input diffusion allpass [0,1]
    prep(allpL[0], 10);  prep(allpR[0], 10);
    prep(allpL[1], 20);  prep(allpR[1], 15);
    // Tank allpass [2,3] — AP1 needs LFO headroom, AP2L/R based on Dattorro times
    prep(allpL[2], 35);  prep(allpR[2], 45);
    prep(allpL[3], 70);  prep(allpR[3], 100);

    // Tank main delays: base ~35ms, up to 4× at density=400 → allocate 160ms
    prep(tankD1L, 160); prep(tankD1R, 160);
    prep(tankD2L, 140); prep(tankD2R, 140);

    for (int i=0;i<NE;++i) { prep(earlyL[i],200); prep(earlyR[i],200); }

    plateLpL = 0.f; plateLpR = 0.f;
    plateFbL = 0.f; plateFbR = 0.f;
    lfL = 0.f; lfR = 0.f;
    xfL = 0.f; xfR = 0.f;
    lfoPhase = 0.f;

    ringBufSize = (int)(sampleRate * 3.0) + 4;
    ringBufL.assign(ringBufSize, 0.f);
    ringBufR.assign(ringBufSize, 0.f);
    ringWritePos = 0;
    for (auto& g : grains) g.active = false;
    for (int i = 0; i < 8; ++i) voiceTimers[i] = 0.f;

    phase=IDLE; gateLevel=0.f; beatTimer=0.f; phTimer=0.f;
    wasPlaying=false;
}

void MGV5Processor::releaseResources() {}

void MGV5Processor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer&)
{
    juce::ScopedNoDenormals ndn;
    const int NS = buffer.getNumSamples();
    const int CH = juce::jmin(buffer.getNumChannels(), 2);

    // ── Read parameters ───────────────────────────────────────────────────
    float size       = *apvts.getRawParameterValue("size")    / 100.f;
    float scatterPct = *apvts.getRawParameterValue("density") / 100.f;
    float spreadRaw  = *apvts.getRawParameterValue("spread")  / 100.f;
    float spread     = juce::jmin(spreadRaw, 1.f);
    float spreadWidth = spreadRaw;
    float hiCut   = *apvts.getRawParameterValue("highcut");
    float loFreq  = *apvts.getRawParameterValue("lowfreq");
    float metal   = *apvts.getRawParameterValue("metal")   / 100.f;
    float res     = *apvts.getRawParameterValue("resonance");
    float atkMs   = *apvts.getRawParameterValue("attack");
    float hldMs   = *apvts.getRawParameterValue("hold");
    float sustainLvl = *apvts.getRawParameterValue("sustain") / 100.f;
    float dcyMs   = *apvts.getRawParameterValue("decay");
    int   div     = (int)*apvts.getRawParameterValue("division");
    bool  gateOn  = *apvts.getRawParameterValue("gate_on")       > 0.5f;
    bool  doReset = *apvts.getRawParameterValue("reset_on_beat")  > 0.5f;
    float dryDel  = *apvts.getRawParameterValue("drydel");
    float dryG    = *apvts.getRawParameterValue("dry") / 100.f;
    float wetG    = *apvts.getRawParameterValue("wet") / 100.f;

    int   grainLenS    = juce::jlimit(1, ringBufSize / 2,
                           (int)(*apvts.getRawParameterValue("sample") / 1000.f * (float)sr));
    float overlapFrac  = *apvts.getRawParameterValue("overlap") / 100.f;
    float grainPeriodS = juce::jmax(1.f, (float)grainLenS * (1.f - overlapFrac));
    float readRate     = juce::jlimit(0.01f, 8.f, apvts.getRawParameterValue("rate")->load());
    float positionMs   = *apvts.getRawParameterValue("position");
    int   positionS    = juce::jlimit(0, ringBufSize / 2,
                           (int)(positionMs / 1000.f * (float)sr));

    if (pendingFlush.exchange(false))
    {
        flushReverb();
        for (int i = 0; i < 8; ++i)
            voiceTimers[i] = grainPeriodS - 1.f;
    }

    // ── Host transport ────────────────────────────────────────────────────
    double bpm       = 120.0;
    bool   playing   = false;
    double ppqPos    = 0.0;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm().hasValue())         bpm    = *pos->getBpm();
            playing = pos->getIsPlaying();
            if (pos->getPpqPosition().hasValue()) ppqPos = *pos->getPpqPosition();
        }
    }

    if (!playing && wasPlaying)
    {
        beatTimer = 0.f;
        if (phase != IDLE) { phase = DECAY; phTimer = 0.f; }
    }
    wasPlaying = playing;

    const float dm[]  = { 4.f,2.f,1.f,0.5f,0.25f };
    beatPeriod = (float)((60.0/bpm) * sr * dm[div]);

    if (playing && beatPeriod > 0.f)
    {
        double ppqPerGate  = dm[div];
        double posInCycle  = std::fmod(ppqPos, ppqPerGate);
        float  syncTimer   = (float)(posInCycle / ppqPerGate) * beatPeriod;
        if (std::abs(syncTimer - beatTimer) > (float)(sr * 0.005))
            beatTimer = syncTimer;
    }

    float atkS = (atkMs/1000.f)*(float)sr;
    float hldS = (hldMs/1000.f)*(float)sr;
    float dcyS = (dcyMs/1000.f)*(float)sr;

    // ── Per-block audio parameters ────────────────────────────────────────
    float hiCutNorm = hiCut / 12000.f;
    float damp = juce::jlimit(0.f, 0.95f,
        ((1.f - hiCutNorm) * 0.85f + 0.1f) * (1.f - metal * 0.7f));

    float fbGain = juce::jlimit(0.f, 0.98f,
        size * (0.90f + (res - 0.5f) * 0.047f));

    // Scatter range: Scatter knob + metal both increase position randomness
    int grainScatter = juce::jlimit(1, ringBufSize / 2,
        1 + (int)((float)grainLenS * (scatterPct * 3.f + metal * 2.f)));

    float dryDelSamples = juce::jmax(1.f, (dryDel/1000.f)*(float)sr);
    pdDryL.setDelay(juce::jmin(dryDelSamples,(float)(pdDryL.getMaximumDelayInSamples()-1)));
    pdDryR.setDelay(juce::jmin(dryDelSamples,(float)(pdDryR.getMaximumDelayInSamples()-1)));

    float loGain = juce::Decibels::decibelsToGain(juce::jmax(-24.f, loFreq));

    for (int s = 0; s < NS; ++s)
    {
        // ── Beat clock + gate trigger ─────────────────────────────────────
        if (gateOn && playing)
        {
            beatTimer += 1.f;
            if (beatTimer >= beatPeriod)
            {
                beatTimer -= beatPeriod;
                triggerGate(doReset, sustainLvl);
            }
        }
        else if (!gateOn)
        {
            gateLevel = 1.f;
            phase     = IDLE;
        }

        // ── Gate envelope ─────────────────────────────────────────────────
        if (gateOn && playing)
        {
            phTimer += 1.f;
            switch (phase)
            {
                case IDLE:
                    gateLevel = sustainLvl;
                    break;

                case ATTACK:
                {
                    // Ramp from sustainLvl to 1.0 — Sustain sets the floor so it
                    // works even when Attack is longer than the gate period.
                    float prog = atkS > 0.f ? juce::jlimit(0.f,1.f,phTimer/atkS) : 1.f;
                    gateLevel = sustainLvl + (1.f - sustainLvl) * prog;
                    if (phTimer >= atkS) { gateLevel=1.f; phase=HOLD; phTimer=0.f; }
                    break;
                }

                case HOLD:
                    gateLevel = 1.f;
                    if (phTimer >= hldS) { phase=DECAY; phTimer=0.f; }
                    break;

                case DECAY:
                {
                    float t = dcyS > 0.f ? juce::jlimit(0.f,1.f,phTimer/dcyS) : 1.f;
                    gateLevel = 1.f - (1.f - sustainLvl) * (1.f - std::exp(-4.5f * t));
                    if (phTimer >= dcyS) { gateLevel=sustainLvl; phase=IDLE; phTimer=0.f; }
                    break;
                }
            }
        }

        // ── Input samples ─────────────────────────────────────────────────
        float inL = CH > 0 ? buffer.getSample(0,s) : 0.f;
        float inR = CH > 1 ? buffer.getSample(1,s) : inL;

        // ── Ring buffer write (input + feedback) ──────────────────────────
        // Soft clip via tanh prevents hard saturation while keeping the sharp
        // digital character — plateLpL/R is LP-filtered grain output (feedback).
        ringBufL[ringWritePos] = std::tanh(inL + plateLpL * fbGain);
        ringBufR[ringWritePos] = std::tanh(inR + plateLpR * fbGain);
        ringWritePos = (ringWritePos + 1) % ringBufSize;

        // ── Spawn grain (single voice) ────────────────────────────────────
        // Position sets how far back in history to read; Scatter adds jitter.
        voiceTimers[0] += 1.f;
        if (voiceTimers[0] >= grainPeriodS)
        {
            voiceTimers[0] -= grainPeriodS;
            int slot = 0, oldestPhase = -1;
            for (int i = 0; i < MAX_GRAINS; ++i) {
                if (!grains[i].active) { slot = i; oldestPhase = -2; break; }
                if (grains[i].phase > oldestPhase) { oldestPhase = (int)grains[i].phase; slot = i; }
            }
            int minBack = (int)((float)grainLenS * juce::jmax(1.f, readRate)) + grainLenS;
            int jitter  = grainScatter > 1 ? grainRng.nextInt(grainScatter) : 0;
            grains[slot].readPos = (float)((ringWritePos - minBack - positionS - jitter + ringBufSize * 8) % ringBufSize);
            grains[slot].phase   = 0.f;
            grains[slot].active  = true;
        }

        // ── Read grain outputs (hard cut — no interpolation, no windowing) ──
        // readPos advances by readRate each sample: <1 = pitch down, >1 = pitch up.
        // Integer truncation (no interpolation) keeps the hard digital character.
        float gL = 0.f, gR = 0.f;
        int nActive = 0;
        for (auto& g : grains)
        {
            if (!g.active) continue;
            int p = ((int)g.readPos % ringBufSize + ringBufSize) % ringBufSize;
            gL += ringBufL[p];
            gR += ringBufR[p];
            ++nActive;
            g.readPos += readRate;
            if (g.readPos >= (float)ringBufSize) g.readPos -= (float)ringBufSize;
            g.phase += 1.f;
            if (g.phase >= (float)grainLenS) g.active = false;
        }
        float grainNorm = nActive > 0 ? 1.f / (float)nActive : 0.f;
        gL *= grainNorm;
        gR *= grainNorm;

        // ── Granular output → feedback → output ───────────────────────────
        float gMono = (gL + gR) * 0.5f;
        float aOutL = gMono*(1.f-spread) + gL*spread;
        float aOutR = gMono*(1.f-spread) + gR*spread;

        // LP-filter the grain output into the feedback state.
        // damp is derived from hiCut + metal — controls brightness of the tail.
        plateLpL = aOutL * (1.f - damp) + plateLpL * damp;
        plateLpR = aOutR * (1.f - damp) + plateLpR * damp;

        // ── Low freq tilt ─────────────────────────────────────────────────
        lfL = aOutL*0.015f + lfL*0.985f;
        lfR = aOutR*0.015f + lfR*0.985f;
        aOutL += lfL * (loGain - 1.f);
        aOutR += lfR * (loGain - 1.f);

        // ── Stereo width (mid-side) ───────────────────────────────────────
        float mid  = (aOutL + aOutR) * 0.5f;
        float side = (aOutL - aOutR) * 0.5f * spreadWidth;
        float wetL = (mid + side) * gateLevel;
        float wetR = (mid - side) * gateLevel;

        // ── Dry signal delay then output ──────────────────────────────────
        pdDryL.pushSample(0, inL);  pdDryR.pushSample(0, inR);
        float dryOutL = pdDryL.popSample(0);
        float dryOutR = pdDryR.popSample(0);
        if (CH > 0) buffer.setSample(0, s, dryOutL*dryG + wetL*wetG);
        if (CH > 1) buffer.setSample(1, s, dryOutR*dryG + wetR*wetG);
    }

    currentGateLevel.store(gateLevel);
}

void MGV5Processor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void MGV5Processor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MGV5Processor();
}
