/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//#include <juce_Reverb.h>

#include "stretch/signalsmith-stretch.h"

//==============================================================================
/**
*/
class ReShimmerAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    ReShimmerAudioProcessor();
    ~ReShimmerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
        
    
    juce::AudioProcessorParameter* getBypassParameter() const override;
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
private:
    
    const int numPitchBuffer = 2;
    signalsmith::stretch::SignalsmithStretch<float> stretch[2];
    juce::AudioBuffer<float> mPitchBuffer[2];

    juce::AudioBuffer<float> tempBuffer;
    
    
    juce::AudioBuffer<float> preMixBuffer;
    
    
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;
    
    
    void updateReverbParams();
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReShimmerAudioProcessor)
};
