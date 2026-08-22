#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace delaymxa
{

// FIG. 1 - Motif d'echo, trace comme la figure d'un manuel technique.
// Ce n'est pas une illustration : les batons sont la vraie reponse
// impulsionnelle du delay — impulsion seche a t=0 au niveau (1-mix), echos a
// chaque multiple du temps de delai reellement applique (lisse), decroissant
// du vrai gain de feedback RT60 du moteur. En ping-pong les echos alternent
// gauche (trait plein spot) / droite (tirets encre) : jamais la couleur seule.
// Le repaint est pilote par le Timer de l'editeur (~30 Hz).
class EchoPlot : public juce::Component
{
public:
    explicit EchoPlot (DelayProcessor&);

    void paint (juce::Graphics&) override;

private:
    DelayProcessor& processor;

    std::atomic<float>* release  = nullptr;
    std::atomic<float>* mix      = nullptr;
    std::atomic<float>* pingPong = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoPlot)
};

}
