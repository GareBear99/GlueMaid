#pragma once
#include <JuceHeader.h>

class GlueMaidAudioProcessor final : public juce::AudioProcessor,
                                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    GlueMaidAudioProcessor();
    ~GlueMaidAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    struct IDs
    {
        static constexpr const char* glue   = "glue";
        static constexpr const char* punch  = "punch";
        static constexpr const char* soft   = "soft";
        static constexpr const char* scHp   = "scHp";
        static constexpr const char* mix    = "mix";
        static constexpr const char* output = "output";
        static constexpr const char* clip   = "clip";
    };

    struct MeterState
    {
        std::atomic<float> grDb { 0.0f };
        std::atomic<float> inRms { 0.0f };
        std::atomic<float> outRms { 0.0f };
        std::atomic<float> crest { 0.0f };
    } meters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    static inline float clamp01(float x) { return juce::jlimit(0.0f, 1.0f, x); }
    static inline float dbToLin(float db) { return std::pow(10.0f, db / 20.0f); }
    static inline float linToDb(float lin) { return 20.0f * std::log10(std::max(lin, 1.0e-12f)); }

    void analyzeBlock(const juce::AudioBuffer<float>& buffer);
    void updateTargetsIfNeeded();

    float sr = 44100.0f;

    // Sidechain HP (detector only)
    juce::dsp::IIR::Filter<float> scHP[2];

    // Detector state
    float env = 0.0f;
    float gainSmooth = 1.0f;

    // Smoothed parameters
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> glueSm;

    // Derived compressor targets
    float targetThreshDb = -18.0f;
    float targetRatio = 2.0f;
    float targetKneeDb = 6.0f;
    float atkMs = 10.0f;
    float relMs = 120.0f;

    // Analysis metrics
    float inRms = 0.0f;
    float inCrest = 2.0f;

    // Buffers
    juce::AudioBuffer<float> dryCopy;
    juce::AudioBuffer<float> wetBuf;

    // Output gain
    juce::dsp::Gain<float> outGain;

    std::atomic<bool> dirty { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlueMaidAudioProcessor)
};
