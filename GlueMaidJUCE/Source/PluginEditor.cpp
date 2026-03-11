#include "PluginEditor.h"

GlueMaidAudioProcessorEditor::GlueMaidAudioProcessorEditor (GlueMaidAudioProcessor& p)
: AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (760, 420);

    setupKnob(glue, "Glue");
    setupKnob(scHp, "SC HP");
    setupKnob(mix, "Mix");
    setupKnob(output, "Output");

    punch.setButtonText("Punch");
    soft.setButtonText("Soft");
    clip.setButtonText("Clip");

    addAndMakeVisible(punch);
    addAndMakeVisible(soft);
    addAndMakeVisible(clip);

    for (auto* l : { &inLbl,&crestLbl,&grLbl,&outLbl })
    {
        l->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(*l);
    }

    auto& apvts = audioProcessor.apvts;
    aGlue = std::make_unique<Attach>(apvts, GlueMaidAudioProcessor::IDs::glue, glue);
    aSc   = std::make_unique<Attach>(apvts, GlueMaidAudioProcessor::IDs::scHp, scHp);
    aMix  = std::make_unique<Attach>(apvts, GlueMaidAudioProcessor::IDs::mix, mix);
    aOut  = std::make_unique<Attach>(apvts, GlueMaidAudioProcessor::IDs::output, output);

    aPunch = std::make_unique<BAttach>(apvts, GlueMaidAudioProcessor::IDs::punch, punch);
    aSoft  = std::make_unique<BAttach>(apvts, GlueMaidAudioProcessor::IDs::soft, soft);
    aClip  = std::make_unique<BAttach>(apvts, GlueMaidAudioProcessor::IDs::clip, clip);

    startTimerHz(15);
}

GlueMaidAudioProcessorEditor::~GlueMaidAudioProcessorEditor() {}

void GlueMaidAudioProcessorEditor::setupKnob(juce::Slider& s, const juce::String& name)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 18);
    s.setName(name);
    s.setMouseDragSensitivity(160);
    addAndMakeVisible(s);
}

void GlueMaidAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour(0xff0b0b10));

    g.setColour(juce::Colour(0xffe8e8ff));
    g.setFont(18.0f);
    g.drawText("GlueMaid", 16, 10, 300, 26, juce::Justification::left);

    g.setFont(12.0f);
    g.setColour(juce::Colour(0xffa8a8c8));
    g.drawText("Automatic Bus Glue (program-dependent timing, parallel mix)", 16, 34, 680, 18, juce::Justification::left);

    auto label = [&](juce::Slider& s)
    {
        auto r = s.getBounds();
        g.setColour(juce::Colour(0xffc8c8e8));
        g.drawFittedText(s.getName(), r.withY(r.getY()-18).withHeight(18), juce::Justification::centred, 1);
    };

    for (auto* s : { &glue,&scHp,&mix,&output })
        label(*s);
}

void GlueMaidAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(14);
    r.removeFromTop(60);

    auto top = r.removeFromTop(240);
    const int big = 170;

    glue.setBounds(top.removeFromLeft(big).reduced(12));
    scHp.setBounds(top.removeFromLeft(big).reduced(12));
    mix.setBounds(top.removeFromLeft(big).reduced(12));
    output.setBounds(top.removeFromLeft(big).reduced(12));

    auto mid = r.removeFromTop(70);
    punch.setBounds(mid.removeFromLeft(120).reduced(10).withHeight(26));
    soft.setBounds(mid.removeFromLeft(120).reduced(10).withHeight(26));
    clip.setBounds(mid.removeFromLeft(120).reduced(10).withHeight(26));

    auto meters = r.removeFromTop(90);
    inLbl.setBounds(meters.removeFromTop(22));
    crestLbl.setBounds(meters.removeFromTop(22));
    grLbl.setBounds(meters.removeFromTop(22));
    outLbl.setBounds(meters.removeFromTop(22));
}

void GlueMaidAudioProcessorEditor::timerCallback()
{
    auto toDb = [](float rms){ return 20.0f * std::log10(std::max(rms, 1.0e-12f)); };

    const float inR = audioProcessor.meters.inRms.load(std::memory_order_relaxed);
    const float outR = audioProcessor.meters.outRms.load(std::memory_order_relaxed);
    const float crest = audioProcessor.meters.crest.load(std::memory_order_relaxed);
    const float gr = audioProcessor.meters.grDb.load(std::memory_order_relaxed);

    inLbl.setText("In RMS:  " + juce::String(toDb(inR), 1) + " dB", juce::dontSendNotification);
    crestLbl.setText("Crest:   " + juce::String(crest, 2) + " (peak/rms)", juce::dontSendNotification);
    grLbl.setText("GR Avg:  " + juce::String(gr, 2) + " dB", juce::dontSendNotification);
    outLbl.setText("Out RMS: " + juce::String(toDb(outR), 1) + " dB", juce::dontSendNotification);
}
