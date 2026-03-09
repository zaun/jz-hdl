#!/usr/bin/env python3
"""
Generate test WAV files with pure sine tones for spectrum analyzer testing.

Creates 16-bit mono 48kHz WAV files with known frequency content.
Use with send_audio.py to verify the comb filter spectrum analyzer.

The analyzer covers 65 Hz (bar 0) to 2400 Hz (bar 79), log-spaced.
Each tone should light up a specific bar or narrow group of bars.

Usage:
    python3 gen_test_tones.py                    # generates all test files
    python3 gen_test_tones.py --test sweep        # frequency sweep
    python3 gen_test_tones.py --test single 440   # single tone at 440 Hz
"""

import argparse
import math
import struct
import wave
import os

SAMPLE_RATE = 48000
DURATION = 10       # seconds per test
AMPLITUDE = 0.7     # fraction of full scale (avoid clipping)

# Bin frequencies for reference (from the delay LUT)
BIN_FREQS = []
for k in range(80):
    f = 65.0 * (2500.0 / 65.0) ** (k / 79.0)
    BIN_FREQS.append(f)


def make_sine(freq, duration=DURATION, amplitude=AMPLITUDE):
    """Generate samples for a sine wave."""
    n_samples = int(SAMPLE_RATE * duration)
    samples = []
    for i in range(n_samples):
        t = i / SAMPLE_RATE
        val = amplitude * math.sin(2 * math.pi * freq * t)
        samples.append(int(val * 32767))
    return samples


def make_multi_sine(freqs, duration=DURATION, amplitude=AMPLITUDE):
    """Generate samples for multiple sine waves summed."""
    n_samples = int(SAMPLE_RATE * duration)
    per_amp = amplitude / len(freqs)  # divide amplitude to avoid clipping
    samples = []
    for i in range(n_samples):
        t = i / SAMPLE_RATE
        val = sum(per_amp * math.sin(2 * math.pi * f * t) for f in freqs)
        samples.append(max(-32768, min(32767, int(val * 32767))))
    return samples


def make_sweep(f_start, f_end, duration=DURATION, amplitude=AMPLITUDE):
    """Generate a logarithmic frequency sweep."""
    n_samples = int(SAMPLE_RATE * duration)
    samples = []
    log_start = math.log(f_start)
    log_end = math.log(f_end)
    phase = 0
    for i in range(n_samples):
        t = i / SAMPLE_RATE
        # Log sweep: frequency increases exponentially
        frac = t / duration
        freq = math.exp(log_start + (log_end - log_start) * frac)
        phase += 2 * math.pi * freq / SAMPLE_RATE
        val = amplitude * math.sin(phase)
        samples.append(int(val * 32767))
    return samples


def write_wav(filename, samples):
    """Write samples to a 16-bit mono WAV file."""
    with wave.open(filename, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        data = struct.pack(f'<{len(samples)}h', *samples)
        wf.writeframes(data)
    print(f"  Written: {filename} ({len(samples)/SAMPLE_RATE:.1f}s, {len(samples)} samples)")


def gen_all(output_dir):
    """Generate all test WAV files."""
    os.makedirs(output_dir, exist_ok=True)

    # 1. Single tones at specific bars
    test_freqs = {
        'tone_100hz': 100,    # bar ~3  (bass)
        'tone_200hz': 200,    # bar ~12 (low-mid)
        'tone_440hz': 440,    # bar ~25 (A4, mid)
        'tone_1000hz': 1000,  # bar ~45 (mid-high)
        'tone_2000hz': 2000,  # bar ~65 (high)
    }
    print("Single tones:")
    for name, freq in test_freqs.items():
        # Find which bar this should light up
        bar = min(range(80), key=lambda k: abs(BIN_FREQS[k] - freq))
        print(f"  {freq} Hz -> expected bar {bar} (bin freq {BIN_FREQS[bar]:.0f} Hz)")
        samples = make_sine(freq)
        write_wav(os.path.join(output_dir, f'{name}.wav'), samples)

    # 2. Three tones simultaneously (chord)
    print("\nChord (200 + 440 + 1000 Hz):")
    chord_freqs = [200, 440, 1000]
    for f in chord_freqs:
        bar = min(range(80), key=lambda k: abs(BIN_FREQS[k] - f))
        print(f"  {f} Hz -> expected bar {bar}")
    samples = make_multi_sine(chord_freqs)
    write_wav(os.path.join(output_dir, 'chord_3tone.wav'), samples)

    # 3. Frequency sweep 65 -> 2500 Hz
    print("\nSweep 65-2500 Hz (should light bars left to right):")
    samples = make_sweep(65, 2500, duration=15)
    write_wav(os.path.join(output_dir, 'sweep_65_2500.wav'), samples)

    # 4. Bass + treble (two separated tones)
    print("\nBass + Treble (100 + 1500 Hz):")
    samples = make_multi_sine([100, 1500])
    write_wav(os.path.join(output_dir, 'bass_treble.wav'), samples)

    print(f"\nAll files in: {output_dir}/")
    print("Usage: python3 send_audio.py <port> <wav_file> --loop")


def main():
    parser = argparse.ArgumentParser(description='Generate test tones for spectrum analyzer')
    parser.add_argument('--test', nargs='+', help='Test type: sweep | single <freq> | chord <f1> <f2> ...')
    parser.add_argument('--output', '-o', default='examples/uart_audio/tools/test_audio',
                        help='Output directory (default: examples/uart_audio/tools/test_audio)')
    args = parser.parse_args()

    if args.test is None:
        gen_all(args.output)
    elif args.test[0] == 'sweep':
        os.makedirs(args.output, exist_ok=True)
        samples = make_sweep(65, 2500, duration=15)
        write_wav(os.path.join(args.output, 'sweep.wav'), samples)
    elif args.test[0] == 'single' and len(args.test) > 1:
        os.makedirs(args.output, exist_ok=True)
        freq = float(args.test[1])
        bar = min(range(80), key=lambda k: abs(BIN_FREQS[k] - freq))
        print(f"{freq} Hz -> expected bar {bar} (bin freq {BIN_FREQS[bar]:.0f} Hz)")
        samples = make_sine(freq)
        write_wav(os.path.join(args.output, f'tone_{int(freq)}hz.wav'), samples)
    elif args.test[0] == 'chord' and len(args.test) > 1:
        os.makedirs(args.output, exist_ok=True)
        freqs = [float(f) for f in args.test[1:]]
        for f in freqs:
            bar = min(range(80), key=lambda k: abs(BIN_FREQS[k] - f))
            print(f"  {f} Hz -> expected bar {bar}")
        samples = make_multi_sine(freqs)
        write_wav(os.path.join(args.output, 'chord.wav'), samples)
    else:
        parser.print_help()


if __name__ == '__main__':
    main()
