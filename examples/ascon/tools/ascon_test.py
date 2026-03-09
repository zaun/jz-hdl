#!/usr/bin/env python3
"""
Ascon-128 FPGA test tool — communicates with the ascon_top module over UART.

Protocol:
  TX to FPGA:
    Byte 0:      'E' (encrypt) or 'D' (decrypt)
    Bytes 1-16:  128-bit key (MSB first)
    Bytes 17-32: 128-bit nonce (MSB first)
    Byte 33:     Data length in bytes
    Bytes 34+:   Data (send in 8-byte blocks, read response between blocks)
    Decrypt:     + 16 bytes expected tag (send AFTER reading all plaintext)

  RX from FPGA:
    Bytes 0+:    Output data (same length as input, streamed per block)
    Next byte:   'K' (success) or 'F' (tag fail)
    Encrypt:     + 16 bytes tag

  IMPORTANT: The FPGA has no RX FIFO. Data must be sent in 8-byte blocks
  with the response read between each block to prevent RX data loss during
  S_TX_BLK transmission.

Usage:
  python3 ascon_test.py /dev/ttyUSB0 encrypt "Hello, Ascon!"
  python3 ascon_test.py /dev/ttyUSB0 decrypt <hex_ciphertext> <hex_tag>
  python3 ascon_test.py /dev/ttyUSB0 selftest
"""

import os
import sys
import time
import serial
import binascii


# Default key and nonce for testing
DEFAULT_KEY   = bytes.fromhex("000102030405060708090a0b0c0d0e0f")
DEFAULT_NONCE = bytes.fromhex("000102030405060708090a0b0c0d0e0f")


def sync_fpga(ser):
    """Flush FPGA state machine back to S_RX_CMD.

    Sends enough null bytes to complete any in-progress operation.
    Null bytes in S_RX_CMD are harmless (not 'E' or 'D').
    """
    ser.write(bytes(512))
    ser.flush()
    time.sleep(0.3)
    ser.reset_input_buffer()


def _send_blocks(ser, cmd_byte, key, nonce, data):
    """Send data in 8-byte blocks, interleaving TX/RX to avoid UART loss.

    The FPGA has no RX FIFO — bytes arriving during S_TX_BLK are dropped.
    We must read each block's response before sending the next block's data.

    Returns the streamed output bytes.
    """
    n = len(data)
    hdr = cmd_byte + key + nonce + bytes([n])

    if n == 0:
        ser.write(hdr)
        ser.flush()
        return b''

    # Send header + first block together (header is consumed before data)
    first = min(8, n)
    ser.write(hdr + data[0:first])
    ser.flush()
    out = ser.read(first)

    # Remaining blocks: send chunk, read response
    offset = first
    while offset < n:
        chunk = min(8, n - offset)
        ser.write(data[offset:offset+chunk])
        ser.flush()
        out += ser.read(chunk)
        offset += chunk

    return out


def send_encrypt(ser, key, nonce, plaintext):
    """Send encrypt command and return (status, ciphertext, tag)."""
    ct = _send_blocks(ser, b'E', key, nonce, plaintext)

    # Read status + 16-byte tag
    resp = ser.read(1 + 16)
    if len(resp) < 1:
        raise TimeoutError("No response from FPGA")
    status = chr(resp[0])
    tag = resp[1:17]
    return status, ct, tag


def send_decrypt(ser, key, nonce, ciphertext, tag):
    """Send decrypt command and return (status, plaintext).

    Tag is sent AFTER reading plaintext to avoid RX data loss.
    """
    pt = _send_blocks(ser, b'D', key, nonce, ciphertext)

    # Now FPGA is in S_RX_TAG — safe to send tag
    ser.write(tag)
    ser.flush()

    # Read status byte
    resp = ser.read(1)
    if len(resp) < 1:
        raise TimeoutError("No response from FPGA")
    status = chr(resp[0])
    return status, pt


def _chunk_nonce(base_nonce, chunk_idx):
    """Derive per-chunk nonce: XOR chunk index into last 4 bytes."""
    import struct
    n = bytearray(base_nonce)
    idx_bytes = struct.pack('>I', chunk_idx)
    for i in range(4):
        n[12 + i] ^= idx_bytes[i]
    return bytes(n)


def encrypt_chunked(ser, key, nonce, plaintext):
    """Encrypt data of any size by splitting into 255-byte chunks.

    Each chunk uses a unique nonce derived from base nonce + chunk index.
    Returns (ciphertext_bytes, list_of_tags).
    """
    ct_all = b''
    tags = []
    offset = 0
    chunk_idx = 0

    while offset < len(plaintext) or chunk_idx == 0:
        chunk = plaintext[offset:offset+255]
        cn = _chunk_nonce(nonce, chunk_idx)
        status, ct, tag = send_encrypt(ser, key, cn, chunk)
        if status != 'K':
            raise RuntimeError(f"Encrypt failed on chunk {chunk_idx}: '{status}'")
        ct_all += ct
        tags.append(tag)
        offset += 255
        chunk_idx += 1

    return ct_all, tags


def decrypt_chunked(ser, key, nonce, blob):
    """Decrypt a chunked encrypted file.

    File format: 4-byte LE total length, then per chunk:
      2-byte LE chunk_ct_len + chunk_ct + 16-byte tag
    """
    import struct
    total_len = struct.unpack('<I', blob[0:4])[0]
    pos = 4
    pt_all = b''
    chunk_idx = 0

    while pos < len(blob):
        chunk_ct_len = struct.unpack('<H', blob[pos:pos+2])[0]
        pos += 2
        ct = blob[pos:pos+chunk_ct_len]
        pos += chunk_ct_len
        tag = blob[pos:pos+16]
        pos += 16

        cn = _chunk_nonce(nonce, chunk_idx)
        status, pt = send_decrypt(ser, key, cn, ct, tag)
        if status != 'K':
            raise RuntimeError(f"Decrypt failed on chunk {chunk_idx}: tag mismatch")
        pt_all += pt
        chunk_idx += 1

    return pt_all[:total_len]


def selftest(ser):
    """Run basic self-test: encrypt then decrypt, verify round-trip."""
    print("=== Ascon-128 FPGA Self-Test ===\n")

    tests = [
        ("Empty message",    b""),
        ("8 bytes (1 block)", b"ABCDEFGH"),
        ("5 bytes (partial)", b"Hello"),
        ("16 bytes (2 blks)", b"0123456789ABCDEF"),
        ("13 bytes (1+part)", b"Hello, World!"),
    ]

    key = DEFAULT_KEY
    nonce = DEFAULT_NONCE
    passed = 0

    for name, pt in tests:
        print(f"Test: {name}")
        print(f"  Plaintext:  {pt.hex()} ({pt})")

        # Encrypt
        status, ct, tag = send_encrypt(ser, key, nonce, pt)
        print(f"  Ciphertext: {ct.hex()}")
        print(f"  Tag:        {tag.hex()}")
        print(f"  Enc status: {status}")

        if status != 'K':
            print(f"  FAIL: encrypt returned '{status}'\n")
            continue

        # Decrypt with correct tag
        status2, pt2 = send_decrypt(ser, key, nonce, ct, tag)
        print(f"  Decrypted:  {pt2.hex()} ({pt2})")
        print(f"  Dec status: {status2}")

        if status2 != 'K':
            print(f"  FAIL: decrypt returned '{status2}'\n")
            continue

        if pt2 != pt:
            print(f"  FAIL: plaintext mismatch!\n")
            continue

        # Decrypt with wrong tag (should fail)
        bad_tag = bytes(16)
        status3, _ = send_decrypt(ser, key, nonce, ct, bad_tag)
        print(f"  Bad tag:    {status3} (expected F)")

        if status3 != 'F':
            print(f"  FAIL: bad tag should return 'F'\n")
            continue

        print(f"  PASS\n")
        passed += 1

    print(f"Results: {passed}/{len(tests)} passed")
    return passed == len(tests)


def main():
    if len(sys.argv) < 3:
        print("Usage:")
        print("  ascon_test.py <port> selftest")
        print("  ascon_test.py <port> encrypt <file_or_text>")
        print("  ascon_test.py <port> decrypt <file | hex_ct hex_tag>")
        sys.exit(1)

    port = sys.argv[1]
    cmd = sys.argv[2]

    ser = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)
    sync_fpga(ser)

    key = DEFAULT_KEY
    nonce = DEFAULT_NONCE

    if cmd == "selftest":
        ok = selftest(ser)
        sys.exit(0 if ok else 1)

    elif cmd == "encrypt":
        if len(sys.argv) < 4:
            pt = b""
        elif os.path.isfile(sys.argv[3]):
            with open(sys.argv[3], "rb") as f:
                pt = f.read()
        else:
            pt = sys.argv[3].encode()

        ct_all, tags = encrypt_chunked(ser, key, nonce, pt)
        print(f"Encrypted {len(pt)} bytes in {len(tags)} chunk(s)", file=sys.stderr)

        if sys.stdout.isatty():
            print(f"Ciphertext: {ct_all.hex()}")
            for i, t in enumerate(tags):
                print(f"Tag[{i}]:     {t.hex()}")
        else:
            # File format: 4-byte LE total length, then per chunk:
            #   2-byte LE chunk_ct_len + chunk_ct + 16-byte tag
            import struct
            sys.stdout.buffer.write(struct.pack('<I', len(pt)))
            offset = 0
            for t in tags:
                chunk_len = min(255, len(ct_all) - offset)
                sys.stdout.buffer.write(struct.pack('<H', chunk_len))
                sys.stdout.buffer.write(ct_all[offset:offset+chunk_len])
                sys.stdout.buffer.write(t)
                offset += chunk_len

    elif cmd == "decrypt":
        if len(sys.argv) > 3 and os.path.isfile(sys.argv[3]):
            with open(sys.argv[3], "rb") as f:
                blob = f.read()
            pt = decrypt_chunked(ser, key, nonce, blob)
        else:
            ct = bytes.fromhex(sys.argv[3])
            tag = bytes.fromhex(sys.argv[4])
            status, pt = send_decrypt(ser, key, nonce, ct, tag)
            if status != 'K':
                print(f"Decrypt FAILED: tag mismatch", file=sys.stderr)
                sys.exit(1)

        print(f"Decrypted {len(pt)} bytes", file=sys.stderr)

        if sys.stdout.isatty():
            print(f"Plaintext: {pt.hex()}")
            try:
                print(f"Text:      {pt.decode()}")
            except UnicodeDecodeError:
                pass
        else:
            sys.stdout.buffer.write(pt)

    else:
        print(f"Unknown command: {cmd}")
        sys.exit(1)

    ser.close()


if __name__ == "__main__":
    main()
