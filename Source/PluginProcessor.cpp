#include "PluginProcessor.h"
#include "PluginEditor.h"

VcdrAudioProcessor::VcdrAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), forwardFFT(8), inverseFFT(8), window(fftSize, juce::dsp::WindowingFunction<float>::hann, false)
#endif
{
}

VcdrAudioProcessor::~VcdrAudioProcessor()
{
}

const juce::String VcdrAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VcdrAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool VcdrAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool VcdrAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double VcdrAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int VcdrAudioProcessor::getNumPrograms()
{
    return 1;
}

int VcdrAudioProcessor::getCurrentProgram()
{
    return 0;
}

void VcdrAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String VcdrAudioProcessor::getProgramName (int index)
{
    return {};
}

void VcdrAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void VcdrAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    for (int envSamp = 0; envSamp < fftSize; ++envSamp)
        sinenv[envSamp] = sin(((float)envSamp / fftSize) * juce::MathConstants<float>::pi * 2);
    
    inputBuffer.setSize (getTotalNumOutputChannels(), (int)circularBufferSize);
    outputBuffer.setSize (getTotalNumOutputChannels(), (int)circularBufferSize);

    outputWritePointer = hopSize * 2;
    //set all frequency bins to 1.0
    std::fill(binAmps + 0, binAmps + fftSize, 1);
}

void VcdrAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool VcdrAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void VcdrAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    const auto bufferSize = buffer.getNumSamples();

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        // bufferFill
        fillBuffer(channel, bufferSize, channelData);
        
        for (int sample = 0; sample < bufferSize; ++sample)
        {
            // wpisanie wartości z outputBuffer na wyjście, pomnożenie przez 0.5 (overlap)
            channelData[sample] = (outputBuffer.getSample(channel, (outputReadPointer + sample) % circularBufferSize) * .5f);
            // po odczytaniu wartości, zerujemy
            outputBuffer.setSample(channel, (outputReadPointer + sample) % circularBufferSize, 0.0f);
        }
        
    }
    outputReadPointer = (outputReadPointer + bufferSize) % circularBufferSize;
}

void VcdrAudioProcessor::fillBuffer(const int channel, const int bufferSize, float* channelData)
{
    if (channel == 1)
    {
        inputWritePointer = (inputWritePointer - bufferSize + circularBufferSize) % circularBufferSize;
        outputWritePointer = (outputWritePointer - bufferSize + circularBufferSize) % circularBufferSize;
    }
    
    for (int x = 0; x < bufferSize; x++)
    {
        // kopiowanie wartosci do input circular buffer
        inputBuffer.copyFrom(channel, inputWritePointer, channelData + x, 1);
        // zapobieganie wpisania poza tablice
        inputWritePointer = (inputWritePointer + 1) % circularBufferSize;
        
        if(++hopCount >= hopSize)
        {
            vocode(channel);
            
            // unwrapping
            for (int sample = 0; sample < fftSize; ++sample)
            {
                outputBuffer.addSample(channel, (outputWritePointer + sample) % circularBufferSize, fftBuffer[sample]);
            }
            
            // po processingu, zwiekszamy write pointer output buffera o hopSize
            outputWritePointer = (outputWritePointer + hopSize) % circularBufferSize;
            hopCount = 0;
        }
    }
    
}

void VcdrAudioProcessor::vocode(const int channel)
{
    for (int sample = 0; sample < fftSize; sample++)
    {
        chunkOne[sample] = inputBuffer.getSample(channel, (inputWritePointer + sample - fftSize + circularBufferSize) % circularBufferSize);
    }
    // windowing
    window.multiplyWithWindowingTable(chunkOne, fftSize);
    
    for (int sample = 0; sample < fftSize; ++sample)
    {
        // zapelniamy polowe fft buffer
        fftBuffer[sample] = chunkOne[sample];
    }
    
    forwardFFT.performRealOnlyForwardTransform(fftBuffer, true);
    
    getMagnitudeOfInterleavedComplexArray(fftBuffer);
    smoothSpectrum();
    
    // fft processing
    for (int sample = 0; sample < 2 * fftSize; ++sample)
    {
        fftBuffer[sample] = fftAudioEnv[sample / 2];
    }

    // polowa buffera jest zapelniona zrekonstruowanymi wartosciami
    inverseFFT.performRealOnlyInverseTransform(fftBuffer);
    multiplyBySineEnvelope(fftBuffer);
}

void VcdrAudioProcessor::getMagnitudeOfInterleavedComplexArray(float *array)
{
    for (int i = 0; i < fftSize; ++i)
    {
        magnitudes[i] = sqrt(pow(array[2 * i], 2) + pow(array[2 * i + 1], 2));
    }
}

void VcdrAudioProcessor::smoothSpectrum()
{
    juce::zeromem(fftAudioEnv, sizeof(fftAudioEnv));
    for (int smoothSamp = 0; smoothSamp < fftSize; smoothSamp++) {
        for (int j = 0; j < 5; j++) {
            if ((smoothSamp - j) >= 0) {
                fftAudioEnv[smoothSamp] += magnitudes[smoothSamp - j];
            }
        }
        fftAudioEnv[smoothSamp] /= 5;
    }
}

void VcdrAudioProcessor::multiplyBySineEnvelope(float *array)
{
    for (int i = 0; i < fftSize; ++i)
        array[i] *= sinenv[i];
}

bool VcdrAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* VcdrAudioProcessor::createEditor()
{
    return new VcdrAudioProcessorEditor (*this);
}

void VcdrAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
}

void VcdrAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VcdrAudioProcessor();
}
