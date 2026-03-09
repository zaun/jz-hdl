#!/usr/bin/env python3
import sys
import re

OPCODES = {
    'NOP'  : 0x00,
    'LDI'  : 0x01,
    'LDA'  : 0x02,
    'STA'  : 0x03,
    'ADD'  : 0x04,
    'SUB'  : 0x05,
    'JMP'  : 0x06,
    'JZ'   : 0x07,
    'LDIN' : 0x08,
    'STOU' : 0x09,
    'WAI'  : 0x0A,
    'HLT'  : 0xFF,
}

def parse_arg(tok, labels):
    if tok in labels:
        return labels[tok] & 0xFF
    if tok.startswith('0x'):
        return int(tok, 16) & 0xFF
    return int(tok) & 0xFF

def assemble(lines):
    cleaned = []
    labels = {}
    addr = 0
    mem_size = 0

    for ln in lines:
        # Check for @MEMSIZE(n) in the full line (comments included)
        match = re.search(r'@MEMSIZE\((\d+)\)', ln)
        if match:
            mem_size = int(match.group(1))

        ln = ln.split(';', 1)[0].strip()
        if not ln:
            continue
        if ln.endswith(':'):
            labels[ln[:-1]] = addr
            continue
        cleaned.append(ln)
        parts = ln.replace(',', ' ').split()
        mnem = parts[0].upper()
        addr += 1 + (1 if mnem in ('LDI','LDA','STA','ADD','SUB','JMP','JZ','WAI') else 0)

    out = []
    for ln in cleaned:
        parts = ln.replace(',', ' ').split()
        mnem = parts[0].upper()
        if mnem not in OPCODES:
            raise SystemExit(f'Unknown opcode: {mnem}')
        out.append(OPCODES[mnem])
        if mnem in ('LDI','LDA','STA','ADD','SUB','JMP','JZ','WAI'):
            if len(parts) < 2:
                raise SystemExit(f'Missing operand for {mnem}')
            out.append(parse_arg(parts[1], labels))

    # Pad with zeros if mem_size was specified
    if mem_size > len(out):
        out.extend([0] * (mem_size - len(out)))
        
    return out

def main():
    if len(sys.argv) < 3:
        print("usage: assembler.py in.asm out.mem")
        sys.exit(1)
    with open(sys.argv[1]) as f:
        lines = f.readlines()
    bytes_out = assemble(lines)
    with open(sys.argv[2], 'w') as f:
        for b in bytes_out:
            f.write("{:02x}\n".format(b))

if __name__ == '__main__':
    main()
