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
    previousDelayMS = apvts.getRawParameterValue("TIME")->load();
       
    mSampleRate = sampleRate;
       
    const int numInputChannels = getTotalNumInputChannels();
       
    // [2] Set size of the delay buffer:
    const int delayBufferSize = 2.0 * (sampleRate + samplesPerBlock); //2 second buffer
       
    mDelayBuffer.setSize(numInputChannels, delayBufferSize);
       
    mDelayBuffer.clear();
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

    const int bufferLength = buffer.getNumSamples();
    const int delayBufferLength = mDelayBuffer.getNumSamples();
    
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            // [3] Create read pointers into our buffers.
            const float* bufferData = buffer.getReadPointer(channel);
            const float* delayBufferData = mDelayBuffer.getReadPointer(channel);
            
            //fillDBuffer does circular buffer stuff
            fillDelayBuffer(channel, bufferLength, delayBufferLength, bufferData, delayBufferData);
            
            //[maybe 10?] call getFromDelayBuffer to copy data from delayBuffer to regular buffer (function unfinished)
            getFromDelayBuffer (buffer, channel, bufferLength, delayBufferLength, bufferData, delayBufferData);
        }
        
        // [5] After copying a full buffer's samples into our delay buffer (512 samps), we want to INCREMENT THE WRITEPOSITION by the bufferLength
        mWritePosition += bufferLength;
        // [6] Wrap around when we get to the end of our delayBuffer, so that when our writePosition exceeds the delayBufferLength, it starts writing from zero again.
        mWritePosition %= delayBufferLength;
    
}

void ReShimmerAudioProcessor::fillDelayBuffer (int channel, const int bufferLength, const int delayBufferLength, const float* bufferData, const float* delayBufferData)
{
    // [4b] Copy the data from main buffer to delay buffer
    // Check that our delayBufferLength is greater than bufferLength + mWritePosition. This will be the case most of the time, but not when the writePosition reaches a certain point
    if (delayBufferLength >= bufferLength + mWritePosition)
    {
        //mDelayBuffer.copyFromWithRamp(<#int destChannel#>, <#int destStartSample#>, <#const float *source#>, <#int numSamples#>, <#float startGain#>, <#float endGain#>)
        mDelayBuffer.copyFromWithRamp (channel, mWritePosition, bufferData, bufferLength, 0.8, 0.8);
    } else {
        //If delayBufferLength !> bufferLength + mWritePosition, we don't wanna copy a full bufffer's worth, only how many samples are left to fill in our delayBuffer
        const int delayBufferRemaining = delayBufferLength - mWritePosition;
        
        mDelayBuffer.copyFromWithRamp (channel, mWritePosition, bufferData, delayBufferRemaining, 0.8, 0.8);
        // [4c] We're at the end of our delayBuffer now. Keep copying, but our writePosition will be 0 now. Also, since we didn't copy the whole buffer last time, we need to make up for that.
        
        // Copy the leftover bit from the buffer, by adding it to bufferData. And the amount of samples we want to copy here is the leftover
        //mDelayBuffer.copyFromWithRamp (channel, 0, bufferData + delayBufferRemaining, bufferLength - delayBufferRemaining, 0.8, 0.8);
        mDelayBuffer.copyFromWithRamp (channel, 0, bufferData + delayBufferRemaining, bufferLength - delayBufferRemaining, 0.8, 0.8);
        
    }
}

void ReShimmerAudioProcessor::getFromDelayBuffer (juce::AudioBuffer<float>& buffer, int channel, const int bufferLength, const int delayBufferLength, const float* bufferData, const float* delayBufferData)
{
    // [7] Need a delay time in MS
    
    float currentDelayMS = apvts.getRawParameterValue("TIME")->load();
    //float currentDelayMS = 10;
    float delayMS;
    
    //====
    if (currentDelayMS == previousDelayMS)
    {
        //call a set delay time function
        
    } else {
        //interpolate
        
    }
  
    delayMS = currentDelayMS;
    
    //===
    
    float delaySamps = mSampleRate * (delayMS) / 1000;
    // Create a read position. We want to be able to go back in time into our delay buffer and grab audio data.
    //const int readPosition = static_cast<int> ( (delayBufferLength + mWritePosition) - delaySamps) % delayBufferLength;
    
    const int readPosition = static_cast<int> (delayBufferLength + mWritePosition - (delaySamps)) % delayBufferLength;
    
    // [8] Now we want to add a signal from our delayBuffer to our regular buffer. We need to do some checks first.
    
    if (delayBufferLength > bufferLength + readPosition)
    {
        // If statement checks if there are enough values in the delay buffer.
        // [9] Add from delayBuffer to buffer
        buffer.addFrom (channel, 0, delayBufferData + readPosition, bufferLength);
    } else {
        //how much space left in buffer?
        const int bufferRemaining = delayBufferLength - readPosition;
        buffer.addFrom (channel, 0, delayBufferData + readPosition, bufferRemaining);
        
        buffer.addFrom (channel, bufferRemaining, delayBufferData, bufferLength - bufferRemaining);
        
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
