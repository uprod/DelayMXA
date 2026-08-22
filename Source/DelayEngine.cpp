#include "DelayEngine.h"

namespace delaymxa
{

DelayEngine::DelayEngine() = default;

void DelayEngine::prepare (double newSampleRate, int blockSize, int numChannels)
{
    sampleRate = newSampleRate;
    numCh = juce::jmax (1, numChannels);

    juce::dsp::ProcessSpec spec { sampleRate,
                                  (juce::uint32) juce::jmax (1, blockSize),
                                  (juce::uint32) juce::jmax (2, numCh) };
    delayLine.prepare (spec);
    delayLine.setMaximumDelayInSamples ((int) (maxDelayMs * 0.001f * (float) sampleRate) + 8);

    // On lisse les changements de temps sur ~50 ms pour eviter les clics.
    delaySamples.reset (sampleRate, 0.05);
    delaySamples.setCurrentAndTargetValue (delayMs * 0.001f * (float) sampleRate);

    reset();
}

void DelayEngine::reset()
{
    delayLine.reset();
    dampStateL = 0.0f;
    dampStateR = 0.0f;
}

void DelayEngine::setDelayMs (float ms)            { delayMs    = juce::jlimit (1.0f, maxDelayMs, ms); }
void DelayEngine::setReleaseSeconds (float s)      { releaseSec = juce::jlimit (0.05f, 12.0f, s); }
void DelayEngine::setDamp (float a)                { damp       = juce::jlimit (0.0f, 1.0f, a); }
void DelayEngine::setPingPong (bool shouldPing)    { pingPong   = shouldPing; }

float DelayEngine::feedbackGain() const
{
    // RT60 : apres 'releaseSec', le niveau doit chuter de ~60 dB. Comme une
    // repetition survient toutes les 'delaySec', le gain par repetition g verifie
    //   g ^ (releaseSec / delaySec) = 10^(-3)   =>   g = 10^(-3 * delaySec / releaseSec)
    // La formule vit dans feedbackGainFor(), partagee avec l'UI.
    return feedbackGainFor (delayMs, releaseSec);
}

void DelayEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int chs        = juce::jmin (numCh, buffer.getNumChannels());

    const float fb = feedbackGain();

    // Coefficient du passe-bas de feedback : damp 0 = laisse tout passer (clair),
    // damp 1 = coupe fort (echos sombres).
    const float a = juce::jmax (0.03f, 1.0f - damp * 0.95f);

    float target = delayMs * 0.001f * (float) sampleRate;
    target = juce::jlimit (1.0f, (float) delayLine.getMaximumDelayInSamples() - 1.0f, target);
    delaySamples.setTargetValue (target);

    if (chs >= 2)
    {
        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int n = 0; n < numSamples; ++n)
        {
            const float ds = delaySamples.getNextValue();

            const float dl = delayLine.popSample (0, ds);
            const float dr = delayLine.popSample (1, ds);

            // Passe-bas dans la boucle de feedback.
            dampStateL += a * (dl - dampStateL);
            dampStateR += a * (dr - dampStateR);

            const float inL = left[n];
            const float inR = right[n];

            float pushL, pushR;
            if (pingPong)
            {
                // Le son entre en mono, et le feedback est croise : l'echo de droite
                // revient a gauche et inversement -> rebond gauche/droite.
                const float mono = 0.5f * (inL + inR);
                pushL = mono + dampStateR * fb;
                pushR = dampStateL * fb;
            }
            else
            {
                // Stereo classique : chaque canal a son propre feedback.
                pushL = inL + dampStateL * fb;
                pushR = inR + dampStateR * fb;
            }

            delayLine.pushSample (0, pushL);
            delayLine.pushSample (1, pushR);

            left[n]  = dl;   // sortie = echo seul (wet) ; le mix dry/wet est fait dans le processeur
            right[n] = dr;
        }
    }
    else
    {
        // Entree mono : pas de rebond stereo possible, simple echo.
        auto* mono = buffer.getWritePointer (0);

        for (int n = 0; n < numSamples; ++n)
        {
            const float ds = delaySamples.getNextValue();
            const float d0 = delayLine.popSample (0, ds);
            dampStateL += a * (d0 - dampStateL);

            delayLine.pushSample (0, mono[n] + dampStateL * fb);
            mono[n] = d0;
        }
    }

    // Publication du temps reellement applique (apres lissage) pour l'UI.
    uiDelayMs.store (delaySamples.getCurrentValue() / (float) sampleRate * 1000.0f,
                     std::memory_order_relaxed);
}

}
