#!/usr/bin/env python3
"""
UART Audio Sender for JZ-HDL UART Audio Example

Sends 16-bit mono PCM audio to the FPGA over UART at ~1.5 Mbaud.
Audio is resampled to 48kHz mono if needed.

Protocol (Python -> FPGA):
    [LEN]  1 byte:  number of data bytes (must be even)
    [DATA] N bytes: 16-bit signed samples, little-endian
    [PAR]  1 byte:  XOR of all data bytes

Protocol (FPGA -> Python):
    [ACK]  1 byte:  bits[7:1] = buffer fill (0-127), bit[0] = status (0=OK, 1=RESEND)

Pipelined sender: packets are sent continuously without waiting for individual
ACKs. ACKs are read non-blocking between sends for flow control only. This
avoids the USB-serial round-trip latency bottleneck (~1ms per ACK).

Usage:
    python3 send_audio.py <serial_port> <wav_file> [--baud 1485000] [--loop]
"""

import argparse
import struct
import sys
import time
import wave

import serial


BAUD_DEFAULT = 1_485_000  # Must match FPGA UART config (~1.5 Mbaud)
SAMPLES_PER_PACKET = 64   # 64 samples = 128 data bytes per packet
SAMPLE_RATE = 48_000      # FPGA playback rate
HIGH_WATER = 100          # buffer fill level (0-127) to pause sending
LOW_WATER = 60            # buffer fill level to resume sending


def load_wav(path: str) -> list[int]:
    """Load a WAV file and return 16-bit mono signed samples at 48kHz."""
    with wave.open(path, "rb") as wf:
        n_channels = wf.getnchannels()
        sample_width = wf.getsampwidth()
        framerate = wf.getframerate()
        n_frames = wf.getnframes()

        raw = wf.readframes(n_frames)

    print(f"Input: {n_channels}ch, {sample_width * 8}-bit, {framerate}Hz, "
          f"{n_frames} frames ({n_frames / framerate:.1f}s)")

    # Decode samples
    if sample_width == 1:
        samples = [((b - 128) << 8) for b in raw]
        if n_channels == 2:
            samples = [(samples[i] + samples[i + 1]) // 2
                       for i in range(0, len(samples), 2)]
    elif sample_width == 2:
        fmt = f"<{len(raw) // 2}h"
        all_samples = list(struct.unpack(fmt, raw))
        if n_channels == 2:
            samples = [(all_samples[i] + all_samples[i + 1]) // 2
                       for i in range(0, len(all_samples), 2)]
        elif n_channels == 1:
            samples = all_samples
        else:
            samples = all_samples[::n_channels]
    elif sample_width == 3:
        samples_24 = []
        for i in range(0, len(raw), 3):
            val = int.from_bytes(raw[i : i + 3], "little", signed=True)
            samples_24.append(val >> 8)
        if n_channels == 2:
            samples = [(samples_24[i] + samples_24[i + 1]) // 2
                       for i in range(0, len(samples_24), 2)]
        elif n_channels == 1:
            samples = samples_24
        else:
            samples = samples_24[::n_channels]
    else:
        print(f"Unsupported sample width: {sample_width}", file=sys.stderr)
        sys.exit(1)

    # Resample to 48kHz if needed
    if framerate != SAMPLE_RATE:
        print(f"Resampling {framerate}Hz -> {SAMPLE_RATE}Hz...")
        ratio = framerate / SAMPLE_RATE
        new_len = int(len(samples) / ratio)
        resampled = []
        for i in range(new_len):
            src_pos = i * ratio
            idx = int(src_pos)
            frac = src_pos - idx
            if idx + 1 < len(samples):
                val = int(samples[idx] * (1 - frac) + samples[idx + 1] * frac)
            else:
                val = samples[idx]
            resampled.append(max(-32768, min(32767, val)))
        samples = resampled

    duration = len(samples) / SAMPLE_RATE
    print(f"Output: {len(samples)} samples at {SAMPLE_RATE}Hz ({duration:.1f}s)")
    return samples


def make_packet(samples: list[int]) -> bytes:
    """Build a UART packet from a list of 16-bit signed samples."""
    data = b"".join(struct.pack("<h", s) for s in samples)
    length = len(data)
    assert 0 < length <= 255, f"Packet too large: {length} bytes"

    parity = 0
    for b in data:
        parity ^= b

    return bytes([length]) + data + bytes([parity])


def drain_acks(ser) -> int:
    """Read all pending ACK bytes, return latest fill level (or -1 if none)."""
    last_fill = -1
    while ser.in_waiting:
        data = ser.read(ser.in_waiting)
        if data:
            # Use the last ACK byte for fill level
            last_fill = (data[-1] >> 1) & 0x7F
    return last_fill


def send_audio(port: str, wav_path: str, baud: int, loop: bool):
    """Stream audio to FPGA over UART with pipelined packet sending."""
    samples = load_wav(wav_path)
    if not samples:
        print("No samples to send!", file=sys.stderr)
        return

    ser = serial.Serial(port, baud, timeout=0)
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    print(f"Opened {port} at {baud} baud")
    print("Streaming audio... (Ctrl+C to stop)")

    sample_idx = 0
    packets_sent = 0
    last_fill = 0
    start_time = time.time()

    try:
        while True:
            # Check for fill-level updates (non-blocking)
            # FPGA sends these every ~1ms while idle, plus after each packet
            fill = drain_acks(ser)
            if fill >= 0:
                last_fill = fill

            # Flow control: if buffer is full, wait for FPGA to report drain
            if last_fill > HIGH_WATER:
                time.sleep(0.001)
                continue

            # Get next chunk of samples
            end_idx = sample_idx + SAMPLES_PER_PACKET
            if end_idx > len(samples):
                if loop:
                    chunk = samples[sample_idx:] + samples[:end_idx - len(samples)]
                    sample_idx = end_idx - len(samples)
                else:
                    chunk = samples[sample_idx:]
                    if not chunk:
                        break
                    sample_idx = len(samples)
            else:
                chunk = samples[sample_idx:end_idx]
                sample_idx = end_idx

            # Send packet (no ACK wait — pipelined)
            packet = make_packet(chunk)
            ser.write(packet)
            packets_sent += 1

            # Progress display
            if packets_sent % 200 == 0:
                elapsed = time.time() - start_time
                fill_pct = last_fill * 100 // 127
                if not loop:
                    progress = sample_idx / len(samples) * 100
                    total_dur = len(samples) / SAMPLE_RATE
                    print(f"\r  {progress:5.1f}% | "
                          f"buf: {fill_pct:3d}% | "
                          f"pkts: {packets_sent} | "
                          f"{elapsed:.0f}s / {total_dur:.0f}s",
                          end="", flush=True)
                else:
                    print(f"\r  looping | "
                          f"buf: {fill_pct:3d}% | "
                          f"pkts: {packets_sent} | "
                          f"elapsed: {elapsed:.0f}s",
                          end="", flush=True)

    except KeyboardInterrupt:
        print("\nStopped by user")
    finally:
        elapsed = time.time() - start_time
        print(f"\nSent {packets_sent} packets ({sample_idx} samples) "
              f"in {elapsed:.1f}s")
        ser.close()


def main():
    parser = argparse.ArgumentParser(
        description="Stream audio to FPGA over UART"
    )
    parser.add_argument("port", help="Serial port (e.g. /dev/ttyUSB0, COM3)")
    parser.add_argument("wav_file", help="WAV file to stream")
    parser.add_argument("--baud", type=int, default=BAUD_DEFAULT,
                        help=f"Baud rate (default: {BAUD_DEFAULT})")
    parser.add_argument("--loop", action="store_true",
                        help="Loop audio continuously")
    args = parser.parse_args()

    send_audio(args.port, args.wav_file, args.baud, args.loop)


if __name__ == "__main__":
    main()
