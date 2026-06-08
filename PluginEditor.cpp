#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocoduhAudioProcessorEditor::VocoduhAudioProcessorEditor (VocoduhAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    addAndMakeVisible (spectrum);

    setupKnob (gainSlider,     gainLabel,     "OUTPUT");
    setupKnob (attackSlider,   attackLabel,   "ATTACK");
    setupKnob (releaseSlider,  releaseLabel,  "RELEASE");
    setupKnob (unvoicedSlider, unvoicedLabel, "UNVOICED");
    setupKnob (enhanceSlider,  enhanceLabel,  "ENHANCE");

    gainAttachment     = std::make_unique<SliderAttachment> (audioProcessor.parameters, "outputGain", gainSlider);
    attackAttachment   = std::make_unique<SliderAttachment> (audioProcessor.parameters, "attackMs",   attackSlider);
    releaseAttachment  = std::make_unique<SliderAttachment> (audioProcessor.parameters, "releaseMs",  releaseSlider);
    unvoicedAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "unvoiced",   unvoicedSlider);
    enhanceAttachment  = std::make_unique<SliderAttachment> (audioProcessor.parameters, "enhance",    enhanceSlider);

    // emphasize OUTPUT — bigger, bolder label (the knob itself is enlarged in resized())
    gainLabel.setFont (juce::Font (juce::FontOptions().withHeight (18.0f).withStyle ("Bold")));

    // bands dropdown
    bandsBox.addItemList ({ "8", "16", "24", "32", "48", "64" }, 1);   // item IDs start at 1
    addAndMakeVisible (bandsBox);
    bandsAttachment = std::make_unique<ComboAttachment> (audioProcessor.parameters, "bandCount", bandsBox);

    bandsLabel.setText ("BANDS", juce::dontSendNotification);
    bandsLabel.setJustificationType (juce::Justification::centredRight);
    bandsLabel.setColour (juce::Label::textColourId, juce::Colour (0xff20486E));
    addAndMakeVisible (bandsLabel);

    setSize (600, 470);
}

VocoduhAudioProcessorEditor::~VocoduhAudioProcessorEditor()
{
    gainSlider.setLookAndFeel (nullptr);
    attackSlider.setLookAndFeel (nullptr);
    releaseSlider.setLookAndFeel (nullptr);
    unvoicedSlider.setLookAndFeel (nullptr);
    enhanceSlider.setLookAndFeel (nullptr);
}

//==============================================================================
void VocoduhAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff20486E));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setLookAndFeel (&blueLnf);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, juce::Colour (0xff20486E));
    l.setFont (juce::Font (juce::FontOptions().withHeight (14.0f).withStyle ("Bold")));
    addAndMakeVisible (l);
}

void VocoduhAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xffDCECFB), 0.0f, 0.0f,
                             juce::Colour (0xffBBD7F0), 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    auto top = getLocalBounds().removeFromTop (64);

    g.setColour (juce::Colour (0xff20486E));
    g.setFont (juce::Font (juce::FontOptions().withHeight (30.0f).withStyle ("Bold")));
    g.drawText ("VOCODUH", top.removeFromTop (40), juce::Justification::centred, false);

    g.setColour (juce::Colour (0xffD17BA6));
    g.setFont (juce::Font (juce::FontOptions().withHeight (15.0f)));
    g.drawText ("vivian creations, made with FFT and love :)", top, juce::Justification::centred, false);
}

void VocoduhAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (64);            // title + subtext
    area.reduce (12, 8);

    // two knobs down each side
    auto left  = area.removeFromLeft (120);
    auto right = area.removeFromRight (120);

    auto stack = [] (juce::Rectangle<int> col,
                     juce::Label& l1, juce::Slider& s1,
                     juce::Label& l2, juce::Slider& s2)
    {
        auto t = col.removeFromTop (col.getHeight() / 2);
        l1.setBounds (t.removeFromTop (18));
        s1.setBounds (t.reduced (4));
        l2.setBounds (col.removeFromTop (18));
        s2.setBounds (col.reduced (4));
    };
    stack (left,  attackLabel,   attackSlider,   releaseLabel, releaseSlider);   // ATTACK / RELEASE
    stack (right, unvoicedLabel, unvoicedSlider, enhanceLabel, enhanceSlider);   // UNVOICED / ENHANCE

    // center column: histogram on top, big OUTPUT in the middle, BANDS at the bottom
    auto centre = area.reduced (10, 0);

    auto bands  = centre.removeFromBottom (40);
    auto output = centre.removeFromBottom (160);
    spectrum.setBounds (centre);

    gainLabel.setBounds (output.removeFromTop (20));
    gainSlider.setBounds (output.withSizeKeepingCentre (140, 140));   // bigger than the side knobs

    auto db = bands.withSizeKeepingCentre (220, 28);
    bandsLabel.setBounds (db.removeFromLeft (56));
    bandsBox.setBounds (db);
}
