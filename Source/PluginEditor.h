#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class VcdrAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    VcdrAudioProcessorEditor (VcdrAudioProcessor&);
    ~VcdrAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    void timerCallback() override;

private:
    void drawFrame(juce::Graphics& g);
    
    VcdrAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VcdrAudioProcessorEditor)
};
