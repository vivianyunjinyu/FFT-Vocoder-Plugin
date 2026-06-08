#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocoduhAudioProcessor::VocoduhAudioProcessor()
: AudioProcessor (BusesProperties()
                  .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                  .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)
                  .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)),
  parameters (*this, nullptr, juce::Identifier ("VocoduhParams"), //APVTS
              {
                    std::make_unique<juce::AudioParameterFloat> ("outputGain",   // parameterID
                                                               "Output Gain",  // parameter name
                                                               0.0f,           // minimum
                                                               1.0f,           // maximum
                                                               0.2f),          // default
                    std::make_unique<juce::AudioParameterFloat>("attackMs", "Attack", 0.1f, 100.0f, 5.0f),
                    std::make_unique<juce::AudioParameterFloat>("releaseMs", "Release", 1.0f, 500.0f, 80.0f),
                    std::make_unique<juce::AudioParameterFloat>("unvoiced", "Unvoiced", 0.0f, 1.0f, 0.0f),
                    std::make_unique<juce::AudioParameterFloat>("enhance", "Enhance", 0.0f, 1.0f, 0.0f),
                    std::make_unique<juce::AudioParameterChoice>("bandCount", "Bands",
                                                                juce::StringArray{"8", "16", "24", "32", "48", "64"}, //choices of bands
                                                                2), //default index 2 -> 16 band
      
              })
{
    outputGain = parameters.getRawParameterValue ("outputGain");
    attackTime = parameters.getRawParameterValue("attackMs");
    releaseTime = parameters.getRawParameterValue("releaseMs");
    unvoicedAmt = parameters.getRawParameterValue("unvoiced");
    enhanceAmt = parameters.getRawParameterValue("enhance");
    bandCountParam = parameters.getRawParameterValue("bandCount");
    
}

VocoduhAudioProcessor::~VocoduhAudioProcessor()
{
}

//==============================================================================
const juce::String VocoduhAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VocoduhAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool VocoduhAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool VocoduhAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double VocoduhAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int VocoduhAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int VocoduhAudioProcessor::getCurrentProgram()
{
    return 0;
}

void VocoduhAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String VocoduhAudioProcessor::getProgramName (int index)
{
    return {};
}

void VocoduhAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//maps dropdown index to real band count==============================
int VocoduhAudioProcessor::bandCountForChoice (int choiceIndex)
{
    static const int counts[] = { 8, 16, 24, 32, 48, 64 };
    return counts[juce::jlimit (0, 5, choiceIndex)];
}
//=====================================================================

//update band ranges==================================================
void VocoduhAudioProcessor::updateBandEdges (int bands)
{
    auto hzToMel = [] (float hz)  { return 2595.0f * std::log10 (1.0f + hz / 700.0f); }; //conversion Hz -> Mel
    auto melToHz = [] (float mel) { return 700.0f * (std::pow (10.0f, mel / 2595.0f) - 1.0f); }; //conversion Mel -> Hz

    const float fLow    = 20.0f;
    const float fHigh   = (float) (s_r * 0.5); //Nyquist
    const float melLow  = hzToMel (fLow);
    const float melHigh = hzToMel (fHigh);

    for (int b = 0; b <= bands; ++b) //run through every band border (bands + 1)
    {
        const float mel = melLow + (melHigh - melLow) * (float) b / (float) bands; //evenly space out mel band border
        const float hz  = melToHz (mel);
        int bin = (int) std::round (hz * (float) fftSize / (float) s_r); //calculate bin index from hz
        bandEdgeBins[(size_t) b] = juce::jlimit (0, fftSize / 2, bin); //write down the bin (jlimit = safety net)
    }
}
//========================================================================

//==============================================================================
void VocoduhAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    s_r = sampleRate;

    setLatencySamples(fftSize); //report latency
    
    frameRate = (float) (s_r / hopSize); //calculate amount of samples per each frame
    std::fill(smoothedBandGain.begin(), smoothedBandGain.end(), 0.0f); //reset all band gains
    
    //RESETS FFT THINGS--------------------------------------------------------
    fifoIndex = 0;
    olaReadPos = 0;
    nextFFTBlockReady = false;
    
    std::fill (fifo.begin(), fifo.end(), 0.0f); // FIFO
    std::fill (fftData.begin(), fftData.end(), 0.0f); // FFTData
    std::fill (windowTable.begin(), windowTable.end(), 0.0f); // windowTable
    std::fill (olaBuffer.begin(), olaBuffer.end(), 0.0f); // olaBuffer
    std::fill (olaNormBuffer.begin(), olaNormBuffer.end(), 0.0f); // NormBuffer
    //---------------------------------------------------------------------------
    
    //RESETS CARRIER FFT THINGS--------------------------------------------------
    std::fill (carrierFifo.begin(), carrierFifo.end(), 0.0f);
    std::fill (carrierFftData.begin(), carrierFftData.end(), 0.0f);
    carrierFifoIndex = 0;
    //---------------------------------------------------------------------------
    
    //FILL WINDOW TABLE (KAISER)--------------------------------------------------
    juce::dsp::WindowingFunction<float>::fillWindowingTables (
        windowTable.data(),
        (size_t) fftSize,
        juce::dsp::WindowingFunction<float>::kaiser,
        false,                                         
        8.0f);
    //-------------------------------------------------------------------------------
    
    // MEL BAND EDGES ------------------------------------------------------------
    activeBands = bandCountForChoice ((int) *bandCountParam);
    updateBandEdges (activeBands);
    // ---------------------------------------------------------------------------
}

void VocoduhAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool VocoduhAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
   #else
    const auto& mainInput  = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    // leave it if the main input/output are mono/stereo
    if (mainOutput != juce::AudioChannelSet::mono()
        && mainOutput != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    // For this plugin, the main vocal input should match the output layout.
    if (mainInput != mainOutput)
        return false;
   #endif

    // Input bus 1 is our sidechain/carrier input.
    const auto& sidechain = layouts.getChannelSet (true, 1); //setting 

    // Sidechain can be disabled, mono, or stereo.
    if (! sidechain.isDisabled()
        && sidechain != juce::AudioChannelSet::mono()
        && sidechain != juce::AudioChannelSet::stereo())
        return false;

    return true;
   #endif
}
#endif

// MODULATOR function to pass samples and mark every 1024 sample when it's ready for next FFT ================
void VocoduhAudioProcessor::pushNextSampleIntoFifo (float sample) noexcept
{
    fifo[(size_t) fifoIndex] = sample; //store new sample into FIFO buffer
    ++fifoIndex; //increments pointer

    if (fifoIndex >= fftSize) //check if we filled the 1024 sample frame
    {
        std::fill (fftData.begin(), fftData.end(), 0.0f); //clears fftData that has old data
        std::copy (fifo.begin(), fifo.end(), fftData.begin()); //copy FIFO data into now cleared fftData array

        nextFFTBlockReady = true; //we ready to FFT!

        std::move (fifo.begin() + hopSize, fifo.end(), fifo.begin()); //delete the first [hopSize] amount of data & shift everything forward
        fifoIndex = fftSize - hopSize; //set the next write position
    }
}
//==================================================================================================

// CARRIER function to pass samples and mark every 1024 sample when it's ready for next FFT ================
void VocoduhAudioProcessor::pushNextCarrierSampleIntoFifo(float sample) noexcept
{
    carrierFifo[(size_t) carrierFifoIndex] = sample; //store new sample into carrier FIFO buffer
    ++carrierFifoIndex; //increments pointer

    if (carrierFifoIndex >= fftSize) //check if we filled the 1024 sample frame
    {
        std::fill (carrierFftData.begin(), carrierFftData.end(), 0.0f); //clears fftData that has old data
        std::copy (carrierFifo.begin(), carrierFifo.end(), carrierFftData.begin()); //copy FIFO data into now cleared fftData array

        std::move (carrierFifo.begin() + hopSize, carrierFifo.end(), carrierFifo.begin()); //delete the first [hopSize] amount of data & shift everything forward
        carrierFifoIndex = fftSize - hopSize; //set the next write position
    }
}
//==================================================================================================

//function to perform one frame of FFT================================================================
void VocoduhAudioProcessor::processFFTFrame() noexcept
{
    for (int i = 0; i < fftSize; ++i){
        fftData[(size_t) i ] *= windowTable[(size_t) i]; //apply window to modulator
        carrierFftData[(size_t) i] *= windowTable[(size_t) i]; //apply window to carrier
    }

    forwardFFT.performRealOnlyForwardTransform (fftData.data()); //perform FFT on modulator
    forwardFFT.performRealOnlyForwardTransform(carrierFftData.data()); ///perform FFT on carrier

    // SPECTRUAL MANIPULATION PART------------------------------------------------------------------------
    const float unvoiced = *unvoicedAmt;
    
    auto* mod = reinterpret_cast<std::complex<float>*>(fftData.data()); //recasting bins of modulator as complex float
    auto* car = reinterpret_cast<std::complex<float>*>(carrierFftData.data()); //recasting bins of carrier
    
    const int numBins = fftSize / 2;
    const float enhance = *enhanceAmt;

    for (int b = 0; b < activeBands; ++b)
    {
        const int startBin = bandEdgeBins[(size_t) b]; //store lower bound
        const int endBin   = bandEdgeBins[(size_t) b + 1]; //store upper bound

        // measure the voice's average loudness across this band
        float sum = 0.0f;
        for (int k = startBin; k < endBin; ++k) //go through the current band range
            sum += std::abs (mod[k]); //add the volume to sum

        const int count = juce::jmax (1, endBin - startBin); //count the amount of bins
        const float target = sum / (float) count; //calculate average gain in the band
        
        const float coeff = (target > smoothedBandGain[(size_t) b]) ? attackCoeff : releaseCoeff;
        //if target is higher than right now, use attack; if it's not, use release
            smoothedBandGain[(size_t) b] = coeff * smoothedBandGain[(size_t) b] + (1.0f - coeff) * target; //slide the gain towards target

        const float noiseWeight = (float) b / (float) (activeBands - 1); //lower the band, less noise it'll have
        for (int k = startBin; k < endBin; ++k){
            const std::complex<float> noise (noiseGen.nextFloat() * 2.0f - 1.0f, noiseGen.nextFloat() * 2.0f - 1.0f); //generate noise
            const std::complex<float> carrierBin = car[k] + unvoiced * (noiseWeight) * noise; //add noise to carrier signal
                
            const float mag = std::abs(carrierBin); //get magnitude of carrierBin
            const std::complex<float> flat = (mag > 1e-6f) ? carrierBin / mag : carrierBin;   //if magnitude is significant, get unit magnitude
            const std::complex<float> shaped = carrierBin * (1.0f - enhance) + flat * enhance; //blend carrierBin with its flattened ver.
            
            mod[k] = shaped * smoothedBandGain[(size_t) b]; //smooth the signal and assign it
        }
    }

    for (int k = 1; k < numBins; ++k)
        mod[fftSize - k] = std::conj (mod[k]); //assign corresponding values to imaginary part of spectrum
    // SPECTRUAL MANIPULATION PART------------------------------------------------------------------------

    forwardFFT.performRealOnlyInverseTransform (fftData.data()); //perform IFFT

    for (int i = 0; i < fftSize; ++i) //puts data into olaBuffer
    {
        const int index = (olaReadPos + i) % olaBufferSize; //calculate current index in olaBuffer (and wrap around when it reaches the end)

        const float reconstructedSample = fftData[(size_t) i] * windowTable[(size_t) i]; //get the reconstructed IFFT sample and window AGAIN

        olaBuffer[(size_t) index] += reconstructedSample; //add the sample to the current ola position
        olaNormBuffer[(size_t) index] += windowTable[(size_t) i] * windowTable[(size_t) i]; //add window squared since we windowed twice
    }

    nextFFTBlockReady = false; //reset FIFO command
}
//==================================================================================================


//PROCESS BLOCK GOES BURRRRRRR======================================================================
void VocoduhAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    
    auto sidechain = getBusBuffer(buffer, true, 1); //get carrier input
    const bool haveCarrier = sidechain.getNumChannels() > 0; //safety net
    
    const float gain = *outputGain; //get output gain pointer
    
    //UPDATE AMOUNT OF BANDS USER WANTS------------------------------
    const int desiredBands = bandCountForChoice ((int) *bandCountParam);
    if (desiredBands != activeBands)
    {
        activeBands = desiredBands;
        updateBandEdges (activeBands);
        std::fill (smoothedBandGain.begin(), smoothedBandGain.end(), 0.0f); //reset BandGain
    }
    //----------------------------------------------------------------

    for (int sample = 0; sample < numSamples; ++sample)
    {
        //GET INPUT SAMPLE AND SEND IT TO FIFO-------------------------------------
        float inputSample = 0.0f;
        float carrierSample = 0.0f;

        if (getTotalNumInputChannels() > 0) //MODULATOR
            inputSample = buffer.getReadPointer (0)[sample]; //get the input sample

        pushNextSampleIntoFifo (inputSample); //send into fifo helper
        
        if (haveCarrier) //CARRIER
            carrierSample = sidechain.getReadPointer(0)[sample];
        pushNextCarrierSampleIntoFifo(carrierSample);
        //--------------------------------------------------------------------------

        if (nextFFTBlockReady)
            processFFTFrame(); //perform one block of FFT

        float outputSample = 0.0f;
        
        //normalize and set output sample to the result---------------------------
        const float norm = olaNormBuffer[(size_t) olaReadPos]; //read the normalization value
        if (norm > 0.000001f){
            outputSample = olaBuffer[(size_t) olaReadPos] / norm; //normalize volume and send it to outputSample
            outputSample *= gain; //control by global output gain
            
            const float attackSec  = *attackTime  * 0.001f; //convert attack to second
            const float releaseSec = *releaseTime * 0.001f; //concert release to second
            attackCoeff  = std::exp (-1.0f / (attackSec  * frameRate)); //calculate how fast to alter gain attack
            releaseCoeff = std::exp (-1.0f / (releaseSec * frameRate)); //calculate how fast to alter gain release
        }
        //------------------------------------------------------------------------

        //reset & increment ola stuffs-------------------------------------------------
        olaBuffer[(size_t) olaReadPos] = 0.0f; //reset olaBuffer after it's used
        olaNormBuffer[(size_t) olaReadPos] = 0.0f; //reset olaNormBuffer after it's used

        ++olaReadPos; //increment ola position

        if (olaReadPos >= olaBufferSize)
            olaReadPos = 0; //wraps ola position back
        //------------------------------------------------------------------------------

        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
            buffer.setSample (channel, sample, outputSample); //output sample
    }
}
//==================================================================================================
        

//==============================================================================
bool VocoduhAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* VocoduhAudioProcessor::createEditor()
{
    return new VocoduhAudioProcessorEditor (*this);
}

//==============================================================================
void VocoduhAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void VocoduhAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocoduhAudioProcessor();
}
