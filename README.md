# Vocoduh

I built an FFT-based vocoder plugin with the JUCE framework in C++ based on Ableton's stock vocoder plugin. It takes two audio inputs, a vocal/modulator signal and a sidechain carrier signal, to create a robotic vocoder effect.

## GUI (She's so cute I know):

<img width="602" height="470" alt="image" src="https://github.com/user-attachments/assets/b59c633b-f937-4eab-8d8f-b979635dbc4f" />


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
