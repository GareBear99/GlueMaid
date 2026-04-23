#include "PluginProcessor.h"
#include "PluginEditor.h"

GlueMaidAudioProcessor::GlueMaidAudioProcessor()
: AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                 .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "PARAMS", createParams())
{
    for (auto* id : { IDs::glue, IDs::punch, IDs::soft, IDs::scHp, IDs::mix, IDs::output, IDs::clip })
        apvts.addParameterListener(id, this);
}

GlueMaidAudioProcessor::~GlueMaidAudioProcessor()
{
    for (auto* id : { IDs::glue, IDs::punch, IDs::soft, IDs::scHp, IDs::mix, IDs::output, IDs::clip })
        apvts.removeParameterListener (id, this);
}

const juce::String GlueMaidAudioProcessor::getName() const { return "GlueMaid"; }
bool GlueMaidAudioProcessor::acceptsMidi() const { return false; }
bool GlueMaidAudioProcessor::producesMidi() const { return false; }
bool GlueMaidAudioProcessor::isMidiEffect() const { return false; }
double GlueMaidAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int GlueMaidAudioProcessor::getNumPrograms() { return 1; }
int GlueMaidAudioProcessor::getCurrentProgram() { return 0; }
void GlueMaidAudioProcessor::setCurrentProgram (int) {}
const juce::String GlueMaidAudioProcessor::getProgramName (int) { return {}; }
void GlueMaidAudioProcessor::changeProgramName (int, const juce::String&) {}

bool GlueMaidAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* GlueMaidAudioProcessor::createEditor() { return new GlueMaidAudioProcessorEditor (*this); }

bool GlueMaidAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getChannelSet(true, 0)  == juce::AudioChannelSet::stereo()
        && layouts.getChannelSet(false, 0) == juce::AudioChannelSet::stereo();
}

juce::AudioProcessorValueTreeState::ParameterLayout GlueMaidAudioProcessor::createParams()
{
    using P = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<P>(IDs::glue,   "Glue",   juce::NormalisableRange<float>(0.f, 100.f, 0.01f), 40.f));
    layout.add(std::make_unique<B>(IDs::punch,  "Punch",  true));
    layout.add(std::make_unique<B>(IDs::soft,   "Soft",   false));
    layout.add(std::make_unique<P>(IDs::scHp,   "SC HP",  juce::NormalisableRange<float>(20.f, 250.f, 0.01f, 0.35f), 90.f));
    layout.add(std::make_unique<P>(IDs::mix,    "Mix",    juce::NormalisableRange<float>(0.f, 100.f, 0.01f), 100.f));
    layout.add(std::make_unique<P>(IDs::output, "Output", juce::NormalisableRange<float>(-24.f, 24.f, 0.01f), 0.f));
    layout.add(std::make_unique<B>(IDs::clip,   "Clip",   true));

    return layout;
}

void GlueMaidAudioProcessor::parameterChanged (const juce::String&, float)
{
    dirty.store(true, std::memory_order_release);
}

void GlueMaidAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = (float) sampleRate;
    const int maxBlock = std::max(64, samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels = 2;

    for (int ch=0; ch<2; ++ch)
    {
        scHP[ch].reset();
        scHP[ch].prepare(spec);
    }

    outGain.reset();
    outGain.prepare(spec);
    outGain.setRampDurationSeconds(0.02);

    dryCopy.setSize(2, maxBlock, false, false, true);
    wetBuf.setSize(2, maxBlock, false, false, true);

    mixSm.reset(sr, 0.02);
    outSm.reset(sr, 0.02);
    glueSm.reset(sr, 0.05);

    env = 0.0f;
    gainSmooth = 1.0f;

    dirty.store(true, std::memory_order_release);
}

void GlueMaidAudioProcessor::releaseResources() {}

void GlueMaidAudioProcessor::analyzeBlock(const juce::AudioBuffer<float>& buffer)
{
    double sumSq = 0.0;
    float peak = 0.0f;
    const int n = buffer.getNumSamples();

    for (int ch=0; ch<2; ++ch)
    {
        const float* x = buffer.getReadPointer(ch);
        for (int i=0; i<n; ++i)
        {
            const float v = x[i];
            sumSq += (double)v * (double)v;
            peak = std::max(peak, std::abs(v));
        }
    }

    inRms = std::sqrt((float)(sumSq / (double)(n * 2)));
    inCrest = peak / std::max(inRms, 1.0e-6f);

    meters.inRms.store(inRms, std::memory_order_relaxed);
    meters.crest.store(inCrest, std::memory_order_relaxed);
}

void GlueMaidAudioProcessor::updateTargetsIfNeeded()
{
    const float glue01 = clamp01(apvts.getRawParameterValue(IDs::glue)->load() / 100.0f);
    const bool punch   = apvts.getRawParameterValue(IDs::punch)->load() > 0.5f;
    const bool soft    = apvts.getRawParameterValue(IDs::soft)->load() > 0.5f;

    glueSm.setTargetValue(glue01);

    // Transientness proxy from crest
    const float t = clamp01(juce::jmap(inCrest, 1.2f, 8.0f, 0.0f, 1.0f));
    const float sustain = 1.0f - t;

    targetThreshDb = -8.0f - 22.0f * glue01 - 4.0f * sustain;

    targetRatio = 1.4f + 2.6f * glue01;
    if (soft) targetRatio *= 0.75f;
    targetRatio = juce::jlimit(1.2f, 6.0f, targetRatio);

    targetKneeDb = soft ? (10.0f + 6.0f * glue01) : (4.0f + 4.0f * glue01);

    const float atkBase = soft ? 16.0f : 10.0f;
    const float relBase = soft ? 220.0f : 140.0f;

    atkMs = atkBase + 22.0f * sustain + (punch ? (14.0f + 10.0f * t) : (0.0f + 4.0f * sustain));
    relMs = relBase + 260.0f * sustain + 60.0f * glue01;

    atkMs = juce::jlimit(2.0f, 80.0f, atkMs);
    relMs = juce::jlimit(40.0f, 900.0f, relMs);

    if (dirty.exchange(false, std::memory_order_acq_rel))
    {
        float scHpHz = apvts.getRawParameterValue(IDs::scHp)->load();
        // Punch bias: protect low-end by raising detector HP a bit
        if (punch) scHpHz = juce::jlimit(20.0f, 250.0f, scHpHz + 30.0f);

        auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, scHpHz, 0.707f);
        for (int ch=0; ch<2; ++ch)
            *scHP[ch].coefficients = *c;
    }
}

void GlueMaidAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    if (n <= 0) return;

    dryCopy.makeCopyOf(buffer, true);

    analyzeBlock(dryCopy);
    updateTargetsIfNeeded();

    const float mix01 = clamp01(apvts.getRawParameterValue(IDs::mix)->load() / 100.0f);
    const float outDb = apvts.getRawParameterValue(IDs::output)->load();
    const bool clipOn = apvts.getRawParameterValue(IDs::clip)->load() > 0.5f;

    mixSm.setTargetValue(mix01);
    outSm.setTargetValue(dbToLin(outDb));

    wetBuf.makeCopyOf(dryCopy, true);

    const float atkC = std::exp(-1.0f / (0.001f * atkMs * sr));
    const float relC = std::exp(-1.0f / (0.001f * relMs * sr));

    float envLocal = env;
    float gLocal = gainSmooth;
    float grAcc = 0.0f;

    for (int i=0; i<n; ++i)
    {
        const float l = wetBuf.getSample(0, i);
        const float r = wetBuf.getSample(1, i);

        const float dl = scHP[0].processSample(l);
        const float dr = scHP[1].processSample(r);
        const float d = 0.5f * (std::abs(dl) + std::abs(dr));

        const float coeff = (d > envLocal) ? atkC : relC;
        envLocal = d + coeff * (envLocal - d);

        float g = 1.0f;

        const float xDb = linToDb(envLocal);
        const float overDb = xDb - targetThreshDb;

        float grDb = 0.0f;
        if (overDb <= -targetKneeDb * 0.5f)
        {
            grDb = 0.0f;
        }
        else if (overDb >= targetKneeDb * 0.5f)
        {
            grDb = overDb - (overDb / targetRatio);
        }
        else
        {
            const float y = overDb + targetKneeDb * 0.5f;
            const float k = targetKneeDb;
            const float softOver = (y * y) / (2.0f * k);
            grDb = softOver - (softOver / targetRatio);
        }

        g = dbToLin(-grDb);
        grAcc += grDb;

        const float gcoeff = (g < gLocal) ? atkC : relC;
        gLocal = g + gcoeff * (gLocal - g);

        wetBuf.setSample(0, i, l * gLocal);
        wetBuf.setSample(1, i, r * gLocal);
    }

    env = envLocal;
    gainSmooth = gLocal;

    const float grDbAvg = (n > 0) ? (grAcc / (float)n) : 0.0f;
    meters.grDb.store(grDbAvg, std::memory_order_relaxed);

    // Subtle auto makeup based on glue and GR
    const float glue01 = glueSm.getCurrentValue();
    const float makeupDb = juce::jlimit(0.0f, 6.0f, 0.6f * glue01 * 6.0f + 0.25f * grDbAvg);
    const float makeup = dbToLin(makeupDb);

    for (int ch=0; ch<2; ++ch)
    {
        float* w = wetBuf.getWritePointer(ch);
        for (int i=0; i<n; ++i)
            w[i] *= makeup;
    }

    // Parallel mix
    const float mix = mixSm.getCurrentValue();
    for (int ch=0; ch<2; ++ch)
    {
        const float* dry = dryCopy.getReadPointer(ch);
        float* w = wetBuf.getWritePointer(ch);
        for (int i=0; i<n; ++i)
            w[i] = w[i] * mix + dry[i] * (1.0f - mix);
    }

    // Output gain
    outGain.setGainLinear(outSm.getCurrentValue());
    {
        juce::dsp::AudioBlock<float> blk(wetBuf);
        juce::dsp::ProcessContextReplacing<float> ctx(blk);
        outGain.process(ctx);
    }

    // Soft clip
    if (clipOn)
    {
        const float k = 1.35f;
        const float norm = std::tanh(k);
        for (int ch=0; ch<2; ++ch)
        {
            float* y = wetBuf.getWritePointer(ch);
            for (int i=0; i<n; ++i)
                y[i] = std::tanh(k * y[i]) / norm;
        }
    }

    buffer.makeCopyOf(wetBuf, true);

    // Output RMS
    double outSq = 0.0;
    for (int ch=0; ch<2; ++ch)
    {
        const float* y = buffer.getReadPointer(ch);
        for (int i=0; i<n; ++i)
            outSq += (double)y[i] * (double)y[i];
    }
    const float outRms = std::sqrt((float)(outSq / (double)(n * 2)));
    meters.outRms.store(outRms, std::memory_order_relaxed);
}

void GlueMaidAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void GlueMaidAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    dirty.store(true, std::memory_order_release);
}

// Standard JUCE factory (required by juce_add_plugin).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlueMaidAudioProcessor();
}
