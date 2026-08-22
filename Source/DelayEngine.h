#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace delaymxa
{

// Coeur DSP du delay/echo. Deux lignes a retard (gauche/droite). A chaque
// repetition on reinjecte le son retarde dans la ligne (feedback) : c'est ce qui
// cree les echos successifs.
//
//  - "Release time" : au lieu de regler un feedback abstrait, on choisit la duree
//    (en secondes) au bout de laquelle la traine de l'echo a quasiment disparu
//    (-60 dB). Le gain de feedback est calcule automatiquement a partir de cette
//    duree et du temps de delai (formule type RT60).
//  - "Ping-pong" : le feedback est croise entre les deux canaux, donc les echos
//    rebondissent gauche -> droite -> gauche dans l'image stereo.
//  - "Damp" : un filtre passe-bas dans la boucle de feedback assombrit peu a peu
//    les repetitions (caractere delay analogique / bande).
class DelayEngine
{
public:
    DelayEngine();

    void prepare (double sampleRate, int blockSize, int numChannels);
    void reset();

    void setDelayMs (float ms);
    void setReleaseSeconds (float seconds);
    void setDamp (float amount01);
    void setPingPong (bool shouldPingPong);

    // Traite le buffer en place (le remplace par le signal d'echo seul = wet).
    void process (juce::AudioBuffer<float>& buffer);

    static constexpr float maxDelayMs = 2000.0f;

    // Gain de feedback par repetition pour un couple (delai, release) donne.
    // Partage avec l'UI (FIG. 1 / FIG. 2) : une seule source de verite pour
    // la geometrie tracee. Meme formule RT60 que le moteur.
    static float feedbackGainFor (float delayMs, float releaseSec) noexcept
    {
        const float delaySec = juce::jmax (0.001f, delayMs * 0.001f);
        const float g = std::pow (10.0f, -3.0f * delaySec / juce::jmax (0.05f, releaseSec));
        return juce::jlimit (0.0f, 0.98f, g);   // < 1 pour ne jamais s'auto-osciller
    }

    // Temps de delai reellement applique (lisse), publie pour l'UI.
    // Lecture sans verrou, jamais bloquante.
    float getUiDelayMs() const noexcept { return uiDelayMs.load (std::memory_order_relaxed); }

private:
    float feedbackGain() const;

    double sampleRate = 44100.0;
    int    numCh = 2;

    float delayMs    = 350.0f;
    float releaseSec = 0.8f;
    float damp       = 0.3f;
    bool  pingPong   = true;

    juce::SmoothedValue<float> delaySamples;   // lisse les changements de temps (glisse facon bande)

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 1 << 18 };

    // Etats des passe-bas (un par canal) dans la boucle de feedback.
    float dampStateL = 0.0f;
    float dampStateR = 0.0f;

    std::atomic<float> uiDelayMs { 350.0f };   // copie du temps lisse pour l'affichage
};

}
