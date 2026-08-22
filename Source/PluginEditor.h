#pragma once

/*  IMPECCABLE DIRECTION CONTRACT — seed 5bcea053 (roll: assigned)
    THESIS: The panel IS the signal path — a service-manual schematic read as
    the circuit you hear; refuses knobs-on-a-metal-plate.
    OWN-WORLD: Diazo film negative — dark drafting film #17140F, pale ink
    #E6DCC2, spot teal #2FD6B0 (one spot ink per MXA sibling). Routed Gothic
    drafting lettering + Courier Prime figures, double sheet border, title
    block, FIG. captions.
    STORY: A producer reads the schematic, counts the echoes decaying in
    FIG. 1 at the real RT60 slope, and trusts every figure at a glance.
    FIRST VIEWPORT: Header + title block; FIG. 1 live echo-pattern impulse
    response full width; FIG. 2 dual delay-line signal path with weighted
    DRY/WET/FEEDBACK traces, damping LP blocks, cross-feed routing that
    redraws when PING-PONG flips; five schematic dials beneath.
    SIGNATURE: the echo comb — FIG. 1 stems and the printed block times ride
    the engine's real smoothed delay time on one 30 Hz clock; drag TIME and
    the whole drawing glides like the tape.
    FORM: Service Manual family template, adopted from PhaserMXA, seed 5bcea053.
    FINISH: unreviewed and undocumented is unfinished; this build ends with
    the finish review, the verdict, DESIGN.md, and every shipping raster
    carrying its provenance.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ManualStyle.h"
#include "EchoPlot.h"
#include "SchematicDiagram.h"

namespace delaymxa
{

class DelayEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit DelayEditor (DelayProcessor& proc);
    ~DelayEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    using APVTS   = juce::AudioProcessorValueTreeState;
    using SAttach = APVTS::SliderAttachment;

    struct Dial
    {
        juce::Slider slider;
        juce::Label  name;
        std::unique_ptr<SAttach> attachment;
    };

    void setupDial (Dial& d, const juce::String& labelText, const juce::String& paramID);
    void timerCallback() override;

    void drawSheetFrame (juce::Graphics& g);
    void drawHeader (juce::Graphics& g);

    DelayProcessor& processor;

    ManualLookAndFeel lookAndFeel;
    juce::Image       filmTexture;

    EchoPlot         plot;
    SchematicDiagram schematic;

    Dial timeDial, releaseDial, dampDial, pingPongSwitch, mixDial;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayEditor)
};

}
