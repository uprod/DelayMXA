#include "SchematicDiagram.h"
#include "ManualStyle.h"
#include "DelayEngine.h"

namespace delaymxa
{

namespace
{
    // Epaisseur de trait proportionnelle a une quantite 0..1 : la geometrie
    // porte la valeur, jamais la couleur seule.
    float weightFor (float amount01)
    {
        return 0.7f + 2.4f * juce::jlimit (0.0f, 1.0f, amount01);
    }

    void drawArrowHead (juce::Graphics& g, juce::Point<float> tip, juce::Point<float> dir, float size)
    {
        dir = dir / (dir.getDistanceFromOrigin() + 1.0e-6f);
        const juce::Point<float> n (-dir.y, dir.x);
        juce::Path p;
        p.addTriangle (tip, tip - dir * size + n * (size * 0.55f),
                             tip - dir * size - n * (size * 0.55f));
        g.fillPath (p);
    }

    void drawDashedLine (juce::Graphics& g, juce::Line<float> line, float thickness)
    {
        const float dashes[] = { 3.0f, 3.0f };
        g.drawDashedLine (line, dashes, 2, thickness);
    }

    // Etiquette imprimee qui interrompt le trait qu'elle chevauche : on pose
    // un cartouche couleur film derriere le texte, comme sur un vrai plan.
    void drawLabelOverLine (juce::Graphics& g, const juce::String& text,
                            juce::Rectangle<float> area, juce::Justification just)
    {
        const float tw = juce::GlyphArrangement::getStringWidth (fonts::lettering (9.0f), text);
        auto knockout = area.withSizeKeepingCentre (tw + 10.0f, area.getHeight());
        if (just.testFlags (juce::Justification::left))
            knockout.setX (area.getX() - 5.0f);
        else if (just.testFlags (juce::Justification::right))
            knockout.setX (area.getRight() - tw - 5.0f);

        g.setColour (palette::film);
        g.fillRect (knockout);
        g.setFont (fonts::lettering (9.0f));
        g.setColour (palette::inkMid);
        g.drawText (text, area, just);
    }

    // Croix de sommateur dans un cercle (jonction "+" du schema).
    void drawSummingNode (juce::Graphics& g, juce::Point<float> c, float r)
    {
        g.setColour (palette::film);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (palette::ink);
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.1f);
        g.drawLine (c.x - r * 0.5f, c.y, c.x + r * 0.5f, c.y, 1.0f);
        g.drawLine (c.x, c.y - r * 0.5f, c.x, c.y + r * 0.5f, 1.0f);
    }

    // Bloc passe-bas d'amortissement : le coude du glyphe EST la valeur de damp
    // (damp fort = coupure precoce et pente visible).
    void drawLowpassBlock (juce::Graphics& g, juce::Rectangle<float> block, float damp01)
    {
        g.setColour (palette::film);
        g.fillRect (block);
        g.setColour (palette::ink);
        g.drawRect (block, 1.1f);

        g.setFont (fonts::lettering (8.5f));
        g.drawText ("LP", block.withTrimmedLeft (3.0f).withWidth (14.0f),
                    juce::Justification::centredLeft);

        const float gx0 = block.getX() + 16.0f;
        const float gx1 = block.getRight() - 4.0f;
        const float cy  = block.getCentreY();
        const float corner = gx0 + (1.0f - juce::jlimit (0.0f, 1.0f, damp01)) * (gx1 - gx0 - 4.0f);

        juce::Path lp;
        lp.startNewSubPath (gx0, cy - 2.0f);
        lp.lineTo (corner, cy - 2.0f);
        lp.lineTo (gx1, cy + 3.0f);
        g.setColour (palette::inkMid);
        g.strokePath (lp, juce::PathStrokeType (1.0f));
    }
}

SchematicDiagram::SchematicDiagram (DelayProcessor& proc)
    : processor (proc)
{
    auto& apvts = processor.getAPVTS();
    release  = apvts.getRawParameterValue ("release");
    damp     = apvts.getRawParameterValue ("damp");
    mix      = apvts.getRawParameterValue ("mix");
    pingPong = apvts.getRawParameterValue ("pingPong");

    setInterceptsMouseClicks (false, false);
}

void SchematicDiagram::paint (juce::Graphics& g)
{
    const float w = (float) getWidth();
    auto caption  = getLocalBounds().toFloat().removeFromBottom (16.0f);

    const float mixV   = mix->load();
    const float dampV  = damp->load();
    const bool  isPP   = pingPong->load() > 0.5f;
    const float msLive = juce::jlimit (1.0f, DelayEngine::maxDelayMs, processor.getDelayMsLive());
    const float fbGain = DelayEngine::feedbackGainFor (msLive, release->load());
    const bool  fbDead = fbGain < 0.005f;

    // Rangs horizontaux du schema.
    const float railL = 24.0f;   // ligne a retard gauche
    const float retA  = 40.0f;   // retour de feedback issu du canal gauche
    const float retB  = 52.0f;   // retour de feedback issu du canal droit
    const float railR = 68.0f;   // ligne a retard droite
    const float dryY  = 88.0f;   // rail dry
    const float inY   = 46.0f;

    // Colonnes.
    const float inX     = 12.0f;
    const float branchX = 36.0f;
    const float sumX    = 88.0f;
    const float blockX0 = 150.0f, blockW = 96.0f, blockH = 18.0f;
    const float blockX1 = blockX0 + blockW;
    const float tapX    = 286.0f;
    const float dropX   = 112.0f;             // descente du retour croise (ping-pong)
    const float mixX    = juce::jmax (560.0f, w * 0.855f);
    const float outX    = w - 16.0f;

    const float wetW = weightFor (mixV);
    const float dryW = weightFor (1.0f - mixV);
    const float fbW  = weightFor (fbGain / 0.98f);

    const juce::Rectangle<float> blockL (blockX0, railL - blockH * 0.5f, blockW, blockH);
    const juce::Rectangle<float> blockR (blockX0, railR - blockH * 0.5f, blockW, blockH);
    const juce::Rectangle<float> lpL (174.0f, retA - 5.0f, 32.0f, 10.0f);
    const juce::Rectangle<float> lpR (174.0f, retB - 5.0f, 32.0f, 10.0f);

    // --- Entree et derivation dry ---------------------------------------------
    g.setColour (palette::ink);
    g.drawEllipse (inX - 3.0f, inY - 3.0f, 6.0f, 6.0f, 1.1f);                 // borne IN
    g.drawLine (inX + 3.0f, inY, branchX, inY, 1.2f);
    g.fillEllipse (branchX - 2.2f, inY - 2.2f, 4.4f, 4.4f);                   // noeud de derivation

    // Montee vers la ligne gauche (toujours alimentee).
    g.drawLine (branchX, inY, branchX, railL, 1.2f);
    g.drawLine (branchX, railL, sumX - 7.0f, railL, 1.2f);
    drawArrowHead (g, { sumX - 7.0f, railL }, { 1.0f, 0.0f }, 6.0f);

    // Descente dry (et alimentation de la ligne droite hors ping-pong).
    g.setColour (palette::ink.withAlpha (0.9f));
    g.drawLine (branchX, inY, branchX, dryY, dryW * 0.75f + 0.4f);
    g.drawLine (branchX, dryY, mixX - 22.0f, dryY, dryW);                     // rail dry
    g.drawLine (mixX - 22.0f, dryY, mixX - 22.0f, inY, dryW);                 // remontee vers le mix
    g.drawLine (mixX - 22.0f, inY, mixX - 8.0f, inY, dryW);
    drawArrowHead (g, { mixX - 8.0f, inY }, { 1.0f, 0.0f }, 6.0f);

    if (! isPP)
    {
        g.setColour (palette::ink);
        g.fillEllipse (branchX - 2.2f, railR - 2.2f, 4.4f, 4.4f);             // jonction vers la ligne R
        g.drawLine (branchX, railR, sumX - 7.0f, railR, 1.2f);
        drawArrowHead (g, { sumX - 7.0f, railR }, { 1.0f, 0.0f }, 6.0f);
    }

    g.setFont (fonts::lettering (9.0f));
    g.setColour (palette::inkMid);
    g.drawText ("IN", juce::Rectangle<float> (24.0f, 10.0f).withPosition (inX - 8.0f, inY - 17.0f),
                juce::Justification::centredLeft);
    g.drawText ("DRY", juce::Rectangle<float> (30.0f, 10.0f).withPosition (branchX + 8.0f, dryY - 13.0f),
                juce::Justification::centredLeft);

    // --- Lignes a retard --------------------------------------------------------
    // Sommateur d'entree gauche -> bloc DELAY L.
    g.setColour (palette::ink);
    g.drawLine (sumX + 7.0f, railL, blockX0, railL, 1.2f);
    drawArrowHead (g, { blockX0, railL }, { 1.0f, 0.0f }, 6.0f);

    if (! isPP)
    {
        g.drawLine (sumX + 7.0f, railR, blockX0, railR, 1.2f);
        drawArrowHead (g, { blockX0, railR }, { 1.0f, 0.0f }, 6.0f);
    }

    // Sortie des blocs vers les noeuds de reprise (taps).
    g.drawLine (blockX1, railL, tapX, railL, 1.2f);
    g.drawLine (blockX1, railR, tapX, railR, 1.2f);
    g.fillEllipse (tapX - 2.2f, railL - 2.2f, 4.4f, 4.4f);
    g.fillEllipse (tapX - 2.2f, railR - 2.2f, 4.4f, 4.4f);

    // Blocs DELAY : le temps imprime est le temps reellement applique (lisse).
    const juce::String msText = (msLive < 100.0f ? juce::String (msLive, 1)
                                                 : juce::String (juce::roundToInt (msLive))) + " ms";
    auto drawDelayBlock = [&] (juce::Rectangle<float> block, const juce::String& name)
    {
        g.setColour (palette::film);
        g.fillRect (block);
        g.setColour (palette::ink);
        g.drawRect (block, 1.2f);
        g.setFont (fonts::lettering (9.0f));
        g.drawText (name, block.withTrimmedBottom (8.0f), juce::Justification::centred);
        g.setFont (fonts::mono (8.0f));
        g.setColour (palette::inkMid);
        g.drawText (msText, block.withTrimmedTop (9.0f), juce::Justification::centred);
    };
    drawDelayBlock (blockL, "DELAY L");
    drawDelayBlock (blockR, "DELAY R");

    // Commande de temps commune : les deux lignes suivent le meme parametre
    // (un seul lisseur dans le moteur) — trait de commande tirete en encre spot.
    {
        const float tX = 160.0f;
        g.setColour (palette::spot.withAlpha (0.6f));
        drawDashedLine (g, { { tX, blockL.getBottom() }, { tX, blockR.getY() } }, 1.0f);
        g.setColour (palette::spot);
        g.fillEllipse (tX - 1.6f, blockL.getBottom() - 1.6f, 3.2f, 3.2f);
        g.fillEllipse (tX - 1.6f, blockR.getY() - 1.6f, 3.2f, 3.2f);
    }

    // --- Boucles de feedback : l'epaisseur EST le vrai gain RT60 ---------------
    auto fbLine = [&] (juce::Line<float> line)
    {
        if (fbDead) drawDashedLine (g, line, 0.7f);
        else        g.drawLine (line, fbW);
    };

    g.setColour (palette::ink);
    if (isPP)
    {
        // Croisement reel du moteur : le retour gauche entre dans la ligne
        // droite, le retour droit revient au sommateur gauche.
        fbLine ({ { tapX, railL }, { tapX, retA } });
        fbLine ({ { tapX, retA }, { dropX, retA } });
        fbLine ({ { dropX, retA }, { dropX, railR } });
        fbLine ({ { dropX, railR }, { blockX0, railR } });
        drawArrowHead (g, { blockX0, railR }, { 1.0f, 0.0f }, 6.0f);

        fbLine ({ { tapX, railR }, { tapX, retB } });
        fbLine ({ { tapX, retB }, { sumX, retB } });
        fbLine ({ { sumX, retB }, { sumX, railL + 7.0f } });
        drawArrowHead (g, { sumX, railL + 7.0f }, { 0.0f, -1.0f }, 6.0f);
    }
    else
    {
        // Hors ping-pong : chaque canal reboucle sur son propre sommateur.
        fbLine ({ { tapX, railL }, { tapX, retA } });
        fbLine ({ { tapX, retA }, { sumX, retA } });
        fbLine ({ { sumX, retA }, { sumX, railL + 7.0f } });
        drawArrowHead (g, { sumX, railL + 7.0f }, { 0.0f, -1.0f }, 6.0f);

        fbLine ({ { tapX, railR }, { tapX, retB } });
        fbLine ({ { tapX, retB }, { sumX, retB } });
        fbLine ({ { sumX, retB }, { sumX, railR - 7.0f } });
        drawArrowHead (g, { sumX, railR - 7.0f }, { 0.0f, 1.0f }, 6.0f);
    }

    // Passe-bas d'amortissement dans chaque retour (poses sur leurs traits).
    drawLowpassBlock (g, lpL, dampV);
    drawLowpassBlock (g, lpR, dampV);

    // L'etiquette de routage interrompt le trait du retour, comme sur un plan —
    // chaque rail porte son nom : la boucle droite recoit la meme mention.
    drawLabelOverLine (g, isPP ? "CROSS-FEED" : "FEEDBACK",
                       juce::Rectangle<float> (80.0f, 10.0f).withCentre ({ 244.0f, retA }),
                       juce::Justification::centred);
    drawLabelOverLine (g, isPP ? "CROSS-FEED" : "FEEDBACK",
                       juce::Rectangle<float> (80.0f, 10.0f).withCentre ({ 244.0f, retB }),
                       juce::Justification::centred);

    // --- Sommateurs -------------------------------------------------------------
    drawSummingNode (g, { sumX, railL }, 7.0f);
    if (! isPP)
        drawSummingNode (g, { sumX, railR }, 7.0f);

    // --- Rails wet vers le sommateur de mix --------------------------------------
    g.setColour (palette::ink.withAlpha (0.9f));
    g.drawLine (tapX, railL, mixX, railL, wetW);
    g.drawLine (mixX, railL, mixX, inY - 8.0f, wetW);
    drawArrowHead (g, { mixX, inY - 8.0f }, { 0.0f, 1.0f }, 6.0f);
    g.drawLine (tapX, railR, mixX, railR, wetW);
    g.drawLine (mixX, railR, mixX, inY + 8.0f, wetW);
    drawArrowHead (g, { mixX, inY + 8.0f }, { 0.0f, -1.0f }, 6.0f);

    g.setFont (fonts::lettering (9.0f));
    g.setColour (palette::inkMid);
    g.drawText ("WET", juce::Rectangle<float> (30.0f, 10.0f).withPosition (mixX - 48.0f, railL - 13.0f),
                juce::Justification::centredRight);

    drawSummingNode (g, { mixX, inY }, 8.0f);
    g.drawText ("MIX", juce::Rectangle<float> (30.0f, 10.0f).withPosition (mixX + 12.0f, inY - 22.0f),
                juce::Justification::centredLeft);

    // --- Sortie -------------------------------------------------------------------
    g.setColour (palette::ink);
    g.drawLine (mixX + 8.0f, inY, outX - 3.0f, inY, 1.4f);
    g.fillEllipse (outX - 3.0f, inY - 3.0f, 6.0f, 6.0f);                      // borne OUT
    g.setColour (palette::inkMid);
    g.drawText ("OUT", juce::Rectangle<float> (28.0f, 10.0f).withPosition (outX - 24.0f, inY - 17.0f),
                juce::Justification::centredRight);

    // --- Rappels de plan : amorces pointees -----------------------------------------
    // Le sommateur d'entree recoit la somme mono (L+R)/2 quand le ping-pong
    // est enclenche : c'est ce que fait reellement le moteur.
    if (isPP)
    {
        g.setColour (palette::inkMid);
        g.drawLine (102.0f, 9.0f, 61.0f, 23.0f, 0.7f);
        g.fillEllipse (60.0f - 1.6f, 24.0f - 1.6f, 3.2f, 3.2f);
        g.setFont (fonts::lettering (9.0f));
        g.drawText ("MONO SUM", juce::Rectangle<float> (74.0f, 10.0f).withPosition (104.0f, 3.0f),
                    juce::Justification::centredLeft);
    }

    // Les passe-bas portent leur nom, relie par une amorce pointee.
    {
        g.setColour (palette::inkMid);
        g.drawLine (318.0f, 80.0f, 205.0f, 58.5f, 0.7f);
        g.fillEllipse (204.0f - 1.6f, 57.5f - 1.6f, 3.2f, 3.2f);
        g.setFont (fonts::lettering (9.0f));
        g.drawText ("DAMP LP", juce::Rectangle<float> (64.0f, 10.0f).withPosition (322.0f, 75.0f),
                    juce::Justification::centredLeft);
    }

    // --- Legende de figure -----------------------------------------------------------
    const juce::String cap = isPP ? "FIG. 2 - SIGNAL PATH, PING-PONG CROSS-FEED"
                                  : "FIG. 2 - SIGNAL PATH, DUAL-LINE STEREO ECHO";
    g.setFont (fonts::lettering (10.0f));
    g.setColour (palette::inkMid);
    g.drawText (cap, caption.withTrimmedTop (4.0f), juce::Justification::bottomLeft);

    const float capW = juce::GlyphArrangement::getStringWidth (fonts::lettering (10.0f), cap);
    g.setColour (palette::inkFaint);
    g.drawHorizontalLine ((int) (caption.getBottom() - 3.0f),
                          caption.getX() + capW + 10.0f, caption.getRight());
}

}
