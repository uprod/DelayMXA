#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DelayEngine.h"

namespace delaymxa
{

class DelayProcessor : public juce::AudioProcessor
{
public:
    DelayProcessor();
    ~DelayProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DelayMXA"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    // La traine de l'echo continue apres l'arret : on annonce le release time a
    // l'hote pour qu'il laisse le plugin sonner.
    double getTailLengthSeconds() const override
    {
        return (double) apvts.getRawParameterValue ("release")->load();
    }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    // Temps de delai reellement applique (lisse) pour l'affichage temps reel
    // de l'editeur (FIG. 1 / FIG. 2 suivent la glisse facon bande).
    float getDelayMsLive() const noexcept { return engine.getUiDelayMs(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pushParameterUpdatesToEngine();

    juce::AudioProcessorValueTreeState apvts;
    DelayEngine engine;

    // Copie du signal sec, pre-allouee dans prepareToPlay : processBlock ne
    // doit jamais allouer sur le thread audio.
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayProcessor)
};

}
