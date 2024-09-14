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
    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    
    // previousDelayMS = apvts.getRawParameterValue("TIME")->load();
       
    for (int i=0; i<numPitchBuffer; ++i)
    {
        stretch[i].presetDefault(2, sampleRate);
        mPitchBuffer[i].setSize(numOutputChannels, samplesPerBlock);
        
        stretch[i].reset();
    }
    
    
    // setup the preMixBuffer
    preMixBuffer.setSize(numOutputChannels, samplesPerBlock);
    
    //DBG(sampleRate);
    //DBG(samplesPerBlock);
    
    int outputLatency = stretch[0].outputLatency();
    setLatencySamples(outputLatency);
    

    const int pitch1 = apvts.getRawParameterValue("PITCH1")->load();
    const int pitch2 = apvts.getRawParameterValue("PITCH2")->load();
    const int tonalityLimit = 8000;
    stretch[0].setTransposeSemitones(pitch1, tonalityLimit);
    stretch[1].setTransposeSemitones(pitch2, tonalityLimit);
    
    // tests
    tempBuffer.setSize(numOutputChannels, samplesPerBlock);
    
    
    auto processSpec = juce::dsp::ProcessSpec();
        
    processSpec.sampleRate = sampleRate;
    processSpec.maximumBlockSize = samplesPerBlock;
    processSpec.numChannels = numInputChannels;
    
    reverb.prepare(processSpec);
        

    
    //reverbParams.roomSize = 0.5f;
    //reverbParams.damping = 0.5f;
    //reverbParams.dryLevel = 0.4;
    //reverbParams.wetLevel = 0.33f;
    //reverbParams.width = 1.0f;
    //reverbParams.freezeMode = 0.0f;
    //reverb.setParameters(reverbParams);
    
    updateReverbParams();
    
    reverb.setEnabled(true);
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
    {
        buffer.clear(i, 0, buffer.getNumSamples());
        //mPitchBuffer.clear(i, 0, buffer.getNumSamples());
        
        mPitchBuffer[0].clear(i, 0, buffer.getNumSamples());
        mPitchBuffer[1].clear(i, 0, buffer.getNumSamples());
        
        preMixBuffer.clear(i, 0, buffer.getNumSamples());
    }
    
    
    bool bypassed = apvts.getRawParameterValue("Bypass")->load();
    
    if (! bypassed)
    {
        
        // float **inputBuffers, **outputBuffers;
        //int inputSamples, outputSamples;
        //stretch.process(inputBuffers, inputSamples, outputBuffers, outputSamples);

        const int bufferLength = buffer.getNumSamples();
        
        
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            const float* inBuf = buffer.getReadPointer(channel);
            float* outBuf = tempBuffer.getWritePointer(channel);
            
            for (int sample = 0; sample < bufferLength; ++sample)
            {
                outBuf[sample] = inBuf[sample];
            }
        }
                
    
        //DBG(bufferLength);
        auto inputBuffers = buffer.getArrayOfReadPointers();
        //auto inputBuffers = tempBuffer.getArrayOfReadPointers();

        
        // calculate all pitchbuffer
        //for (int pitch=0; pitch<numPitchBuffer; ++pitch)
        //{
        //    auto pitchOutBuffers = mPitchBuffer[pitch].getArrayOfWritePointers();
        //    stretch[pitch].process(inputBuffers, bufferLength, pitchOutBuffers, bufferLength);
        //}
        
        
        //DBG(inputBuffers[0][10]);
        auto pitchOutBuffers = mPitchBuffer[0].getArrayOfWritePointers();
        stretch[0].process(inputBuffers, bufferLength, pitchOutBuffers, bufferLength);
        
        //DBG(inputBuffers[0][10]);
        //auto inputBuffers2 = buffer.getArrayOfReadPointers();
        auto pitchOutBuffers2 = mPitchBuffer[1].getArrayOfWritePointers();
        stretch[1].process(inputBuffers, bufferLength, pitchOutBuffers2, bufferLength);
        
        
        // Mixing variables
        const float masterMix = apvts.getRawParameterValue("MIX")->load();
        const float pBalance = apvts.getRawParameterValue("PBALANCE")->load();
        
        
        float rmix1 = pBalance;
        float rmix2 = 1.0 - pBalance;
        
        // preMixing
        // should mix all pitched buffer together before the reverb
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            const float* pitchInBufferData1 = mPitchBuffer[0].getReadPointer(channel);
            const float* pitchInBufferData2 = mPitchBuffer[1].getReadPointer(channel);
            
            float* preMixBufferData = preMixBuffer.getWritePointer(channel);
            
            for (int sample = 0; sample < bufferLength; ++sample)
            {
                // mix the pitched signal together using, mixing paramaters
                preMixBufferData[sample] = pitchInBufferData1[sample] * rmix1 + pitchInBufferData2[sample] * rmix2;
            }
        }
        
        // apply Reverb to the preMixing buffer
        updateReverbParams();    // load the params from the apvts
        auto audioBlock = juce::dsp::AudioBlock<float>(preMixBuffer);
        auto processContext = juce::dsp::ProcessContextReplacing<float>(audioBlock);
        reverb.process(processContext);
        
        
        // final mixing
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            
            float* outbufferData = buffer.getWritePointer(channel);
            const float* preMixBufferData = preMixBuffer.getReadPointer(channel);
            
            const float dryMix = 1.0 - masterMix;
            
            for (int sample = 0; sample < bufferLength; ++sample)
            {
                outbufferData[sample] *= dryMix;
                
                // add mixed signal
                outbufferData[sample] += masterMix*preMixBufferData[sample];
            }
            
        }
        
        
        // update parameters
        
        const int pitch1 = apvts.getRawParameterValue("PITCH1")->load();
        const int pitch2 = apvts.getRawParameterValue("PITCH2")->load();
        const int tonalityLimit = 8000;
        
        stretch[0].setTransposeSemitones(pitch1, tonalityLimit);
        stretch[1].setTransposeSemitones(pitch2, tonalityLimit);

    }
}

void ReShimmerAudioProcessor::updateReverbParams()
{
    const float reverbMix = apvts.getRawParameterValue("REVERBMIX")->load();
    const float roomSize = apvts.getRawParameterValue("ROOMSIZE")->load();
    reverbParams.roomSize = roomSize;
    reverbParams.damping = apvts.getRawParameterValue("DAMPING")->load();
    reverbParams.wetLevel = reverbMix * (1-0.7*roomSize);
    reverbParams.dryLevel = 1.0 - reverbMix;
    reverbParams.width = apvts.getRawParameterValue("WIDTH")->load();
    reverbParams.freezeMode = apvts.getRawParameterValue("FREEZE")->load();

    reverb.setParameters(reverbParams);
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

    
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("MIX", 1), "Mix", 0.0, 1.0, 1.0));
    
    // pitch parameters
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID("PITCH1", 1), "Pitch1", -12, 24, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID("PITCH2", 1), "Pitch2", -12, 24, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("PBALANCE", 1), "PBalance", 0.0, 1.0, 0.5));

    // reverb parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("ROOMSIZE", 1), "RoomSize", 0.0, 1.0, 0.5));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("DAMPING", 1), "Damping", 0.0, 1.0, 0.5));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("REVERBMIX", 1), "ReverbMix", 0.0, 1.0, 0.5));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("WIDTH", 1), "Width", 0.0, 1.0, 1.0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("FREEZE", 1), "Freeze", 0.0, 1.0, 0.0));
    

    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReShimmerAudioProcessor();
}
