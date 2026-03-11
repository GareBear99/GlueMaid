#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class GlueMaidAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit GlueMaidAudioProcessorEditor (GlueMaidAudioProcessor&);
    ~GlueMaidAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    GlueMaidAudioProcessor& audioProcessor;

    juce::Slider glue, scHp, mix, output;
    juce::ToggleButton punch, soft, clip;

    juce::Label inLbl, crestLbl, grLbl, outLbl;

    using Attach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<Attach> aGlue, aSc, aMix, aOut;
    std::unique_ptr<BAttach> aPunch, aSoft, aClip;

    void setupKnob(juce::Slider& s, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlueMaidAudioProcessorEditor)
};
