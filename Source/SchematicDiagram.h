#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace delaymxa
{

// FIG. 2 - Le chemin du signal dessine comme dans le manuel : IN, deux lignes
// a retard L/R (le temps imprime dans les blocs EST le temps lisse reel),
// passe-bas d'amortissement dans la boucle (la pente du glyphe EST le damp),
// retours de feedback dont l'epaisseur EST le vrai gain RT60 — croises quand
// le ping-pong est enclenche, paralleles sinon —, rails dry/wet ponderes par
// le mix. La quantite est dessinee en geometrie : le schema est la valeur.
class SchematicDiagram : public juce::Component
{
public:
    explicit SchematicDiagram (DelayProcessor&);

    void paint (juce::Graphics&) override;

private:
    DelayProcessor& processor;

    std::atomic<float>* release  = nullptr;
    std::atomic<float>* damp     = nullptr;
    std::atomic<float>* mix      = nullptr;
    std::atomic<float>* pingPong = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SchematicDiagram)
};

}
