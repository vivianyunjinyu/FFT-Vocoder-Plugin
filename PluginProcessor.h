/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class VocoduhAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    VocoduhAudioProcessor();
    ~VocoduhAudioProcessor() override;

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
    
    juce::AudioProcessorValueTreeState parameters;
    
    //BAND CONTROL======================================================================
    static constexpr int maxBands = 64;     // ceiling — array is this big
    int getActiveBands() const { return activeBands; }
    void getBandLevels (float* dest) const
        {
            for (int i = 0; i < activeBands; ++i)
                dest[i] = smoothedBandGain[(size_t) i];
        }

private:
    //==============================================================================
    
    double s_r = 44100.0; //sample rate
    std::atomic<float>* outputGain = nullptr; //output gain pointer
    std::atomic<float>* unvoicedAmt = nullptr; //unvoiced amount pointer
    std::atomic<float>* enhanceAmt = nullptr; //enhanced amount pointer
    juce::Random noiseGen;
    
    //FFT STUFFS----------------------------------------------------------
    static constexpr int fftOrder = 10; //2^10 -> 1024-point FFT
    static constexpr int fftSize = 1 << fftOrder; // 2^fftOrder; 1024
    std::array<float, fftSize * 2> fftData {}; //create FFT buffer to store 2x the fftSize data (real & imaginary parts)
    std::array<float, fftSize> fifo {}; //creates a "first in, first out" buffer that collects samples until it reaches 1024; THEN we ball and run FFT
    int fifoIndex = 0; //keep track of # of samples inside FIFO
    bool nextFFTBlockReady = false; //ask do we go on and FFT next chunk?
    
    //variables for the carrier signal------------------------------------
    std::array<float, fftSize * 2> carrierFftData {};
    std::array<float, fftSize> carrierFifo {};
    int carrierFifoIndex = 0;
    //--------------------------------------------------------------------
    
    //HOP WINDOW-----------------------------------------------------
    static constexpr int hopSize = fftSize / 4; //4x overlap
    static constexpr int olaBufferSize = fftSize * 2; //buffer to store overlapping IFFT audio (ola = overlap-add)
    std::array<float, olaBufferSize> olaBuffer {}; //array to store ola
    std::array<float, olaBufferSize> olaNormBuffer {}; //stores normalization amounts
    int olaReadPos = 0; //keeps track of OLA position
    std::array<float, fftSize> windowTable {}; //stores window values (i.g. hann, kaiser, etc.) for later use (multiplication)
    //------------------------------------------------------------------------

    //FFT OBJECT & BANDS -------------------------------------------------------
    juce::dsp::FFT forwardFFT { fftOrder }; //create the JUCE FFT object that'll perform forward and inverse FFT
    //------------------------------------------------------------------------
    
    //BAND CONTROL---------------------------------------------------------------
    int activeBands = 24; //current count (runtime)
        std::array<int,   maxBands + 1> bandEdgeBins {}; //store band bounds
        std::array<float, maxBands>     smoothedBandGain {}; //store band gain

        std::atomic<float>* bandCountParam = nullptr;
    //------------------------------------------------------------------------
    
    //ATTACK & RELEASE------------------------------------------------------
    std::atomic<float>* attackTime = nullptr; //attack time pointer
    std::atomic<float>* releaseTime = nullptr; //release time pointer
    
    float frameRate = 172.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    //----------------------------------------------------------------------
    
    //HELPER FUNCTIONS------------------------------------------------
    void pushNextSampleIntoFifo (float sample) noexcept; //helper function for FIFO
    void processFFTFrame() noexcept; //helper function to process one FFT and add to olabuffer
    void pushNextCarrierSampleIntoFifo (float sample) noexcept; //FIFO for carrier
    void updateBandEdges (int bands); //update the amount of bands being used
    static int bandCountForChoice (int choiceIndex); //selecting the amount of bands
    //--------------------------------------------------------------------------
    
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocoduhAudioProcessor)
};
