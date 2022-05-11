#pragma once

#include <JuceHeader.h>

class VcdrAudioProcessor  : public juce::AudioProcessor
{
public:
    VcdrAudioProcessor();
    ~VcdrAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

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
    
    void getMagnitudeOfInterleavedComplexArray(float *array);
    void smoothSpectrum();

    float binAmps[256];
    const int fftSize = 256;
    
    const int scopeSize = 512;
    bool nextBlockReady = false;
    float scopeData[512];
    
private:
    void fillBuffer(const int channel, const int bufferSize, float* channelData);
    void vocode(const int channel);
    void multiplyBySineEnvelope(float *array);
    void drawNextFrameOfSpectrum();
    
    const int circularBufferSize = 4096;
    juce::AudioBuffer<float> inputBuffer;
    juce::AudioBuffer<float> outputBuffer;
    
    int inputWritePointer = 0;
    int outputWritePointer;
    int outputReadPointer = 0;

    juce::dsp::FFT forwardFFT;
    juce::dsp::FFT inverseFFT;
    
    const int hopSize = fftSize / 2;
    int hopCount = 0;
    float chunkOne[256];
    float fftBuffer[512];
    
    float magnitudes[256];
    float fftAudioEnv[256];
    double sinenv[256];
    
    juce::String fftSizeStr = "";
    juce::dsp::WindowingFunction<float> window;
    int samplesToEnd = 0;
    int samplesatBeg = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VcdrAudioProcessor)
};
