#include "EchoPlot.h"
#include "ManualStyle.h"
#include "DelayEngine.h"

namespace delaymxa
{

namespace
{
    constexpr float kDbTop = 6.0f;
    constexpr float kDbBot = -60.0f;

    float xForTime (juce::Rectangle<float> r, float t, float windowSec)
    {
        return r.getX() + 8.0f + (t / windowSec) * (r.getWidth() - 16.0f);
    }

    float yForDb (juce::Rectangle<float> r, float db)
    {
        const float t = (kDbTop - db) / (kDbTop - kDbBot);
        return r.getY() + t * r.getHeight();
    }

    // Pas de graduation "propre" (1/2/5 x 10^k) pour l'axe du temps.
    float niceStep (float raw)
    {
        const float mag  = std::pow (10.0f, std::floor (std::log10 (raw)));
        const float norm = raw / mag;
        if (norm <= 1.0f) return mag;
        if (norm <= 2.0f) return 2.0f * mag;
        if (norm <= 5.0f) return 5.0f * mag;
        return 10.0f * mag;
    }

    juce::String timeLabel (float t, bool useMs, float step)
    {
        if (useMs)
            return juce::String (juce::roundToInt (t * 1000.0f));
        return step < 1.0f ? juce::String (t, 1) : juce::String (juce::roundToInt (t));
    }
}

EchoPlot::EchoPlot (DelayProcessor& proc)
    : processor (proc)
{
    auto& apvts = processor.getAPVTS();
    release  = apvts.getRawParameterValue ("release");
    mix      = apvts.getRawParameterValue ("mix");
    pingPong = apvts.getRawParameterValue ("pingPong");

    setInterceptsMouseClicks (false, false);
}

void EchoPlot::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    auto caption = full.removeFromBottom (16.0f);
    auto box = full;

    // Valeurs reelles du moteur : temps lisse publie par la ligne a retard,
    // gain de feedback recalcule par la meme formule RT60.
    const float delayMs  = juce::jlimit (1.0f, DelayEngine::maxDelayMs, processor.getDelayMsLive());
    const float delaySec = delayMs * 0.001f;
    const float relSec   = release->load();
    const float mixV     = mix->load();
    const bool  isPP     = pingPong->load() > 0.5f;
    const float fbGain   = DelayEngine::feedbackGainFor (delayMs, relSec);

    // Fenetre de temps : assez pour lire la traine (-60 dB) et plusieurs echos.
    const float windowSec = juce::jmax (delaySec * 3.2f, delaySec + relSec);
    const bool  useMs     = windowSec < 1.0f;

    // --- Grille --------------------------------------------------------------
    const float step = niceStep (windowSec / 6.5f);
    g.setColour (palette::inkFaint);
    for (float t = step; t < windowSec * 0.995f; t += step)
        g.drawVerticalLine ((int) xForTime (box, t, windowSec), box.getY() + 1.0f, box.getBottom() - 1.0f);

    for (float db = 0.0f; db > kDbBot; db -= 12.0f)
    {
        g.setColour (db == 0.0f ? palette::inkMid.withAlpha (0.65f) : palette::inkFaint);
        g.drawHorizontalLine ((int) yForDb (box, db), box.getX() + 1.0f, box.getRight() - 1.0f);
    }

    const float baseY = box.getBottom() - 1.0f;

    // --- Guide de decroissance RT60 ------------------------------------------
    // Droite (en dB) du premier echo jusqu'a -60 dB, 'release' secondes plus
    // tard : c'est le parametre Release rendu visible.
    const float level1 = mixV > 1.0e-4f ? 20.0f * std::log10 (mixV) : kDbBot - 1.0f;
    if (level1 > kDbBot)
    {
        const float tBot = delaySec + relSec * (level1 - kDbBot) / 60.0f;
        const float tEnd = juce::jmin (windowSec, tBot);
        if (tEnd > delaySec)
        {
            const float dbEnd = level1 - 60.0f * (tEnd - delaySec) / relSec;
            g.setColour (palette::spot.withAlpha (0.35f));
            g.drawLine (xForTime (box, delaySec, windowSec), yForDb (box, level1),
                        xForTime (box, tEnd, windowSec),     yForDb (box, dbEnd), 1.0f);
        }
    }

    // --- Batons : la vraie reponse impulsionnelle -----------------------------
    auto stemL = [&] (float t, float db)   // canal gauche : spot, plein, point rempli
    {
        const float sx = xForTime (box, t, windowSec);
        const float sy = yForDb (box, juce::jmax (db, kDbBot));
        g.setColour (palette::spot);
        g.drawLine (sx, baseY, sx, sy, 1.6f);
        g.fillEllipse (sx - 2.2f, sy - 2.2f, 4.4f, 4.4f);
    };

    auto stemR = [&] (float t, float db)   // canal droit : encre, tirets, anneau
    {
        const float sx = xForTime (box, t, windowSec);
        const float sy = yForDb (box, juce::jmax (db, kDbBot));
        g.setColour (palette::ink.withAlpha (0.85f));
        const float dashes[] = { 4.0f, 3.0f };
        g.drawDashedLine ({ { sx, baseY }, { sx, sy } }, dashes, 2, 1.1f);
        g.drawEllipse (sx - 2.4f, sy - 2.4f, 4.8f, 4.8f, 1.1f);
    };

    // Impulsion seche a t=0, niveau (1-mix), sur les deux canaux : encre,
    // marqueur carre ouvert.
    if (mixV < 0.9999f)
    {
        const float dryDb = 20.0f * std::log10 (1.0f - mixV);
        if (dryDb > kDbBot)
        {
            const float sx = xForTime (box, 0.0f, windowSec);
            const float sy = yForDb (box, dryDb);
            g.setColour (palette::ink);
            g.drawLine (sx, baseY, sx, sy, 1.2f);
            g.drawRect (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ sx, sy }), 1.1f);
        }
    }

    if (level1 > kDbBot)
    {
        // Nombre d'echos au-dessus de -60 dB dans la fenetre ; si le peigne
        // devient trop dense pour l'ecran, on sous-echantillonne l'affichage
        // (pas impair en ping-pong pour continuer d'alterner les canaux).
        const int maxK = (int) std::floor (windowSec / delaySec);
        int stride = juce::jmax (1, (maxK + 399) / 400);
        if (isPP && stride > 1 && (stride % 2) == 0)
            ++stride;

        const float dbPerEcho = fbGain > 1.0e-6f ? 20.0f * std::log10 (fbGain) : -1.0e9f;

        for (int k = 1; k <= maxK; k += stride)
        {
            const float t  = (float) k * delaySec;
            const float db = level1 + (float) (k - 1) * dbPerEcho;
            if (db < kDbBot || t > windowSec)
                break;

            if (! isPP)          stemL (t, db);   // canaux identiques : un seul jeu de batons
            else if (k % 2 == 1) stemL (t, db);   // impair -> gauche
            else                 stemR (t, db);   // pair -> droite
        }
    }

    // Legende L / R / DRY en haut a gauche, sur cartouche film : la ligne de
    // reference 0 dB s'interrompt sous elle au lieu de la traverser. Peinte
    // AVANT les echelles, pour que le cartouche "0 dB" s'imprime entier.
    {
        float lx = box.getX() + 34.0f;
        const float ly = box.getY() + 12.0f;

        g.setColour (palette::film);
        g.fillRect (juce::Rectangle<float> (box.getX() + 30.0f, ly - 7.0f,
                                            isPP ? 130.0f : 104.0f, 14.0f));
        g.setFont (fonts::mono (9.0f));

        g.setColour (palette::spot);
        g.drawLine (lx, ly, lx + 14.0f, ly, 1.6f);
        g.setColour (palette::inkMid);
        g.drawText (isPP ? "L" : "L+R",
                    juce::Rectangle<float> (26.0f, 10.0f).withPosition (lx + 18.0f, ly - 5.0f),
                    juce::Justification::centredLeft);
        lx += isPP ? 40.0f : 54.0f;

        if (isPP)
        {
            g.setColour (palette::ink.withAlpha (0.85f));
            g.drawLine (lx, ly, lx + 4.0f, ly, 1.1f);
            g.drawLine (lx + 7.0f, ly, lx + 11.0f, ly, 1.1f);
            g.setColour (palette::inkMid);
            g.drawText ("R", juce::Rectangle<float> (12.0f, 10.0f).withPosition (lx + 15.0f, ly - 5.0f),
                        juce::Justification::centredLeft);
            lx += 40.0f;
        }

        g.setColour (palette::ink);
        g.drawRect (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ lx + 7.0f, ly }), 1.1f);
        g.setColour (palette::inkMid);
        g.drawText ("DRY", juce::Rectangle<float> (30.0f, 10.0f).withPosition (lx + 16.0f, ly - 5.0f),
                    juce::Justification::centredLeft);
    }

    // --- Echelles -------------------------------------------------------------
    // Chaque chiffre est pose sur un cartouche film : ni le grain ni la grille
    // ne peuvent corrompre une valeur que le regard doit pouvoir croire.
    auto drawFigure = [&g] (const juce::String& text, juce::Point<float> anchor,
                            juce::Justification just)
    {
        const auto font = fonts::mono (9.0f);
        const float tw  = juce::GlyphArrangement::getStringWidth (font, text);
        auto area = juce::Rectangle<float> (tw + 6.0f, 11.0f).withCentre (anchor);
        if (just.testFlags (juce::Justification::left))
            area.setX (anchor.x - 3.0f);
        else if (just.testFlags (juce::Justification::right))
            area.setX (anchor.x - tw - 3.0f);

        g.setColour (palette::film);
        g.fillRect (area);
        g.setFont (font);
        g.setColour (palette::inkMid);
        g.drawText (text, area, juce::Justification::centred);
    };

    for (float t = step; t < windowSec * 0.995f; t += step)
    {
        const float tx = xForTime (box, t, windowSec);
        if (tx > box.getRight() - 26.0f)   // laisse la place au cartouche d'unite
            break;
        drawFigure (timeLabel (t, useMs, step),
                    { tx, box.getBottom() - 8.0f }, juce::Justification::centred);
    }

    // Chiffres en retrait du bord : le baton sec a t=0 reste d'un seul tenant.
    for (float db = -12.0f; db > kDbBot; db -= 12.0f)
        drawFigure (juce::String ((int) db),
                    { box.getX() + 20.0f, yForDb (box, db) - 6.0f },
                    juce::Justification::left);

    // Designation des unites, une fois par echelle, convention de plan
    // (le "dB" est porte par la ligne de reference 0, cartouche sous la ligne).
    drawFigure ("0 dB", { box.getX() + 20.0f, yForDb (box, 0.0f) + 8.0f }, juce::Justification::left);
    drawFigure (useMs ? "ms" : "s", { box.getRight() - 6.0f, box.getBottom() - 8.0f },
                juce::Justification::right);

    // Tallies a la machine a ecrire : release reel et gain par repetition.
    // Chacun sur son cartouche film, comme tout chiffre de la feuille.
    {
        g.setFont (fonts::mono (10.0f));
        auto tallyLine = [&] (const juce::String& name, const juce::String& value, float y)
        {
            auto area = juce::Rectangle<float> (120.0f, 12.0f)
                            .withPosition (box.getRight() - 126.0f, y);
            const float vw = juce::GlyphArrangement::getStringWidth (fonts::mono (10.0f), value);
            g.setColour (palette::film);
            g.fillRect (juce::Rectangle<float> (area.getX() - 4.0f, y - 1.0f, 46.0f + vw + 10.0f, 14.0f));
            g.setColour (palette::inkMid);
            g.drawText (name, area.removeFromLeft (42.0f), juce::Justification::centredLeft);
            g.setColour (palette::ink);
            g.drawText (value, area, juce::Justification::centredLeft);
        };
        tallyLine ("RT60", juce::String (relSec, 2) + " s", box.getY() + 6.0f);
        tallyLine ("g", juce::String (fbGain, fbGain < 0.1f ? 3 : 2), box.getY() + 20.0f);
    }

    // --- Cadre + legende de figure --------------------------------------------
    g.setColour (palette::ink);
    g.drawRect (box, 1.0f);

    const juce::String cap = "FIG. 1 - ECHO PATTERN, IMPULSE RESPONSE";
    g.setFont (fonts::lettering (10.0f));
    g.setColour (palette::inkMid);
    g.drawText (cap, caption.withTrimmedTop (4.0f), juce::Justification::bottomLeft);

    const float capW = juce::GlyphArrangement::getStringWidth (fonts::lettering (10.0f), cap);
    g.setColour (palette::inkFaint);
    g.drawHorizontalLine ((int) (caption.getBottom() - 3.0f),
                          caption.getX() + capW + 10.0f, caption.getRight());
}

}
