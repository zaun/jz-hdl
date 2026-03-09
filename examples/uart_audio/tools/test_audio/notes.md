
  Test audio files generated in tools/test_audio/:
  - tone_100hz.wav → bar 9
  - tone_440hz.wav → bar 41
  - tone_1000hz.wav → bar 59
  - sweep_65_2500.wav → should sweep bars left to right over 15 seconds
  - chord_3tone.wav → should show 3 distinct peaks at bars 24, 41, 59

  Best test order:
  1. tone_440hz.wav --loop — should see one bright peak around bar 41
  2. sweep_65_2500.wav --loop — peak should move smoothly left to right
  3. ode_to_joy.wav --loop — should see melodic movement in the mid-range bars
  