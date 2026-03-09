CROSS    := riscv64-unknown-elf-
CC       := $(CROSS)gcc
OBJCOPY  := $(CROSS)objcopy
OBJDUMP  := $(CROSS)objdump
SIZE     := $(CROSS)size

ARCH     := -march=rv32im_zicsr -mabi=ilp32

CFLAGS   := $(ARCH) -Os -ffreestanding -nostdlib -Wall
OUTDIR   := ../out

.PHONY: all clean

all: $(OUTDIR)/bios.hex $(OUTDIR)/kernel.bin $(OUTDIR)/ls.jze $(OUTDIR)/rm.jze $(OUTDIR)/cd.jze $(OUTDIR)/cat.jze $(OUTDIR)/vid.jze $(OUTDIR)/man.jze

# --- BIOS ---

$(OUTDIR)/bios_start.o: bios/start.S | $(OUTDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUTDIR)/bios.o: bios/bios.c | $(OUTDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUTDIR)/bios.elf: $(OUTDIR)/bios_start.o $(OUTDIR)/bios.o
	$(CC) $(ARCH) -nostdlib -T bios/link.ld -Wl,--no-relax -o $@ $^
	$(SIZE) $@

$(OUTDIR)/bios.bin: $(OUTDIR)/bios.elf
	$(OBJCOPY) -O binary $< $@

$(OUTDIR)/bios.hex: $(OUTDIR)/bios.bin
	python3 bin2hex.py $< $@ 4096
	@echo "Generated $@"

# --- Kernel ---

$(OUTDIR)/kernel_start.o: kernel/kernel_start.S | $(OUTDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUTDIR)/kernel.o: kernel/kernel.c | $(OUTDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUTDIR)/kernel.elf: $(OUTDIR)/kernel_start.o $(OUTDIR)/kernel.o
	$(CC) $(ARCH) -nostdlib -T kernel/kernel_link.ld -Wl,--no-relax -o $@ $^
	$(SIZE) $@

$(OUTDIR)/kernel.bin: $(OUTDIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@
	@echo "Kernel: $$(stat -f%z $@ 2>/dev/null || stat -c%s $@) bytes"

# --- ls (relocatable JZEX program) ---

$(OUTDIR)/ls_start.o: ls/start.S | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIE -c -o $@ $<

$(OUTDIR)/ls.o: ls/ls.c | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIE -c -o $@ $<

$(OUTDIR)/ls.elf: $(OUTDIR)/ls_start.o $(OUTDIR)/ls.o
	$(CC) $(ARCH) -nostdlib -fPIE -T ls/link.ld -Wl,--no-relax -o $@ $^
	$(SIZE) $@

$(OUTDIR)/ls_raw.bin: $(OUTDIR)/ls.elf
	$(OBJCOPY) -O binary $< $@

$(OUTDIR)/ls.jze: $(OUTDIR)/ls.elf $(OUTDIR)/ls_raw.bin
	@python3 mkjzex.py $(OUTDIR)/ls.elf $(OUTDIR)/ls_raw.bin $@
	@echo "ls.jze: $$(stat -f%z $@ 2>/dev/null || stat -c%s $@) bytes (JZEX v2)"

# --- rm (relocatable JZEX program) ---

$(OUTDIR)/rm_start.o: rm/start.S | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIE -c -o $@ $<

$(OUTDIR)/rm.o: rm/rm.c | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIE -c -o $@ $<

$(OUTDIR)/rm.elf: $(OUTDIR)/rm_start.o $(OUTDIR)/rm.o
	$(CC) $(ARCH) -nostdlib -fPIE -T rm/link.ld -Wl,--no-relax -o $@ $^
	$(SIZE) $@

$(OUTDIR)/rm_raw.bin: $(OUTDIR)/rm.elf
	$(OBJCOPY) -O binary $< $@

$(OUTDIR)/rm.jze: $(OUTDIR)/rm.elf $(OUTDIR)/rm_raw.bin
	@python3 mkjzex.py $(OUTDIR)/rm.elf $(OUTDIR)/rm_raw.bin $@
	@echo "rm.jze: $$(stat -f%z $@ 2>/dev/null || stat -c%s $@) bytes (JZEX v2)"

# --- cd (relocatable JZEX program) ---

$(OUTDIR)/cd_start.o: cd/start.S | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIE -c -o $@ $<

$(OUTDIR)/cd.o: cd/cd.c | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIE -c -o $@ $<

$(OUTDIR)/cd.elf: $(OUTDIR)/cd_start.o $(OUTDIR)/cd.o
	$(CC) $(ARCH) -nostdlib -fPIE -T cd/link.ld -Wl,--no-relax -o $@ $^
	$(SIZE) $@

$(OUTDIR)/cd_raw.bin: $(OUTDIR)/cd.elf
	$(OBJCOPY) -O binary $< $@

$(OUTDIR)/cd.jze: $(OUTDIR)/cd.elf $(OUTDIR)/cd_raw.bin
	@python3 mkjzex.py $(OUTDIR)/cd.elf $(OUTDIR)/cd_raw.bin $@
	@echo "cd.jze: $$(stat -f%z $@ 2>/dev/null || stat -c%s $@) bytes (JZEX v2)"

# --- cat (relocatable JZEX program) ---

$(OUTDIR)/cat_start.o: cat/start.S | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIE -c -o $@ $<

$(OUTDIR)/cat.o: cat/cat.c | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIE -c -o $@ $<

$(OUTDIR)/cat.elf: $(OUTDIR)/cat_start.o $(OUTDIR)/cat.o
	$(CC) $(ARCH) -nostdlib -fPIE -T cat/link.ld -Wl,--no-relax -o $@ $^
	$(SIZE) $@

$(OUTDIR)/cat_raw.bin: $(OUTDIR)/cat.elf
	$(OBJCOPY) -O binary $< $@

$(OUTDIR)/cat.jze: $(OUTDIR)/cat.elf $(OUTDIR)/cat_raw.bin
	@python3 mkjzex.py $(OUTDIR)/cat.elf $(OUTDIR)/cat_raw.bin $@
	@echo "cat.jze: $$(stat -f%z $@ 2>/dev/null || stat -c%s $@) bytes (JZEX v2)"

# --- vid (relocatable JZEX program) ---

$(OUTDIR)/vid_start.o: vid/start.S | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIE -c -o $@ $<

$(OUTDIR)/vid.o: vid/vid.c | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIE -c -o $@ $<

$(OUTDIR)/vid.elf: $(OUTDIR)/vid_start.o $(OUTDIR)/vid.o
	$(CC) $(ARCH) -nostdlib -fPIE -T vid/link.ld -Wl,--no-relax -o $@ $^
	$(SIZE) $@

$(OUTDIR)/vid_raw.bin: $(OUTDIR)/vid.elf
	$(OBJCOPY) -O binary $< $@

$(OUTDIR)/vid.jze: $(OUTDIR)/vid.elf $(OUTDIR)/vid_raw.bin
	@python3 mkjzex.py $(OUTDIR)/vid.elf $(OUTDIR)/vid_raw.bin $@
	@echo "vid.jze: $$(stat -f%z $@ 2>/dev/null || stat -c%s $@) bytes (JZEX v2)"

# --- man (relocatable JZEX program) ---

$(OUTDIR)/man_start.o: man/start.S | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIE -c -o $@ $<

$(OUTDIR)/man.o: man/man.c | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIE -c -o $@ $<

$(OUTDIR)/man.elf: $(OUTDIR)/man_start.o $(OUTDIR)/man.o
	$(CC) $(ARCH) -nostdlib -fPIE -T man/link.ld -Wl,--no-relax -o $@ $^
	$(SIZE) $@

$(OUTDIR)/man_raw.bin: $(OUTDIR)/man.elf
	$(OBJCOPY) -O binary $< $@

$(OUTDIR)/man.jze: $(OUTDIR)/man.elf $(OUTDIR)/man_raw.bin
	@python3 mkjzex.py $(OUTDIR)/man.elf $(OUTDIR)/man_raw.bin $@
	@echo "man.jze: $$(stat -f%z $@ 2>/dev/null || stat -c%s $@) bytes (JZEX v2)"

# --- Utilities ---

$(OUTDIR)/bios.dis: $(OUTDIR)/bios.elf
	$(OBJDUMP) -d -M no-aliases $< > $@

$(OUTDIR)/kernel.dis: $(OUTDIR)/kernel.elf
	$(OBJDUMP) -d -M no-aliases $< > $@

$(OUTDIR)/ls.dis: $(OUTDIR)/ls.elf
	$(OBJDUMP) -d -M no-aliases $< > $@

$(OUTDIR)/rm.dis: $(OUTDIR)/rm.elf
	$(OBJDUMP) -d -M no-aliases $< > $@

$(OUTDIR)/cd.dis: $(OUTDIR)/cd.elf
	$(OBJDUMP) -d -M no-aliases $< > $@

$(OUTDIR)/cat.dis: $(OUTDIR)/cat.elf
	$(OBJDUMP) -d -M no-aliases $< > $@

$(OUTDIR)/vid.dis: $(OUTDIR)/vid.elf
	$(OBJDUMP) -d -M no-aliases $< > $@

$(OUTDIR)/man.dis: $(OUTDIR)/man.elf
	$(OBJDUMP) -d -M no-aliases $< > $@

SDCARD   := $(OUTDIR)/sdcard

sdcard: all
	@mkdir -p $(SDCARD)
	@cp $(OUTDIR)/kernel.bin $(SDCARD)/KERNEL.BIN
	@cp $(OUTDIR)/ls.jze     $(SDCARD)/LS.JZE
	@cp $(OUTDIR)/rm.jze     $(SDCARD)/RM.JZE
	@cp $(OUTDIR)/cd.jze     $(SDCARD)/CD.JZE
	@cp $(OUTDIR)/cat.jze    $(SDCARD)/CAT.JZE
	@cp $(OUTDIR)/vid.jze    $(SDCARD)/VID.JZE
	@cp $(OUTDIR)/man.jze    $(SDCARD)/MAN.JZE
	@echo "SD card image ready in $(SDCARD)/"
	@ls -la $(SDCARD)/

dis: $(OUTDIR)/bios.dis
	@cat $<

$(OUTDIR):
	mkdir -p $(OUTDIR)

clean:
	rm -f $(OUTDIR)/bios_start.o $(OUTDIR)/bios.o
	rm -f $(OUTDIR)/bios.elf $(OUTDIR)/bios.bin $(OUTDIR)/bios.hex $(OUTDIR)/bios.dis
	rm -f $(OUTDIR)/kernel_start.o $(OUTDIR)/kernel.o
	rm -f $(OUTDIR)/kernel.elf $(OUTDIR)/kernel.bin $(OUTDIR)/kernel.dis
	rm -f $(OUTDIR)/ls_start.o $(OUTDIR)/ls.o
	rm -f $(OUTDIR)/ls.elf $(OUTDIR)/ls_raw.bin $(OUTDIR)/ls.jze $(OUTDIR)/ls.dis
	rm -f $(OUTDIR)/rm_start.o $(OUTDIR)/rm.o
	rm -f $(OUTDIR)/rm.elf $(OUTDIR)/rm_raw.bin $(OUTDIR)/rm.jze $(OUTDIR)/rm.dis
	rm -f $(OUTDIR)/cd_start.o $(OUTDIR)/cd.o
	rm -f $(OUTDIR)/cd.elf $(OUTDIR)/cd_raw.bin $(OUTDIR)/cd.jze $(OUTDIR)/cd.dis
	rm -f $(OUTDIR)/cat_start.o $(OUTDIR)/cat.o
	rm -f $(OUTDIR)/cat.elf $(OUTDIR)/cat_raw.bin $(OUTDIR)/cat.jze $(OUTDIR)/cat.dis
	rm -f $(OUTDIR)/vid_start.o $(OUTDIR)/vid.o
	rm -f $(OUTDIR)/vid.elf $(OUTDIR)/vid_raw.bin $(OUTDIR)/vid.jze $(OUTDIR)/vid.dis
	rm -f $(OUTDIR)/man_start.o $(OUTDIR)/man.o
	rm -f $(OUTDIR)/man.elf $(OUTDIR)/man_raw.bin $(OUTDIR)/man.jze $(OUTDIR)/man.dis
	rm -rf $(SDCARD)
