#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace delaymxa
{

namespace IDs
{
    constexpr auto time     = "time";
    constexpr auto release  = "release";
    constexpr auto damp     = "damp";
    constexpr auto pingPong = "pingPong";
    constexpr auto mix      = "mix";
}

juce::AudioProcessorValueTreeState::ParameterLayout DelayProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Affichages "tally" a la machine a ecrire, aussi bien dans l'editeur que
    // dans les lignes d'automation de l'hote.
    const auto msAttr = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int)
            { return (v < 100.0f ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + " ms"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });

    const auto secAttr = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " s"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });

    const auto pctAttr = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; });

    // Temps de delai. Skew < 1 = plus de finesse dans les temps courts.
    params.push_back (std::make_unique<P> (juce::ParameterID { IDs::time, 1 },
        "Time", juce::NormalisableRange<float> (1.0f, DelayEngine::maxDelayMs, 0.1f, 0.3f), 350.0f, msAttr));

    // Release time : duree de la traine de l'echo (-60 dB), en secondes.
    params.push_back (std::make_unique<P> (juce::ParameterID { IDs::release, 1 },
        "Release", juce::NormalisableRange<float> (0.05f, 12.0f, 0.001f, 0.35f), 0.8f, secAttr));

    params.push_back (std::make_unique<P> (juce::ParameterID { IDs::damp, 1 },
        "Damp", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f, pctAttr));

    params.push_back (std::make_unique<P> (juce::ParameterID { IDs::mix, 1 },
        "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f, pctAttr));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { IDs::pingPong, 1 }, "Ping-Pong", true,
        juce::AudioParameterBoolAttributes()
            .withStringFromValueFunction ([] (bool v, int) { return juce::String (v ? "ON" : "OFF"); })));

    return { params.begin(), params.end() };
}

DelayProcessor::DelayProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

DelayProcessor::~DelayProcessor() = default;

void DelayProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    engine.reset();

    dryBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock);
}

void DelayProcessor::releaseResources()
{
    engine.reset();
}

bool DelayProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == main;
}

void DelayProcessor::pushParameterUpdatesToEngine()
{
    engine.setDelayMs        (apvts.getRawParameterValue (IDs::time)->load());
    engine.setReleaseSeconds (apvts.getRawParameterValue (IDs::release)->load());
    engine.setDamp           (apvts.getRawParameterValue (IDs::damp)->load());
    engine.setPingPong       (apvts.getRawParameterValue (IDs::pingPong)->load() > 0.5f);
}

void DelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    pushParameterUpdatesToEngine();

    // On garde une copie du son d'origine pour le mix dry/wet.
    dryBuffer.makeCopyOf (buffer, true);

    engine.process (buffer);   // 'buffer' contient maintenant l'echo seul (wet)

    const float wetAmt = apvts.getRawParameterValue (IDs::mix)->load();
    const float dryAmt = 1.0f - wetAmt;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dryIn = dryBuffer.getReadPointer (juce::jmin (ch, dryBuffer.getNumChannels() - 1));

        for (int n = 0; n < buffer.getNumSamples(); ++n)
            wet[n] = dryAmt * dryIn[n] + wetAmt * wet[n];
    }
}

juce::AudioProcessorEditor* DelayProcessor::createEditor()
{
    return new DelayEditor (*this);
}

void DelayProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DelayProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

}

// Point d'entree du plugin JUCE — doit etre au niveau global.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new delaymxa::DelayProcessor();
}
