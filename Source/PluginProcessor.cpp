/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ReShimmerAudioProcessor::ReShimmerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

ReShimmerAudioProcessor::~ReShimmerAudioProcessor()
{
}

//==============================================================================
const juce::String ReShimmerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ReShimmerAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ReShimmerAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ReShimmerAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ReShimmerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ReShimmerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ReShimmerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ReShimmerAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ReShimmerAudioProcessor::getProgramName (int index)
{
    return {};
}

void ReShimmerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

juce::AudioProcessorParameter* ReShimmerAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter("Bypass");
}

//==============================================================================
void ReShimmerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // previousDelayMS = apvts.getRawParameterValue("TIME")->load();
       
    stretch.presetDefault(2, sampleRate);
    stretch.presetCheaper(2, sampleRate);
    
    int outputLatency = stretch.outputLatency();
    setLatencySamples(outputLatency);
    
    // setup the pitch audio buffer
    mPitchBuffer.setSize( getTotalNumOutputChannels(), samplesPerBlock*2);

    stretch.reset();
    stretch.setTransposeSemitones(12);
}

void ReShimmerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ReShimmerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ReShimmerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    
    
    // float **inputBuffers, **outputBuffers;
    //int inputSamples, outputSamples;
    //stretch.process(inputBuffers, inputSamples, outputBuffers, outputSamples);
    
    auto inputBuffers = buffer.getArrayOfReadPointers();

    auto pitchOutBuffers = mPitchBuffer.getArrayOfWritePointers();
    
    
    const int bufferLength = buffer.getNumSamples();
    stretch.process(inputBuffers, bufferLength, pitchOutBuffers, bufferLength);
    
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            
            float* outbufferData = buffer.getWritePointer(channel);
            const float* pitchInBufferData = mPitchBuffer.getReadPointer(channel);
            
            
            for (int sample = 0; sample < bufferLength; ++sample)
            {
                outbufferData[sample] = pitchInBufferData[sample];
            }
            
        }
}


//==============================================================================
bool ReShimmerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ReShimmerAudioProcessor::createEditor()
{
    //return new ReShimmerAudioProcessorEditor (*this);
    
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void ReShimmerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void ReShimmerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}


juce::AudioProcessorValueTreeState::ParameterLayout ReShimmerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("Bypass", 1),
        "Bypass",
        false));

    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID("TIME", 1), "Time", 0, 1900, 0 ));

    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReShimmerAudioProcessor();
}
