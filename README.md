# Vocoduh

**Vocoduh** is a real-time FFT-based vocoder plugin built in **JUCE/C++**. It uses a vocal/modulator input to shape the frequency content of a sidechain carrier signal, creating a classic robotic vocoder effect with adjustable band count, envelope smoothing, unvoiced noise, and spectral enhancement controls.

This project was built to explore real-time spectral processing, overlap-add reconstruction, and sidechain routing.

## Features

* Real-time FFT vocoder processing
* Sidechain carrier input
* Adjustable vocoder band count: 8, 16, 24, 32, 48, or 64 bands
* Mel-spaced frequency bands
* Attack and release smoothing for band envelopes
* Unvoiced noise control for breathier consonants
* Spectral enhancement control
* Output gain control
* Custom JUCE UI with rotary controls and live band-level display
* Kaiser-windowed FFT analysis and overlap-add reconstruction

## How It Works

Vocoduh analyzes the modulator signal, usually a voice, in the frequency domain using a 1024-point FFT. The spectrum is divided into mel-spaced frequency bands. For each band, the plugin measures the average magnitude of the modulator and smooths it using attack and release coefficients.

The carrier signal is also transformed into the frequency domain. Each carrier band is then shaped by the corresponding smoothed modulator envelope. This transfers the vocal articulation of the modulator onto the timbre of the carrier.

After spectral shaping, the signal is reconstructed with an inverse FFT and overlap-add synthesis.

## Signal Flow

1. Main input receives the modulator signal, usually vocals.
2. Sidechain input receives the carrier signal, such as a synth, pad, or noise source.
3. Both signals are windowed and transformed using FFT.
4. The modulator spectrum is divided into mel-spaced bands.
5. The average magnitude of each modulator band is calculated.
6. Attack/release smoothing is applied to each band envelope.
7. The carrier spectrum is shaped by the smoothed modulator envelopes.
8. Optional unvoiced noise and spectral enhancement are blended in.
9. The processed frame is reconstructed using IFFT and overlap-add.
10. The output is sent to the plugin’s stereo output.

## Controls

| Control  | Description                                                                                        |
| -------- | -------------------------------------------------------------------------------------------------- |
| Output   | Controls the final output level                                                                    |
| Attack   | Controls how quickly each band envelope responds when the modulator gets louder                    |
| Release  | Controls how quickly each band envelope falls when the modulator gets quieter                      |
| Unvoiced | Adds noise into higher bands for breathier consonant-like articulation                             |
| Enhance  | Blends the carrier spectrum toward a flatter magnitude response for a more pronounced vocoder tone |
| Bands    | Selects the number of vocoder bands: 8, 16, 24, 32, 48, or 64                                      |

## Technical Details

* Framework: JUCE
* Language: C++
* FFT size: 1024 samples
* Hop size: 256 samples
* Overlap: 4x
* Window: Kaiser window
* Band spacing: Mel scale
* Processing style: Real-time STFT vocoder with overlap-add reconstruction
* Plugin I/O: Stereo main input, stereo sidechain input, stereo output
