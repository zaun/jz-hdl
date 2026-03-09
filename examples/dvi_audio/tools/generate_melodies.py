#!/usr/bin/env python3
"""Generate 4 track binary files for the DVI audio example.

Format: 1500 x 64-bit big-endian entries per track (3 songs x 500 entries)
  [63:48] half-period (48 kHz sample ticks per half-cycle)
  [47:24] duration (samples at 48 kHz, 24 bits)
  [23:8]  gap (samples of silence at end of note, 16 bits)
  [7:0]   volume (unsigned 8-bit amplitude)

Song 1 (entries 0-499):    Ode to Joy
Song 2 (entries 500-999):  William Tell Overture (Finale)
Song 3 (entries 1000-1499): Entry of the Gladiators

Output files:
  track0.bin  - Melody (Soprano)
  track1.bin  - Alto harmony
  track2.bin  - Tenor
  track3.bin  - Bass

Half-period = 24000 / freq  (48 kHz sample rate, counted per sample tick)
"""

import struct
import os
import sys

# ---------- Note half-period table (48 kHz / (2 * freq)) ----------

# Octave 2
HP_G2  = 245   # 98.0 Hz
HP_A2  = 218   # 110.1 Hz
HP_B2  = 194   # 123.7 Hz

# Octave 3
HP_C3  = 184   # 130.4 Hz
HP_D3  = 164   # 146.3 Hz
HP_Eb3 = 154   # 155.6 Hz
HP_E3  = 146   # 164.4 Hz
HP_F3  = 137   # 175.2 Hz
HP_G3  = 122   # 196.7 Hz
HP_Ab3 = 116   # 207.7 Hz
HP_A3  = 109   # 220.2 Hz
HP_Bb3 = 103   # 233.1 Hz
HP_B3  = 97    # 247.4 Hz

# Octave 4
HP_C4  = 92    # 260.9 Hz
HP_Db4 = 87    # 276.0 Hz
HP_D4  = 82    # 292.7 Hz
HP_Eb4 = 77    # 311.7 Hz
HP_E4  = 73    # 328.8 Hz
HP_F4  = 69    # 347.8 Hz
HP_Gb4 = 65    # 369.2 Hz
HP_G4  = 61    # 393.4 Hz
HP_Ab4 = 58    # 414.0 Hz
HP_A4  = 55    # 436.4 Hz
HP_Bb4 = 52    # 462.0 Hz
HP_B4  = 49    # 489.8 Hz

# Octave 5
HP_C5  = 46    # 521.7 Hz
HP_Db5 = 43    # 558.1 Hz
HP_D5  = 41    # 585.4 Hz
HP_Eb5 = 39    # 615.4 Hz
HP_E5  = 37    # 648.6 Hz
HP_F5  = 34    # 705.9 Hz
HP_G5  = 31    # 774.2 Hz

# Rest (silence -- half_period=0 means no toggle, volume=0 means silent)
REST = 0

# ---------- Durations at 120 BPM (48 kHz samples) ----------

EIGHTH  = 12000    # ~0.25 sec
QUARTER = 24000    # ~0.50 sec
DOTQ    = 36000    # ~0.75 sec (dotted quarter)
HALF    = 48000    # ~1.00 sec
DOTH    = 72000    # ~1.50 sec (dotted half)
WHOLE   = 96000    # ~2.00 sec

# 1 bar = 4 quarters = 96000 samples

# ---------- Articulation ----------

GAP = 1500         # ~31 ms silence between notes
LGAP = 3000        # ~62 ms longer gap for phrase endings
SGAP = 800         # ~17 ms short gap for legato passages

# ---------- Helpers ----------

SENTINEL = 0xFFFFFFFFFFFFFFFF
SONG_DEPTH = 500     # entries per song
NUM_SONGS  = 3
TRACK_DEPTH = SONG_DEPTH * NUM_SONGS  # 1500 total entries per track


def pack_entry(hp, dur, gap, vol):
    """Pack a 64-bit entry: hp[63:48] dur[47:24] gap[23:8] vol[7:0]"""
    word = (hp << 48) | (dur << 24) | (gap << 8) | vol
    return struct.pack('>Q', word)


def n(hp, dur, gap=GAP, vol=180):
    """Shorthand for a note entry."""
    return (hp, dur, gap, vol)


def r(dur):
    """Rest entry."""
    return (REST, dur, 0, 0)


def write_song(notes):
    """Pack a single song's notes with sentinel padding to SONG_DEPTH."""
    assert len(notes) <= SONG_DEPTH, \
        f"Song has {len(notes)} notes, max is {SONG_DEPTH}"
    data = bytearray()
    for hp, dur, gap, vol in notes:
        data += pack_entry(hp, dur, gap, vol)
    for _ in range(SONG_DEPTH - len(notes)):
        data += struct.pack('>Q', SENTINEL)
    return data, len(notes)


def write_track(path, *songs):
    """Write a track binary file containing multiple songs."""
    data = bytearray()
    counts = []
    for song_notes in songs:
        song_data, count = write_song(song_notes)
        data += song_data
        counts.append(count)
    with open(path, 'wb') as f:
        f.write(data)
    return counts


def section_duration(notes):
    """Total duration of a note list in samples."""
    return sum(dur for _, dur, _, _ in notes)


def rest_for(dur_samples):
    """Create rests totaling dur_samples. Uses whole + half + quarter rests."""
    rests = []
    remaining = dur_samples
    while remaining >= WHOLE:
        rests.append(r(WHOLE))
        remaining -= WHOLE
    while remaining >= HALF:
        rests.append(r(HALF))
        remaining -= HALF
    while remaining >= QUARTER:
        rests.append(r(QUARTER))
        remaining -= QUARTER
    if remaining > 0:
        rests.append(r(remaining))
    return rests


# ======================================================================
# Ode to Joy - Extended 4-part arrangement in C major, 120 BPM
#
# Structure (progressive buildup, ~3 min):
#   Intro:    4 bars  - Bass pedal + tenor drone
#   Verse 1: 16 bars  - Melody solo (pp)
#   Verse 2: 16 bars  - Melody + bass (p)
#   Verse 3: 16 bars  - Melody + alto + bass (mp)
#   Verse 4: 16 bars  - Full SATB (mf)
#   Verse 5: 16 bars  - Full SATB (f)
#   Coda:     8 bars  - Grand ending
#
# Total: 92 bars = 184 seconds ≈ 3:04
# ======================================================================

# ---------- Volume levels ----------
PP  = 70       # pianissimo
P   = 100      # piano
MP  = 130      # mezzo-piano
MF  = 160      # mezzo-forte
F   = 190      # forte
FF  = 220      # fortissimo


# ---------- Phrase builders (melody) ----------
# Each phrase is 4 bars = 384000 samples

def melody_A(v, g=GAP):
    """Phrase A: E E F G | G F E D | C C D E | E. D D"""
    return [
        n(HP_E4, QUARTER, g, v), n(HP_E4, QUARTER, g, v),
        n(HP_F4, QUARTER, g, v), n(HP_G4, QUARTER, g, v),
        n(HP_G4, QUARTER, g, v), n(HP_F4, QUARTER, g, v),
        n(HP_E4, QUARTER, g, v), n(HP_D4, QUARTER, g, v),
        n(HP_C4, QUARTER, g, v), n(HP_C4, QUARTER, g, v),
        n(HP_D4, QUARTER, g, v), n(HP_E4, QUARTER, g, v),
        n(HP_E4, DOTQ,    g, v), n(HP_D4, EIGHTH,  g, v),
        n(HP_D4, HALF,  LGAP, v),
    ]


def melody_Ap(v, g=GAP):
    """Phrase A': same but ending C instead of D"""
    return [
        n(HP_E4, QUARTER, g, v), n(HP_E4, QUARTER, g, v),
        n(HP_F4, QUARTER, g, v), n(HP_G4, QUARTER, g, v),
        n(HP_G4, QUARTER, g, v), n(HP_F4, QUARTER, g, v),
        n(HP_E4, QUARTER, g, v), n(HP_D4, QUARTER, g, v),
        n(HP_C4, QUARTER, g, v), n(HP_C4, QUARTER, g, v),
        n(HP_D4, QUARTER, g, v), n(HP_E4, QUARTER, g, v),
        n(HP_D4, DOTQ,    g, v), n(HP_C4, EIGHTH,  g, v),
        n(HP_C4, HALF,  LGAP, v),
    ]


def melody_B(v, g=GAP):
    """Phrase B (bridge)"""
    return [
        n(HP_D4, QUARTER, g, v), n(HP_D4, QUARTER, g, v),
        n(HP_E4, QUARTER, g, v), n(HP_C4, QUARTER, g, v),
        n(HP_D4, QUARTER, g, v), n(HP_E4, EIGHTH,  g, v),
        n(HP_F4, EIGHTH,  g, v), n(HP_E4, QUARTER, g, v),
        n(HP_C4, QUARTER, g, v),
        n(HP_D4, QUARTER, g, v), n(HP_E4, EIGHTH,  g, v),
        n(HP_F4, EIGHTH,  g, v), n(HP_E4, QUARTER, g, v),
        n(HP_D4, QUARTER, g, v),
        n(HP_C4, QUARTER, g, v), n(HP_D4, QUARTER, g, v),
        n(HP_G3, HALF,  LGAP, v),
    ]


def melody_verse(v, g=GAP):
    """Full verse: A A' B A'"""
    return melody_A(v, g) + melody_Ap(v, g) + melody_B(v, g) + melody_Ap(v, g)


# ---------- Phrase builders (alto) ----------

def alto_A(v, g=GAP):
    return [
        n(HP_C4, QUARTER, g, v), n(HP_C4, QUARTER, g, v),
        n(HP_D4, QUARTER, g, v), n(HP_E4, QUARTER, g, v),
        n(HP_E4, QUARTER, g, v), n(HP_D4, QUARTER, g, v),
        n(HP_C4, QUARTER, g, v), n(HP_B3, QUARTER, g, v),
        n(HP_A3, QUARTER, g, v), n(HP_A3, QUARTER, g, v),
        n(HP_B3, QUARTER, g, v), n(HP_C4, QUARTER, g, v),
        n(HP_C4, DOTQ,    g, v), n(HP_B3, EIGHTH,  g, v),
        n(HP_B3, HALF,  LGAP, v),
    ]


def alto_Ap(v, g=GAP):
    return [
        n(HP_C4, QUARTER, g, v), n(HP_C4, QUARTER, g, v),
        n(HP_D4, QUARTER, g, v), n(HP_E4, QUARTER, g, v),
        n(HP_E4, QUARTER, g, v), n(HP_D4, QUARTER, g, v),
        n(HP_C4, QUARTER, g, v), n(HP_B3, QUARTER, g, v),
        n(HP_A3, QUARTER, g, v), n(HP_A3, QUARTER, g, v),
        n(HP_B3, QUARTER, g, v), n(HP_C4, QUARTER, g, v),
        n(HP_B3, DOTQ,    g, v), n(HP_A3, EIGHTH,  g, v),
        n(HP_A3, HALF,  LGAP, v),
    ]


def alto_B(v, g=GAP):
    return [
        n(HP_B3, QUARTER, g, v), n(HP_B3, QUARTER, g, v),
        n(HP_C4, QUARTER, g, v), n(HP_A3, QUARTER, g, v),
        n(HP_B3, QUARTER, g, v), n(HP_C4, EIGHTH,  g, v),
        n(HP_D4, EIGHTH,  g, v), n(HP_C4, QUARTER, g, v),
        n(HP_A3, QUARTER, g, v),
        n(HP_B3, QUARTER, g, v), n(HP_C4, EIGHTH,  g, v),
        n(HP_D4, EIGHTH,  g, v), n(HP_C4, QUARTER, g, v),
        n(HP_B3, QUARTER, g, v),
        n(HP_A3, QUARTER, g, v), n(HP_B3, QUARTER, g, v),
        n(HP_D3, HALF,  LGAP, v),
    ]


def alto_verse(v, g=GAP):
    return alto_A(v, g) + alto_Ap(v, g) + alto_B(v, g) + alto_Ap(v, g)


# ---------- Phrase builders (tenor) ----------

def tenor_A(v, g=GAP):
    return [
        n(HP_G3, HALF, g, v), n(HP_G3, HALF,   g, v),
        n(HP_G3, HALF, g, v), n(HP_G3, HALF,   g, v),
        n(HP_E3, HALF, g, v), n(HP_G3, HALF,   g, v),
        n(HP_G3, HALF, g, v), n(HP_G3, HALF, LGAP, v),
    ]


def tenor_Ap(v, g=GAP):
    return [
        n(HP_G3, HALF, g, v), n(HP_G3, HALF,   g, v),
        n(HP_G3, HALF, g, v), n(HP_G3, HALF,   g, v),
        n(HP_E3, HALF, g, v), n(HP_G3, HALF,   g, v),
        n(HP_F3, HALF, g, v), n(HP_E3, HALF, LGAP, v),
    ]


def tenor_B(v, g=GAP):
    return [
        n(HP_G3, HALF, g, v), n(HP_G3, HALF, g, v),
        n(HP_G3, HALF, g, v), n(HP_G3, HALF, g, v),
        n(HP_G3, HALF, g, v), n(HP_G3, HALF, g, v),
        n(HP_E3, HALF, g, v), n(HP_D3, HALF, LGAP, v),
    ]


def tenor_verse(v, g=GAP):
    return tenor_A(v, g) + tenor_Ap(v, g) + tenor_B(v, g) + tenor_Ap(v, g)


# ---------- Phrase builders (bass) ----------

def bass_A(v, g=GAP):
    return [
        n(HP_C3, WHOLE, g, v), n(HP_C3, WHOLE, g, v),
        n(HP_C3, WHOLE, g, v), n(HP_G3, WHOLE, LGAP, v),
    ]


def bass_Ap(v, g=GAP):
    return [
        n(HP_C3, WHOLE, g, v), n(HP_C3, WHOLE, g, v),
        n(HP_C3, WHOLE, g, v), n(HP_C3, WHOLE, LGAP, v),
    ]


def bass_B(v, g=GAP):
    return [
        n(HP_G3, WHOLE, g, v), n(HP_G3, WHOLE, g, v),
        n(HP_G3, WHOLE, g, v), n(HP_C3, WHOLE, LGAP, v),
    ]


def bass_verse(v, g=GAP):
    return bass_A(v, g) + bass_Ap(v, g) + bass_B(v, g) + bass_Ap(v, g)


# ======================================================================
# Build the arrangement
# ======================================================================

INTRO_DUR = 4 * WHOLE           # 4 bars
VERSE_DUR = 16 * WHOLE          # 16 bars (one full A A' B A')
CODA_DUR  = 8 * WHOLE           # 8 bars

# ---------- Intro: 4 bars ----------
# Bass: C pedal (whole notes)
# Tenor: G drone (half notes, gentle)
# Alto + Melody: rest

intro_melody = rest_for(INTRO_DUR)
intro_alto   = rest_for(INTRO_DUR)
intro_tenor  = [
    n(HP_G3, WHOLE, SGAP, PP), n(HP_G3, WHOLE, SGAP, PP),
    n(HP_G3, WHOLE, SGAP, PP), n(HP_G3, WHOLE, LGAP, PP),
]
intro_bass = [
    n(HP_C3, WHOLE, SGAP, PP), n(HP_C3, WHOLE, SGAP, PP),
    n(HP_C3, WHOLE, SGAP, PP), n(HP_C3, WHOLE, LGAP, PP),
]

# ---------- Verse 1: Melody solo (pp) ----------

v1_melody = melody_verse(P)
v1_alto   = rest_for(VERSE_DUR)
v1_tenor  = rest_for(VERSE_DUR)
v1_bass   = rest_for(VERSE_DUR)

# ---------- Verse 2: Melody + Bass (p/mp) ----------

v2_melody = melody_verse(MP)
v2_alto   = rest_for(VERSE_DUR)
v2_tenor  = rest_for(VERSE_DUR)
v2_bass   = bass_verse(P)

# ---------- Verse 3: Melody + Alto + Bass (mp) ----------

v3_melody = melody_verse(MP)
v3_alto   = alto_verse(P)
v3_tenor  = rest_for(VERSE_DUR)
v3_bass   = bass_verse(MP)

# ---------- Verse 4: Full SATB (mf) ----------

v4_melody = melody_verse(MF)
v4_alto   = alto_verse(MP)
v4_tenor  = tenor_verse(P)
v4_bass   = bass_verse(MF)

# ---------- Verse 5: Full SATB (f) ----------

v5_melody = melody_verse(F)
v5_alto   = alto_verse(MF)
v5_tenor  = tenor_verse(MP)
v5_bass   = bass_verse(F)

# ---------- Coda: 8 bars, grand ending ----------
# Restate A phrase forte, then sustained C major chord

coda_melody = (
    melody_A(FF) +
    [  # 4 bars: sustained high C
        n(HP_E4, HALF,  SGAP, FF),
        n(HP_D4, HALF,  SGAP, MF),
        n(HP_C4, WHOLE, SGAP, FF),
        n(HP_C4, WHOLE, LGAP, FF),
    ]
)

coda_alto = (
    alto_A(F) +
    [  # 4 bars: sustained E
        n(HP_C4, HALF,  SGAP, F),
        n(HP_B3, HALF,  SGAP, MP),
        n(HP_A3, WHOLE, SGAP, F),
        n(HP_A3, WHOLE, LGAP, F),
    ]
)

coda_tenor = (
    tenor_A(MP) +
    [  # 4 bars: sustained G
        n(HP_G3, HALF,  SGAP, MP),
        n(HP_G3, HALF,  SGAP, P),
        n(HP_E3, WHOLE, SGAP, MP),
        n(HP_E3, WHOLE, LGAP, MP),
    ]
)

coda_bass = (
    bass_A(FF) +
    [  # 4 bars: sustained low C
        n(HP_C3, HALF,  SGAP, FF),
        n(HP_G2, HALF,  SGAP, MF),
        n(HP_C3, WHOLE, SGAP, FF),
        n(HP_C3, WHOLE, LGAP, FF),
    ]
)


# ======================================================================
# Assemble Ode to Joy tracks
# ======================================================================

ode_melody = (intro_melody + v1_melody + v2_melody + v3_melody +
              v4_melody + v5_melody + coda_melody)
ode_alto   = (intro_alto + v1_alto + v2_alto + v3_alto +
              v4_alto + v5_alto + coda_alto)
ode_tenor  = (intro_tenor + v1_tenor + v2_tenor + v3_tenor +
              v4_tenor + v5_tenor + coda_tenor)
ode_bass   = (intro_bass + v1_bass + v2_bass + v3_bass +
              v4_bass + v5_bass + coda_bass)

# Verify all Ode to Joy tracks have the same total duration
ode_mel_dur = section_duration(ode_melody)
ode_alt_dur = section_duration(ode_alto)
ode_ten_dur = section_duration(ode_tenor)
ode_bas_dur = section_duration(ode_bass)

assert ode_mel_dur == ode_alt_dur == ode_ten_dur == ode_bas_dur, \
    (f"Ode to Joy track durations differ!\n"
     f"  Melody: {ode_mel_dur}\n  Alto: {ode_alt_dur}\n"
     f"  Tenor: {ode_ten_dur}\n  Bass: {ode_bas_dur}")

ode_seconds = ode_mel_dur / 48000
print(f"Ode to Joy: {ode_mel_dur} samples = {ode_seconds:.1f}s "
      f"= {int(ode_seconds)//60}:{int(ode_seconds)%60:02d}")


# ======================================================================
# William Tell Overture (Finale) - 4-part arrangement in G major, 152 BPM
#
# The famous "Lone Ranger" galloping theme.
# Structure (~2.5 min):
#   Intro:    4 bars  - Trumpet call (melody only)
#   Section A: 16 bars - Main gallop theme (full SATB)
#   Section B: 16 bars - Contrasting lyrical theme
#   Section A': 16 bars - Gallop theme reprise (full, louder)
#   Coda:     8 bars  - Grand finish
#
# Total: 60 bars at 152 BPM ≈ 95 seconds
# ======================================================================

# ---------- Durations at 152 BPM (48 kHz samples) ----------
# Quarter note = 60/152 * 48000 ≈ 18947 samples

WT_SIXTEENTH = 4737     # ~0.099 sec
WT_EIGHTH    = 9474     # ~0.197 sec
WT_QUARTER   = 18947    # ~0.395 sec
WT_DOTQ      = 28421    # ~0.592 sec (dotted quarter)
WT_HALF      = 37895    # ~0.789 sec
WT_DOTH      = 56842    # ~1.184 sec (dotted half)
WT_WHOLE     = 75789    # ~1.579 sec
WT_BAR       = WT_WHOLE # 4/4 time

WT_GAP  = 1200          # short articulation gap for gallop feel
WT_LGAP = 2400          # longer gap for phrase endings
WT_SGAP = 600           # very short for rapid passages

# ---------- William Tell phrase builders (melody) ----------

def wt_melody_intro(v):
    """4-bar trumpet call: da-da-DUM da-da-DUM da-da-DUM DUM DUM"""
    return [
        # Bar 1-2: triplet-like fanfare
        n(HP_G4, WT_EIGHTH, WT_GAP, v), n(HP_G4, WT_EIGHTH, WT_GAP, v),
        n(HP_G4, WT_QUARTER, WT_GAP, v),
        n(HP_G4, WT_EIGHTH, WT_GAP, v), n(HP_G4, WT_EIGHTH, WT_GAP, v),
        n(HP_G4, WT_QUARTER, WT_GAP, v),
        n(HP_G4, WT_EIGHTH, WT_GAP, v), n(HP_G4, WT_EIGHTH, WT_GAP, v),
        n(HP_G4, WT_QUARTER, WT_GAP, v),
        n(HP_B4, WT_QUARTER, WT_LGAP, v),
        # Bar 3-4: ascending resolution
        n(HP_G4, WT_EIGHTH, WT_GAP, v), n(HP_G4, WT_EIGHTH, WT_GAP, v),
        n(HP_G4, WT_QUARTER, WT_GAP, v),
        n(HP_G4, WT_EIGHTH, WT_GAP, v), n(HP_G4, WT_EIGHTH, WT_GAP, v),
        n(HP_G4, WT_QUARTER, WT_GAP, v),
        n(HP_B4, WT_QUARTER, WT_GAP, v),
        n(HP_D5, WT_QUARTER, WT_GAP, v),
        n(HP_G4, WT_HALF, WT_LGAP, v),
    ]


def wt_melody_A(v):
    """8-bar galloping theme section"""
    return [
        # Bar 1: gallop motif G-B-D
        n(HP_G4, WT_EIGHTH, WT_SGAP, v), n(HP_G4, WT_SIXTEENTH, WT_SGAP, v),
        n(HP_G4, WT_SIXTEENTH, WT_GAP, v), n(HP_B4, WT_QUARTER, WT_GAP, v),
        n(HP_B4, WT_EIGHTH, WT_SGAP, v), n(HP_B4, WT_SIXTEENTH, WT_SGAP, v),
        n(HP_B4, WT_SIXTEENTH, WT_GAP, v), n(HP_D5, WT_QUARTER, WT_GAP, v),
        # Bar 2: answer phrase
        n(HP_D5, WT_EIGHTH, WT_SGAP, v), n(HP_D5, WT_SIXTEENTH, WT_SGAP, v),
        n(HP_D5, WT_SIXTEENTH, WT_GAP, v), n(HP_B4, WT_QUARTER, WT_GAP, v),
        n(HP_G4, WT_HALF, WT_LGAP, v),
        # Bar 3-4: gallop repeat with variation
        n(HP_G4, WT_EIGHTH, WT_SGAP, v), n(HP_G4, WT_SIXTEENTH, WT_SGAP, v),
        n(HP_G4, WT_SIXTEENTH, WT_GAP, v), n(HP_A4, WT_QUARTER, WT_GAP, v),
        n(HP_B4, WT_EIGHTH, WT_SGAP, v), n(HP_B4, WT_SIXTEENTH, WT_SGAP, v),
        n(HP_B4, WT_SIXTEENTH, WT_GAP, v), n(HP_C5, WT_QUARTER, WT_GAP, v),
        n(HP_D5, WT_QUARTER, WT_GAP, v), n(HP_B4, WT_QUARTER, WT_GAP, v),
        n(HP_G4, WT_HALF, WT_LGAP, v),
        # Bar 5-6: soaring theme
        n(HP_D5, WT_QUARTER, WT_GAP, v), n(HP_D5, WT_QUARTER, WT_GAP, v),
        n(HP_E5, WT_QUARTER, WT_GAP, v), n(HP_D5, WT_QUARTER, WT_GAP, v),
        n(HP_C5, WT_QUARTER, WT_GAP, v), n(HP_B4, WT_QUARTER, WT_GAP, v),
        n(HP_A4, WT_QUARTER, WT_GAP, v), n(HP_B4, WT_QUARTER, WT_GAP, v),
        # Bar 7-8: resolution
        n(HP_G4, WT_EIGHTH, WT_SGAP, v), n(HP_G4, WT_SIXTEENTH, WT_SGAP, v),
        n(HP_G4, WT_SIXTEENTH, WT_GAP, v), n(HP_B4, WT_QUARTER, WT_GAP, v),
        n(HP_D5, WT_QUARTER, WT_GAP, v), n(HP_B4, WT_QUARTER, WT_GAP, v),
        n(HP_G4, WT_WHOLE, WT_LGAP, v),
    ]


def wt_melody_B(v):
    """8-bar lyrical contrasting theme"""
    return [
        # Bar 1-2: lyrical ascending line
        n(HP_D4, WT_HALF, WT_GAP, v), n(HP_G4, WT_HALF, WT_GAP, v),
        n(HP_B4, WT_HALF, WT_GAP, v), n(HP_A4, WT_HALF, WT_GAP, v),
        # Bar 3-4: descending answer
        n(HP_G4, WT_HALF, WT_GAP, v), n(HP_F4, WT_HALF, WT_GAP, v),
        n(HP_E4, WT_HALF, WT_GAP, v), n(HP_D4, WT_HALF, WT_LGAP, v),
        # Bar 5-6: repeat higher
        n(HP_E4, WT_HALF, WT_GAP, v), n(HP_A4, WT_HALF, WT_GAP, v),
        n(HP_C5, WT_HALF, WT_GAP, v), n(HP_B4, WT_HALF, WT_GAP, v),
        # Bar 7-8: resolve to G
        n(HP_A4, WT_HALF, WT_GAP, v), n(HP_G4, WT_HALF, WT_GAP, v),
        n(HP_G4, WT_WHOLE, WT_LGAP, v),
    ]


def wt_melody_coda(v):
    """8-bar grand coda"""
    return [
        # Bar 1-2: triumphant fanfare
        n(HP_D5, WT_QUARTER, WT_GAP, v), n(HP_D5, WT_QUARTER, WT_GAP, v),
        n(HP_D5, WT_QUARTER, WT_GAP, v), n(HP_D5, WT_QUARTER, WT_GAP, v),
        n(HP_E5, WT_HALF, WT_GAP, v), n(HP_D5, WT_HALF, WT_GAP, v),
        # Bar 3-4: descending power
        n(HP_C5, WT_QUARTER, WT_GAP, v), n(HP_B4, WT_QUARTER, WT_GAP, v),
        n(HP_A4, WT_QUARTER, WT_GAP, v), n(HP_G4, WT_QUARTER, WT_GAP, v),
        n(HP_B4, WT_HALF, WT_GAP, v), n(HP_D5, WT_HALF, WT_GAP, v),
        # Bar 5-6: final gallop
        n(HP_G4, WT_EIGHTH, WT_SGAP, v), n(HP_G4, WT_SIXTEENTH, WT_SGAP, v),
        n(HP_G4, WT_SIXTEENTH, WT_GAP, v), n(HP_B4, WT_QUARTER, WT_GAP, v),
        n(HP_D5, WT_QUARTER, WT_GAP, v), n(HP_G5, WT_QUARTER, WT_GAP, v),
        n(HP_D5, WT_HALF, WT_GAP, v), n(HP_B4, WT_HALF, WT_GAP, v),
        # Bar 7-8: final G chord
        n(HP_G4, WT_WHOLE, WT_GAP, v),
        n(HP_G4, WT_WHOLE, WT_LGAP, v),
    ]


# ---------- William Tell phrase builders (alto) ----------

def wt_alto_intro(v):
    """4 bars rest during trumpet call"""
    return rest_for(section_duration(wt_melody_intro(v)))


def wt_alto_A(v):
    """8-bar gallop accompaniment"""
    return [
        # Sustained chord tones, half notes
        n(HP_D4, WT_HALF, WT_GAP, v), n(HP_D4, WT_HALF, WT_GAP, v),
        n(HP_D4, WT_HALF, WT_GAP, v), n(HP_D4, WT_HALF, WT_GAP, v),
        n(HP_D4, WT_HALF, WT_GAP, v), n(HP_E4, WT_HALF, WT_GAP, v),
        n(HP_D4, WT_HALF, WT_GAP, v), n(HP_D4, WT_HALF, WT_GAP, v),
        n(HP_G4, WT_HALF, WT_GAP, v), n(HP_G4, WT_HALF, WT_GAP, v),
        n(HP_E4, WT_HALF, WT_GAP, v), n(HP_D4, WT_HALF, WT_GAP, v),
        n(HP_D4, WT_HALF, WT_GAP, v), n(HP_D4, WT_HALF, WT_GAP, v),
        n(HP_B3, WT_WHOLE, WT_LGAP, v),
    ]


def wt_alto_B(v):
    """8-bar lyrical countermelody"""
    return [
        n(HP_B3, WT_HALF, WT_GAP, v), n(HP_D4, WT_HALF, WT_GAP, v),
        n(HP_D4, WT_HALF, WT_GAP, v), n(HP_C4, WT_HALF, WT_GAP, v),
        n(HP_B3, WT_HALF, WT_GAP, v), n(HP_D4, WT_HALF, WT_GAP, v),
        n(HP_C4, WT_HALF, WT_GAP, v), n(HP_B3, WT_HALF, WT_LGAP, v),
        n(HP_C4, WT_HALF, WT_GAP, v), n(HP_E4, WT_HALF, WT_GAP, v),
        n(HP_E4, WT_HALF, WT_GAP, v), n(HP_D4, WT_HALF, WT_GAP, v),
        n(HP_C4, WT_HALF, WT_GAP, v), n(HP_B3, WT_HALF, WT_GAP, v),
        n(HP_B3, WT_WHOLE, WT_LGAP, v),
    ]


def wt_alto_coda(v):
    """8-bar coda"""
    return [
        n(HP_G4, WT_QUARTER, WT_GAP, v), n(HP_G4, WT_QUARTER, WT_GAP, v),
        n(HP_G4, WT_QUARTER, WT_GAP, v), n(HP_G4, WT_QUARTER, WT_GAP, v),
        n(HP_G4, WT_HALF, WT_GAP, v), n(HP_G4, WT_HALF, WT_GAP, v),
        n(HP_E4, WT_QUARTER, WT_GAP, v), n(HP_D4, WT_QUARTER, WT_GAP, v),
        n(HP_C4, WT_QUARTER, WT_GAP, v), n(HP_B3, WT_QUARTER, WT_GAP, v),
        n(HP_D4, WT_HALF, WT_GAP, v), n(HP_G4, WT_HALF, WT_GAP, v),
        n(HP_D4, WT_HALF, WT_GAP, v), n(HP_G4, WT_HALF, WT_GAP, v),
        n(HP_D4, WT_HALF, WT_GAP, v), n(HP_D4, WT_HALF, WT_GAP, v),
        n(HP_D4, WT_WHOLE, WT_GAP, v),
        n(HP_D4, WT_WHOLE, WT_LGAP, v),
    ]


# ---------- William Tell phrase builders (tenor) ----------

def wt_tenor_intro(v):
    return rest_for(section_duration(wt_melody_intro(v)))


def wt_tenor_A(v):
    """8-bar gallop rhythm on chord root"""
    return [
        n(HP_B3, WT_HALF, WT_GAP, v), n(HP_B3, WT_HALF, WT_GAP, v),
        n(HP_B3, WT_HALF, WT_GAP, v), n(HP_B3, WT_HALF, WT_GAP, v),
        n(HP_B3, WT_HALF, WT_GAP, v), n(HP_C4, WT_HALF, WT_GAP, v),
        n(HP_B3, WT_HALF, WT_GAP, v), n(HP_B3, WT_HALF, WT_GAP, v),
        n(HP_B3, WT_HALF, WT_GAP, v), n(HP_B3, WT_HALF, WT_GAP, v),
        n(HP_C4, WT_HALF, WT_GAP, v), n(HP_B3, WT_HALF, WT_GAP, v),
        n(HP_B3, WT_HALF, WT_GAP, v), n(HP_B3, WT_HALF, WT_GAP, v),
        n(HP_G3, WT_WHOLE, WT_LGAP, v),
    ]


def wt_tenor_B(v):
    """8-bar sustained tones"""
    return [
        n(HP_G3, WT_WHOLE, WT_GAP, v), n(HP_G3, WT_WHOLE, WT_GAP, v),
        n(HP_G3, WT_WHOLE, WT_GAP, v), n(HP_G3, WT_WHOLE, WT_LGAP, v),
        n(HP_A3, WT_WHOLE, WT_GAP, v), n(HP_A3, WT_WHOLE, WT_GAP, v),
        n(HP_G3, WT_WHOLE, WT_GAP, v), n(HP_G3, WT_WHOLE, WT_LGAP, v),
    ]


def wt_tenor_coda(v):
    return [
        n(HP_B3, WT_QUARTER, WT_GAP, v), n(HP_B3, WT_QUARTER, WT_GAP, v),
        n(HP_B3, WT_QUARTER, WT_GAP, v), n(HP_B3, WT_QUARTER, WT_GAP, v),
        n(HP_C4, WT_HALF, WT_GAP, v), n(HP_B3, WT_HALF, WT_GAP, v),
        n(HP_A3, WT_QUARTER, WT_GAP, v), n(HP_G3, WT_QUARTER, WT_GAP, v),
        n(HP_E3, WT_QUARTER, WT_GAP, v), n(HP_D3, WT_QUARTER, WT_GAP, v),
        n(HP_G3, WT_HALF, WT_GAP, v), n(HP_B3, WT_HALF, WT_GAP, v),
        n(HP_B3, WT_HALF, WT_GAP, v), n(HP_G3, WT_HALF, WT_GAP, v),
        n(HP_G3, WT_WHOLE, WT_GAP, v),
        n(HP_B3, WT_WHOLE, WT_LGAP, v),
    ]


# ---------- William Tell phrase builders (bass) ----------

def wt_bass_intro(v):
    return rest_for(section_duration(wt_melody_intro(v)))


def wt_bass_A(v):
    """8-bar bass line: root motion"""
    return [
        n(HP_G3, WT_WHOLE, WT_GAP, v), n(HP_G3, WT_WHOLE, WT_GAP, v),
        n(HP_G3, WT_WHOLE, WT_GAP, v), n(HP_G3, WT_WHOLE, WT_GAP, v),
        n(HP_G3, WT_WHOLE, WT_GAP, v), n(HP_G3, WT_WHOLE, WT_GAP, v),
        n(HP_G3, WT_WHOLE, WT_GAP, v), n(HP_G2, WT_WHOLE, WT_LGAP, v),
    ]


def wt_bass_B(v):
    """8-bar bass for lyrical section"""
    return [
        n(HP_G2, WT_WHOLE, WT_GAP, v), n(HP_G2, WT_WHOLE, WT_GAP, v),
        n(HP_G2, WT_WHOLE, WT_GAP, v), n(HP_G2, WT_WHOLE, WT_LGAP, v),
        n(HP_A2, WT_WHOLE, WT_GAP, v), n(HP_A2, WT_WHOLE, WT_GAP, v),
        n(HP_G2, WT_WHOLE, WT_GAP, v), n(HP_G2, WT_WHOLE, WT_LGAP, v),
    ]


def wt_bass_coda(v):
    return [
        n(HP_G3, WT_QUARTER, WT_GAP, v), n(HP_G3, WT_QUARTER, WT_GAP, v),
        n(HP_G3, WT_QUARTER, WT_GAP, v), n(HP_G3, WT_QUARTER, WT_GAP, v),
        n(HP_G3, WT_HALF, WT_GAP, v), n(HP_G3, WT_HALF, WT_GAP, v),
        n(HP_G3, WT_WHOLE, WT_GAP, v),
        n(HP_G3, WT_WHOLE, WT_GAP, v),
        n(HP_G2, WT_WHOLE, WT_GAP, v),
        n(HP_G2, WT_WHOLE, WT_GAP, v),
        n(HP_G2, WT_WHOLE, WT_GAP, v),
        n(HP_G2, WT_WHOLE, WT_LGAP, v),
    ]


# ======================================================================
# Assemble William Tell Overture
# ======================================================================

wt_melody = (wt_melody_intro(F) +
             wt_melody_A(MF) + wt_melody_A(MF) +
             wt_melody_B(MP) + wt_melody_B(MF) +
             wt_melody_A(F) + wt_melody_A(FF) +
             wt_melody_coda(FF))

wt_alto = (wt_alto_intro(F) +
           wt_alto_A(P) + wt_alto_A(MP) +
           wt_alto_B(P) + wt_alto_B(MP) +
           wt_alto_A(MP) + wt_alto_A(MF) +
           wt_alto_coda(F))

wt_tenor = (wt_tenor_intro(F) +
            wt_tenor_A(P) + wt_tenor_A(P) +
            wt_tenor_B(PP) + wt_tenor_B(P) +
            wt_tenor_A(MP) + wt_tenor_A(MP) +
            wt_tenor_coda(MF))

wt_bass = (wt_bass_intro(F) +
           wt_bass_A(MP) + wt_bass_A(MP) +
           wt_bass_B(P) + wt_bass_B(MP) +
           wt_bass_A(MF) + wt_bass_A(F) +
           wt_bass_coda(FF))

# Verify William Tell tracks have the same duration
wt_mel_dur = section_duration(wt_melody)
wt_alt_dur = section_duration(wt_alto)
wt_ten_dur = section_duration(wt_tenor)
wt_bas_dur = section_duration(wt_bass)

# Pad shorter tracks with rest
wt_max_dur = max(wt_mel_dur, wt_alt_dur, wt_ten_dur, wt_bas_dur)
if wt_mel_dur < wt_max_dur:
    wt_melody += rest_for(wt_max_dur - wt_mel_dur)
if wt_alt_dur < wt_max_dur:
    wt_alto += rest_for(wt_max_dur - wt_alt_dur)
if wt_ten_dur < wt_max_dur:
    wt_tenor += rest_for(wt_max_dur - wt_ten_dur)
if wt_bas_dur < wt_max_dur:
    wt_bass += rest_for(wt_max_dur - wt_bas_dur)

wt_seconds = wt_max_dur / 48000
print(f"William Tell: {wt_max_dur} samples = {wt_seconds:.1f}s "
      f"= {int(wt_seconds)//60}:{int(wt_seconds)%60:02d}")


# ======================================================================
# Entry of the Gladiators (Julius Fučík, 1897) - Circus March
# 4-part arrangement in Eb major, 130 BPM, 2/4 time
#
# The iconic circus/clown march theme.
# Structure (~2 min):
#   Intro:     4 bars  - Dramatic chromatic descent (full)
#   March A:   16 bars - Main march theme (bouncy staccato)
#   March A':  16 bars - March reprise (louder)
#   Trio:      16 bars - Famous lyrical "circus" melody
#   Trio':     16 bars - Trio reprise (full SATB)
#   Coda:      4 bars  - Grand finish
#
# Total: 72 bars at 130 BPM (2/4) ≈ 66 seconds
# ======================================================================

# ---------- Durations at 130 BPM, 2/4 time (48 kHz samples) ----------
# Quarter note = 60/130 * 48000 ≈ 22154 samples
# Bar = 2 quarters = 44308 samples

EG_SIXTEENTH = 5538      # ~0.115 sec
EG_EIGHTH    = 11077     # ~0.231 sec
EG_QUARTER   = 22154     # ~0.461 sec
EG_DOTQ      = 33231     # ~0.692 sec
EG_HALF      = 44308     # ~0.923 sec (= 1 bar in 2/4)
EG_WHOLE     = 88615     # ~1.846 sec (= 2 bars)

EG_GAP  = 1000           # short staccato gap for march feel
EG_LGAP = 2000           # longer gap for phrase endings
EG_SGAP = 500            # very short for rapid runs


# ---------- Entry of the Gladiators phrase builders (melody) ----------

def eg_melody_intro(v):
    """4-bar dramatic chromatic descent introduction"""
    return [
        # Bar 1-2: Dramatic fanfare
        n(HP_Eb5, EG_QUARTER, EG_GAP, v),
        n(HP_D5, EG_QUARTER, EG_GAP, v),
        n(HP_Db5, EG_QUARTER, EG_GAP, v),
        n(HP_C5, EG_QUARTER, EG_GAP, v),
        # Bar 3-4: Resolve with punch
        n(HP_B4, EG_QUARTER, EG_GAP, v),
        n(HP_Bb4, EG_QUARTER, EG_GAP, v),
        n(HP_A4, EG_QUARTER, EG_GAP, v),
        n(HP_Ab4, EG_QUARTER, EG_LGAP, v),
    ]


def eg_melody_march(v):
    """8-bar bouncy march theme"""
    return [
        # Bar 1-2: Staccato ascending Eb major
        n(HP_Eb4, EG_EIGHTH, EG_SGAP, v),
        n(HP_Eb4, EG_EIGHTH, EG_GAP, v),
        n(HP_Eb4, EG_EIGHTH, EG_SGAP, v),
        n(HP_G4, EG_EIGHTH, EG_GAP, v),
        n(HP_Bb4, EG_QUARTER, EG_GAP, v),
        n(HP_Ab4, EG_EIGHTH, EG_GAP, v),
        n(HP_G4, EG_EIGHTH, EG_LGAP, v),
        # Bar 3-4: Answer phrase descending
        n(HP_Ab4, EG_EIGHTH, EG_SGAP, v),
        n(HP_Ab4, EG_EIGHTH, EG_GAP, v),
        n(HP_Ab4, EG_EIGHTH, EG_SGAP, v),
        n(HP_Bb4, EG_EIGHTH, EG_GAP, v),
        n(HP_Ab4, EG_QUARTER, EG_GAP, v),
        n(HP_G4, EG_QUARTER, EG_LGAP, v),
        # Bar 5-6: Rising phrase
        n(HP_Eb4, EG_EIGHTH, EG_SGAP, v),
        n(HP_G4, EG_EIGHTH, EG_SGAP, v),
        n(HP_Bb4, EG_EIGHTH, EG_SGAP, v),
        n(HP_Eb5, EG_EIGHTH, EG_GAP, v),
        n(HP_D5, EG_QUARTER, EG_GAP, v),
        n(HP_C5, EG_QUARTER, EG_GAP, v),
        # Bar 7-8: Cadence
        n(HP_Bb4, EG_EIGHTH, EG_SGAP, v),
        n(HP_Ab4, EG_EIGHTH, EG_GAP, v),
        n(HP_G4, EG_EIGHTH, EG_SGAP, v),
        n(HP_F4, EG_EIGHTH, EG_GAP, v),
        n(HP_Eb4, EG_HALF, EG_LGAP, v),
    ]


def eg_melody_trio(v):
    """16-bar famous trio melody — the iconic circus theme"""
    return [
        # Bar 1-2: The famous bouncing theme in Ab major
        n(HP_Ab4, EG_EIGHTH, EG_GAP, v),
        n(HP_C5, EG_EIGHTH, EG_GAP, v),
        n(HP_Eb5, EG_QUARTER, EG_GAP, v),
        n(HP_Eb5, EG_EIGHTH, EG_SGAP, v),
        n(HP_D5, EG_EIGHTH, EG_GAP, v),
        n(HP_Eb5, EG_QUARTER, EG_LGAP, v),
        # Bar 3-4: Descending answer
        n(HP_Eb5, EG_EIGHTH, EG_GAP, v),
        n(HP_D5, EG_EIGHTH, EG_GAP, v),
        n(HP_C5, EG_QUARTER, EG_GAP, v),
        n(HP_C5, EG_EIGHTH, EG_SGAP, v),
        n(HP_Bb4, EG_EIGHTH, EG_GAP, v),
        n(HP_Ab4, EG_QUARTER, EG_LGAP, v),
        # Bar 5-6: Repeat rising
        n(HP_Ab4, EG_EIGHTH, EG_GAP, v),
        n(HP_C5, EG_EIGHTH, EG_GAP, v),
        n(HP_Eb5, EG_QUARTER, EG_GAP, v),
        n(HP_Eb5, EG_EIGHTH, EG_SGAP, v),
        n(HP_D5, EG_EIGHTH, EG_GAP, v),
        n(HP_Eb5, EG_QUARTER, EG_LGAP, v),
        # Bar 7-8: Cadence to Ab
        n(HP_D5, EG_EIGHTH, EG_SGAP, v),
        n(HP_C5, EG_EIGHTH, EG_GAP, v),
        n(HP_Bb4, EG_EIGHTH, EG_SGAP, v),
        n(HP_Ab4, EG_EIGHTH, EG_GAP, v),
        n(HP_Ab4, EG_HALF, EG_LGAP, v),
        # Bar 9-10: Second phrase — higher energy
        n(HP_C5, EG_EIGHTH, EG_GAP, v),
        n(HP_Eb5, EG_EIGHTH, EG_GAP, v),
        n(HP_Ab4, EG_QUARTER, EG_GAP, v),
        n(HP_C5, EG_EIGHTH, EG_SGAP, v),
        n(HP_Bb4, EG_EIGHTH, EG_GAP, v),
        n(HP_Ab4, EG_QUARTER, EG_LGAP, v),
        # Bar 11-12: Echo response
        n(HP_Bb4, EG_EIGHTH, EG_GAP, v),
        n(HP_D5, EG_EIGHTH, EG_GAP, v),
        n(HP_G4, EG_QUARTER, EG_GAP, v),
        n(HP_Bb4, EG_EIGHTH, EG_SGAP, v),
        n(HP_Ab4, EG_EIGHTH, EG_GAP, v),
        n(HP_G4, EG_QUARTER, EG_LGAP, v),
        # Bar 13-14: Building to climax
        n(HP_Ab4, EG_EIGHTH, EG_GAP, v),
        n(HP_C5, EG_EIGHTH, EG_GAP, v),
        n(HP_Eb5, EG_QUARTER, EG_GAP, v),
        n(HP_Eb5, EG_QUARTER, EG_GAP, v),
        n(HP_D5, EG_QUARTER, EG_GAP, v),
        # Bar 15-16: Final cadence
        n(HP_Eb5, EG_EIGHTH, EG_SGAP, v),
        n(HP_D5, EG_EIGHTH, EG_SGAP, v),
        n(HP_C5, EG_EIGHTH, EG_SGAP, v),
        n(HP_Bb4, EG_EIGHTH, EG_GAP, v),
        n(HP_Ab4, EG_WHOLE, EG_LGAP, v),
    ]


def eg_melody_coda(v):
    """4-bar grand finish"""
    return [
        # Bar 1-2: Fanfare
        n(HP_Eb5, EG_EIGHTH, EG_SGAP, v),
        n(HP_Eb5, EG_EIGHTH, EG_GAP, v),
        n(HP_Eb5, EG_QUARTER, EG_GAP, v),
        n(HP_C5, EG_QUARTER, EG_GAP, v),
        n(HP_Eb5, EG_QUARTER, EG_LGAP, v),
        # Bar 3-4: Final chord
        n(HP_Ab4, EG_QUARTER, EG_GAP, v),
        n(HP_Ab4, EG_QUARTER, EG_GAP, v),
        n(HP_Ab4, EG_WHOLE, EG_LGAP, v),
    ]


# ---------- Entry of the Gladiators phrase builders (alto) ----------

def eg_alto_intro(v):
    """4-bar chromatic descent in thirds"""
    return [
        n(HP_C5, EG_QUARTER, EG_GAP, v),
        n(HP_B4, EG_QUARTER, EG_GAP, v),
        n(HP_Bb4, EG_QUARTER, EG_GAP, v),
        n(HP_Ab4, EG_QUARTER, EG_GAP, v),
        n(HP_G4, EG_QUARTER, EG_GAP, v),
        n(HP_Gb4, EG_QUARTER, EG_GAP, v),
        n(HP_F4, EG_QUARTER, EG_GAP, v),
        n(HP_Eb4, EG_QUARTER, EG_LGAP, v),
    ]


def eg_alto_march(v):
    """8-bar march accompaniment — off-beat chords"""
    return [
        # Sustained Eb chord tones
        n(HP_Bb3, EG_HALF, EG_GAP, v),
        n(HP_Bb3, EG_HALF, EG_GAP, v),
        n(HP_Ab3, EG_HALF, EG_GAP, v),
        n(HP_Bb3, EG_HALF, EG_LGAP, v),
        n(HP_G3, EG_HALF, EG_GAP, v),
        n(HP_Bb3, EG_HALF, EG_GAP, v),
        n(HP_G3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_LGAP, v),
    ]


def eg_alto_trio(v):
    """16-bar trio countermelody"""
    return [
        # Flowing thirds and chord tones under melody
        n(HP_Eb4, EG_HALF, EG_GAP, v),
        n(HP_Eb4, EG_HALF, EG_GAP, v),
        n(HP_Eb4, EG_HALF, EG_GAP, v),
        n(HP_C4, EG_HALF, EG_LGAP, v),
        n(HP_Eb4, EG_HALF, EG_GAP, v),
        n(HP_Eb4, EG_HALF, EG_GAP, v),
        n(HP_D4, EG_HALF, EG_GAP, v),
        n(HP_Eb4, EG_HALF, EG_LGAP, v),
        n(HP_Eb4, EG_HALF, EG_GAP, v),
        n(HP_Eb4, EG_HALF, EG_GAP, v),
        n(HP_D4, EG_HALF, EG_GAP, v),
        n(HP_D4, EG_HALF, EG_LGAP, v),
        n(HP_Eb4, EG_HALF, EG_GAP, v),
        n(HP_Eb4, EG_HALF, EG_GAP, v),
        n(HP_Eb4, EG_HALF, EG_GAP, v),
        n(HP_Eb4, EG_HALF, EG_LGAP, v),
    ]


def eg_alto_coda(v):
    return [
        n(HP_Ab4, EG_EIGHTH, EG_SGAP, v),
        n(HP_Ab4, EG_EIGHTH, EG_GAP, v),
        n(HP_Ab4, EG_QUARTER, EG_GAP, v),
        n(HP_Ab4, EG_QUARTER, EG_GAP, v),
        n(HP_Ab4, EG_QUARTER, EG_LGAP, v),
        n(HP_Eb4, EG_QUARTER, EG_GAP, v),
        n(HP_Eb4, EG_QUARTER, EG_GAP, v),
        n(HP_Eb4, EG_WHOLE, EG_LGAP, v),
    ]


# ---------- Entry of the Gladiators phrase builders (tenor) ----------

def eg_tenor_intro(v):
    """4-bar sustained chords"""
    return [
        n(HP_Ab3, EG_HALF, EG_GAP, v),
        n(HP_Ab3, EG_HALF, EG_GAP, v),
        n(HP_G3, EG_HALF, EG_GAP, v),
        n(HP_G3, EG_HALF, EG_LGAP, v),
    ]


def eg_tenor_march(v):
    """8-bar march — rhythmic chord tones"""
    return [
        n(HP_G3, EG_HALF, EG_GAP, v),
        n(HP_G3, EG_HALF, EG_GAP, v),
        n(HP_Ab3, EG_HALF, EG_GAP, v),
        n(HP_G3, EG_HALF, EG_LGAP, v),
        n(HP_Eb3, EG_HALF, EG_GAP, v),
        n(HP_G3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_LGAP, v),
    ]


def eg_tenor_trio(v):
    """16-bar trio — sustained chord roots"""
    return [
        n(HP_Ab3, EG_WHOLE, EG_GAP, v),
        n(HP_Ab3, EG_WHOLE, EG_GAP, v),
        n(HP_Ab3, EG_WHOLE, EG_GAP, v),
        n(HP_Ab3, EG_WHOLE, EG_LGAP, v),
        n(HP_Ab3, EG_WHOLE, EG_GAP, v),
        n(HP_G3, EG_WHOLE, EG_GAP, v),
        n(HP_Ab3, EG_WHOLE, EG_GAP, v),
        n(HP_Ab3, EG_WHOLE, EG_LGAP, v),
    ]


def eg_tenor_coda(v):
    return [
        n(HP_Eb4, EG_EIGHTH, EG_SGAP, v),
        n(HP_Eb4, EG_EIGHTH, EG_GAP, v),
        n(HP_Eb4, EG_QUARTER, EG_GAP, v),
        n(HP_Eb4, EG_QUARTER, EG_GAP, v),
        n(HP_C4, EG_QUARTER, EG_LGAP, v),
        n(HP_Ab3, EG_QUARTER, EG_GAP, v),
        n(HP_Ab3, EG_QUARTER, EG_GAP, v),
        n(HP_Ab3, EG_WHOLE, EG_LGAP, v),
    ]


# ---------- Entry of the Gladiators phrase builders (bass) ----------

def eg_bass_intro(v):
    """4-bar bass pedal"""
    return [
        n(HP_Ab3, EG_WHOLE, EG_GAP, v),
        n(HP_Ab3, EG_WHOLE, EG_LGAP, v),
    ]


def eg_bass_march(v):
    """8-bar march bass line"""
    return [
        n(HP_Eb3, EG_HALF, EG_GAP, v),
        n(HP_Bb3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_GAP, v),
        n(HP_Bb3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_GAP, v),
        n(HP_Bb3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_LGAP, v),
    ]


def eg_bass_trio(v):
    """16-bar trio bass — root motion"""
    return [
        n(HP_Ab3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_GAP, v),
        n(HP_Ab3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_GAP, v),
        n(HP_Ab3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_GAP, v),
        n(HP_Ab3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_LGAP, v),
        n(HP_Ab3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_GAP, v),
        n(HP_G3, EG_HALF, EG_GAP, v),
        n(HP_G3, EG_HALF, EG_GAP, v),
        n(HP_Ab3, EG_HALF, EG_GAP, v),
        n(HP_Eb3, EG_HALF, EG_GAP, v),
        n(HP_Ab3, EG_HALF, EG_GAP, v),
        n(HP_Ab3, EG_HALF, EG_LGAP, v),
    ]


def eg_bass_coda(v):
    return [
        n(HP_Ab3, EG_QUARTER, EG_GAP, v),
        n(HP_Ab3, EG_QUARTER, EG_GAP, v),
        n(HP_Ab3, EG_QUARTER, EG_GAP, v),
        n(HP_Eb3, EG_QUARTER, EG_LGAP, v),
        n(HP_Ab3, EG_QUARTER, EG_GAP, v),
        n(HP_Ab3, EG_QUARTER, EG_GAP, v),
        n(HP_Ab3, EG_WHOLE, EG_LGAP, v),
    ]


# ======================================================================
# Assemble Entry of the Gladiators
# ======================================================================

eg_melody = (eg_melody_intro(F) +
             eg_melody_march(MF) + eg_melody_march(F) +
             eg_melody_trio(MF) + eg_melody_trio(F) +
             eg_melody_coda(FF))

eg_alto = (eg_alto_intro(MP) +
           eg_alto_march(P) + eg_alto_march(MP) +
           eg_alto_trio(P) + eg_alto_trio(MP) +
           eg_alto_coda(F))

eg_tenor = (eg_tenor_intro(P) +
            eg_tenor_march(PP) + eg_tenor_march(P) +
            eg_tenor_trio(PP) + eg_tenor_trio(P) +
            eg_tenor_coda(MF))

eg_bass = (eg_bass_intro(MP) +
           eg_bass_march(MP) + eg_bass_march(MF) +
           eg_bass_trio(P) + eg_bass_trio(MP) +
           eg_bass_coda(FF))

# Pad shorter tracks with rest
eg_mel_dur = section_duration(eg_melody)
eg_alt_dur = section_duration(eg_alto)
eg_ten_dur = section_duration(eg_tenor)
eg_bas_dur = section_duration(eg_bass)

eg_max_dur = max(eg_mel_dur, eg_alt_dur, eg_ten_dur, eg_bas_dur)
if eg_mel_dur < eg_max_dur:
    eg_melody += rest_for(eg_max_dur - eg_mel_dur)
if eg_alt_dur < eg_max_dur:
    eg_alto += rest_for(eg_max_dur - eg_alt_dur)
if eg_ten_dur < eg_max_dur:
    eg_tenor += rest_for(eg_max_dur - eg_ten_dur)
if eg_bas_dur < eg_max_dur:
    eg_bass += rest_for(eg_max_dur - eg_bas_dur)

eg_seconds = eg_max_dur / 48000
print(f"Entry of the Gladiators: {eg_max_dur} samples = {eg_seconds:.1f}s "
      f"= {int(eg_seconds)//60}:{int(eg_seconds)%60:02d}")


# ======================================================================
# Assemble Ode to Joy tracks
# ======================================================================

ode_melody = (intro_melody + v1_melody + v2_melody + v3_melody +
              v4_melody + v5_melody + coda_melody)
ode_alto   = (intro_alto + v1_alto + v2_alto + v3_alto +
              v4_alto + v5_alto + coda_alto)
ode_tenor  = (intro_tenor + v1_tenor + v2_tenor + v3_tenor +
              v4_tenor + v5_tenor + coda_tenor)
ode_bass   = (intro_bass + v1_bass + v2_bass + v3_bass +
              v4_bass + v5_bass + coda_bass)

# Verify all Ode to Joy tracks have the same total duration
ode_mel_dur = section_duration(ode_melody)
ode_alt_dur = section_duration(ode_alto)
ode_ten_dur = section_duration(ode_tenor)
ode_bas_dur = section_duration(ode_bass)

assert ode_mel_dur == ode_alt_dur == ode_ten_dur == ode_bas_dur, \
    (f"Ode to Joy track durations differ!\n"
     f"  Melody: {ode_mel_dur}\n  Alto: {ode_alt_dur}\n"
     f"  Tenor: {ode_ten_dur}\n  Bass: {ode_bas_dur}")

ode_seconds = ode_mel_dur / 48000
print(f"Ode to Joy: {ode_mel_dur} samples = {ode_seconds:.1f}s "
      f"= {int(ode_seconds)//60}:{int(ode_seconds)%60:02d}")

# ======================================================================

if len(sys.argv) != 2:
    print(f'Usage: {sys.argv[0]} <output_dir>', file=sys.stderr)
    sys.exit(1)

outdir = sys.argv[1]
os.makedirs(outdir, exist_ok=True)

tracks = [
    ('track0.bin', 'Melody (Soprano)', ode_melody, wt_melody, eg_melody),
    ('track1.bin', 'Alto harmony',     ode_alto,   wt_alto,   eg_alto),
    ('track2.bin', 'Tenor',            ode_tenor,  wt_tenor,  eg_tenor),
    ('track3.bin', 'Bass',             ode_bass,   wt_bass,   eg_bass),
]

for fname, label, song1, song2, song3 in tracks:
    path = os.path.join(outdir, fname)
    counts = write_track(path, song1, song2, song3)
    print(f'{label}: {"+".join(str(c) for c in counts)} notes -> {path}')

total_bytes = TRACK_DEPTH * 8
print(f'\n{len(tracks)} tracks, {TRACK_DEPTH} entries each ({NUM_SONGS} songs x '
      f'{SONG_DEPTH}), {total_bytes} bytes per file')
