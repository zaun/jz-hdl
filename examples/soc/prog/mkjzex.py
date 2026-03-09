#!/usr/bin/env python3
"""
mkjzex.py - Build JZEX v2 binary from ELF + raw binary

Usage: python3 mkjzex.py program.elf program_raw.bin program.bin

JZEX v2 header (12 bytes):
  Offset  Size  Field
  0       4     Magic: JZEX (0x4A 0x5A 0x45 0x58)
  4       2     Entry point offset (LE)
  6       2     Help string offset (LE)
  8       2     Help string size (LE)
  10      1     Flags (reserved, 0x00)
  11      1     Version (0x01)
"""

import struct
import sys


def read_elf_symbols(elf_path):
    """Parse ELF symbol table to find __help_start and __help_end."""
    with open(elf_path, 'rb') as f:
        data = f.read()

    # ELF header
    if data[:4] != b'\x7fELF':
        print(f"Error: {elf_path} is not an ELF file", file=sys.stderr)
        sys.exit(1)

    ei_class = data[4]  # 1=32-bit, 2=64-bit
    ei_data = data[5]   # 1=LE, 2=BE
    endian = '<' if ei_data == 1 else '>'

    if ei_class == 1:
        # 32-bit ELF
        e_shoff = struct.unpack_from(endian + 'I', data, 32)[0]
        e_shentsize = struct.unpack_from(endian + 'H', data, 46)[0]
        e_shnum = struct.unpack_from(endian + 'H', data, 48)[0]
        e_shstrndx = struct.unpack_from(endian + 'H', data, 50)[0]
    else:
        print("Error: only 32-bit ELF supported", file=sys.stderr)
        sys.exit(1)

    # Find symbol table and string table sections
    symtab_off = None
    symtab_size = None
    symtab_entsize = None
    strtab_off = None

    for i in range(e_shnum):
        sh = e_shoff + i * e_shentsize
        sh_type = struct.unpack_from(endian + 'I', data, sh + 4)[0]
        if sh_type == 2:  # SHT_SYMTAB
            symtab_off = struct.unpack_from(endian + 'I', data, sh + 16)[0]
            symtab_size = struct.unpack_from(endian + 'I', data, sh + 20)[0]
            symtab_entsize = struct.unpack_from(endian + 'I', data, sh + 36)[0]
            # sh_link points to string table
            strtab_idx = struct.unpack_from(endian + 'I', data, sh + 24)[0]
            strtab_sh = e_shoff + strtab_idx * e_shentsize
            strtab_off = struct.unpack_from(endian + 'I', data, strtab_sh + 16)[0]
            break

    symbols = {}
    if symtab_off is not None and strtab_off is not None:
        num_syms = symtab_size // symtab_entsize
        for i in range(num_syms):
            sym = symtab_off + i * symtab_entsize
            st_name = struct.unpack_from(endian + 'I', data, sym)[0]
            st_value = struct.unpack_from(endian + 'I', data, sym + 4)[0]
            # Read symbol name from string table
            name_start = strtab_off + st_name
            name_end = data.index(b'\x00', name_start)
            name = data[name_start:name_end].decode('ascii', errors='replace')
            if name in ('__help_start', '__help_end'):
                symbols[name] = st_value

    return symbols


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} program.elf program_raw.bin output.bin",
              file=sys.stderr)
        sys.exit(1)

    elf_path = sys.argv[1]
    raw_path = sys.argv[2]
    out_path = sys.argv[3]

    # Read symbols from ELF
    symbols = read_elf_symbols(elf_path)

    # Read raw binary
    with open(raw_path, 'rb') as f:
        raw_data = f.read()

    # Header size
    HEADER_SIZE = 12

    # Entry point: right after header
    entry_offset = HEADER_SIZE

    # Help string location (relative to start of file)
    help_start = symbols.get('__help_start', 0)
    help_end = symbols.get('__help_end', 0)
    help_size = help_end - help_start

    if help_size > 0:
        help_offset = HEADER_SIZE + help_start
    else:
        help_offset = 0
        help_size = 0

    # Build header
    header = struct.pack('<4sHHHBB',
                         b'JZEX',
                         entry_offset,
                         help_offset,
                         help_size,
                         0x00,   # flags (reserved)
                         0x01)   # version

    # Write output
    with open(out_path, 'wb') as f:
        f.write(header)
        f.write(raw_data)


if __name__ == '__main__':
    main()
