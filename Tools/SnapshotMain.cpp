// Outil de capture hors-ecran pour la revue de design : instancie le
// processeur et l'editeur sans peripherique audio ni fenetre, fait tourner de
// vrais blocs (le lisseur de temps s'installe), puis peint l'editeur en 2x
// dans un PNG.
//   usage : DelayMXASnapshot <sortie.png> [alt]
//   "alt" : valeurs non par defaut (temps long, traine longue, damp fort,
//           ping-pong coupe -> variante "dual-line" de la FIG. 2)

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../Source/PluginProcessor.h"

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: DelayMXASnapshot <sortie.png> [alt]\n";
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;

    delaymxa::DelayProcessor proc;

    if (argc > 2 && juce::String (argv[2]) == "alt")
    {
        auto set = [&proc] (const char* id, float v01)
        {
            if (auto* p = proc.getAPVTS().getParameter (id))
                p->setValueNotifyingHost (v01);
        };
        set ("time",     0.85f);   // ~1.16 s
        set ("release",  0.90f);   // ~8.9 s de traine
        set ("damp",     0.70f);
        set ("mix",      0.60f);
        set ("pingPong", 0.00f);   // dual-line stereo (retours paralleles)
    }

    // De vrais blocs audio pour un temps lisse authentique (~0.4 s).
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int i = 0; i < 40; ++i)
    {
        buffer.clear();
        proc.processBlock (buffer, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    if (editor == nullptr)
        return 2;

    const int w = editor->getWidth();
    const int h = editor->getHeight();

    juce::Image img (juce::Image::ARGB, w * 2, h * 2, true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        editor->paintEntireComponent (g, true);
    }

    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    out.deleteFile();
    juce::FileOutputStream os (out);
    if (! os.openedOk())
        return 3;

    juce::PNGImageFormat().writeImageToStream (img, os);
    std::cout << "ecrit: " << out.getFullPathName() << " (" << w * 2 << "x" << h * 2 << ")\n";
    return 0;
}
