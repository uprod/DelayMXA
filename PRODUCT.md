# Product

<!-- impeccable:product-schema 1 -->

## Platform

desktop

Native JUCE audio plugin (AU/VST3/Standalone, macOS 11+). Sibling of the MXA suite; the family design authority is `../PhaserMXA/DESIGN.md` and the family product context is `../PhaserMXA/PRODUCT.md` (users, distribution, brand rules — all shared).

## Product Purpose

A regenerative stereo delay. Success: a stranger dials a musical echo in seconds and understands the ping-pong routing at a glance.

## Capabilities and Constraints

- Exactly five parameters: `time` (1–2000 ms), `release` (RT60-style tail 0.05–12 s; feedback gain derived as g = 10^(−3·T/R)), `damp` (one-pole LP in the loop), `mix`, `pingPong` (bool; mono-summed input, L↔R cross-fed returns).
- UI truth taps: atomic smoothed delay time (`DelayEngine::uiDelayMs`); shared static `feedbackGainFor()` is the single source of truth for engine, FIG. 1 stems, and the printed g tally — never fork it.
- Editor: Service Manual family sheet, 820×470, spot ink teal #2FD6B0, DWG NO. MXA-DL-01.

## Brand Commitments

Inherits the family's: MXAudio, "BY MESCALINA" credit, one spot ink per sibling (Delay = teal).

## Evidence on Hand

Working DSP (`Source/DelayEngine.*`); review captures in `.impeccable/review/`. No users/testimonials — nothing may be fabricated.
