#include "PluginProcessor.h"
#include "PluginEditor.h"

VcdrAudioProcessorEditor::VcdrAudioProcessorEditor (VcdrAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    startTimerHz (30);
    setSize (400, 300);
}

VcdrAudioProcessorEditor::~VcdrAudioProcessorEditor()
{
}

void VcdrAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    drawFrame (g);
}

void VcdrAudioProcessorEditor::resized()
{
}

void VcdrAudioProcessorEditor::drawFrame(juce::Graphics& g)
{
    for (int i = 1; i < audioProcessor.scopeSize; ++i)
    {
        auto width  = getLocalBounds().getWidth();
        auto height = getLocalBounds().getHeight();

        
        float firstPointX = (float) juce::jmap(i - 1, 0, audioProcessor.scopeSize - 1, 0, width);
        float secondPointY = (float) juce::jmap (audioProcessor.scopeData[i], 0.0f, 1.0f, 0.0f, (float) height);
        
        g.setColour(juce::Colours::orange);
        //g.drawLine( { firstPointX, firstPointY, secondPointX, secondPointY } );
        g.drawRect(firstPointX, 0.0f, 1.0f, secondPointY);
    }
}

void VcdrAudioProcessorEditor::timerCallback()
{
    if (audioProcessor.nextBlockReady)
    {
        audioProcessor.nextBlockReady = false;
        repaint();
    }
}
