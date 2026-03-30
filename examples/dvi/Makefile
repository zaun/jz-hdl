.PHONY: help check tang_nano_9k tang_nano_20k pa35t-edu build simulate synthesis load fast_load clean

.DEFAULT_GOAL := help

JZ_HDL := ../../compiler/build/jz-hdl
PYTHON3 := $(shell which python3)
YOSYS := yosys
PNR ?= nextpnr-himbaechel

# ---- Board selection from command line ----

BOARD_GOAL := $(firstword $(MAKECMDGOALS))

# -- Gowin: Tang Nano 9K --
ifeq ($(BOARD_GOAL),tang_nano_9k)
ARCH := gowin
BOARD := tangnano9k
FAMILY := GW1N-9C
DEVICE := GW1NR-LV9QN88PC6/I5
HDL := src/tang_nano_9k.jz
endif

# -- Gowin: Tang Nano 20K --
ifeq ($(BOARD_GOAL),tang_nano_20k)
ARCH := gowin
BOARD := tangnano20k
FAMILY := GW2A-18C
DEVICE := GW2AR-LV18QN88C8/I7
HDL := src/tang_nano_20k.jz
endif

# -- Xilinx: PA35T-EDU (Artix-7 XC7A35T) --
ifeq ($(BOARD_GOAL),pa35t-edu)
ARCH := xilinx
BOARD := pa35t-edu
CABLE := ft232
DEVICE := xc7a35tfgg484-2
FPGA_PART := xc7a35tfgg484
XRAY_DB_DIR ?= $(HOME)/Development/fpga-dev/prjxray/database/artix7
HDL := src/pa35t-edu.jz
endif

# -- ICE40: IceSugar v1.5 --
ifeq ($(BOARD_GOAL),ice_sugar_v15)
ARCH := ice40
PNR := nextpnr-ice40
MOUNT := /Volumes/iCELink/
DEVICE := up5k
PACKAGE := sg48
HDL := src/ice_sugar_v15.jz
endif

# ---- Tool paths (resolved only when needed) ----

ifeq ($(ARCH),gowin)
GOWIN_PATH := $(shell dirname $(shell command -v gowin_pack 2>/dev/null) 2>/dev/null)
endif

# ---- Dummy board targets (so make doesn't error) ----
tang_nano_9k:
	@true

tang_nano_20k:
	@true

pa35t-edu:
	@true

help:
	@echo "Boards:"
	@echo "  tang_nano_9k"
	@echo "  tang_nano_20k"
	@echo "  pa35t-edu"
	@echo "  ice_sugar_v15"
	@echo ""
	@echo "Commands:"
	@echo "  make <board> build     - Transpile Verilog"
	@echo "  make <board> synthesis - Synthesis the Verilog"
	@echo "  make <board> load      - Load bitstream to flash (persistent)"
	@echo "  make <board> fast_load - Load bitstream to SRAM  (volatile, fast)"

check:
	@fail=0; \
	test -f $(JZ_HDL) && echo "jz-hdl:         Found" || { echo "jz-hdl:         Not found"; fail=1; }; \
	command -v $(YOSYS) >/dev/null 2>&1 && echo "Yosys:          Found" || { echo "Yosys:          Not found"; fail=1; }; \
	command -v $(PNR) >/dev/null 2>&1 && echo "PNR:            Found" || { echo "PNR:            Not found"; fail=1; }; \
	command -v openFPGALoader >/dev/null 2>&1 && echo "openFPGALoader: Found" || { echo "openFPGALoader: Not found"; fail=1; }; \
	$(CHECK_ARCH_TOOLS) \
	if [ $$fail -ne 0 ]; then echo ""; echo "ERROR: missing required tools"; exit 1; fi

ifeq ($(ARCH),gowin)
CHECK_ARCH_TOOLS = \
	command -v gowin_pack >/dev/null 2>&1 && echo "gowin_pack:     Found" || { echo "gowin_pack:     Not found"; fail=1; };
endif
ifeq ($(ARCH),ice40)
CHECK_ARCH_TOOLS = \
	command -v icepack >/dev/null 2>&1 && echo "icepack:        Found" || { echo "icepack:        Not found"; fail=1; };
endif
ifeq ($(ARCH),xilinx)
CHECK_ARCH_TOOLS = \
	command -v fasm2frames >/dev/null 2>&1 && echo "fasm2frames:    Found" || { echo "fasm2frames:    Not found"; fail=1; }; \
	command -v xc7frames2bit >/dev/null 2>&1 && echo "xc7frames2bit:  Found" || { echo "xc7frames2bit:  Not found"; fail=1; };
endif

# ---- Simulate: run the hardware in a simulation ----

simulate:
	@mkdir -p out
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --simulate src/simulation.jz --vcd -o out/simulation.vcd
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --simulate src/simulation.jz --fst -o out/simulation.fst
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --simulate src/simulation.jz --jzw -o out/simulation.jzw

# ---- Build: generate Verilog + constraints ----

build: check
	@test -n "$(HDL)" || (echo "ERROR: board not selected"; exit 1)
	@mkdir -p out
	@mkdir -p reports
	@echo "Building:"
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --explain --lint $(HDL)
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --alias-report $(HDL) -o reports/alias.txt
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --tristate-report $(HDL) -o reports/tristate.txt
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --memory-report $(HDL) -o reports/memory.txt
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --ast $(HDL) -o out/project.ast
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --ir $(HDL) -o out/project.ir
ifeq ($(ARCH),gowin)
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --verilog $(HDL) -o out/project.v --cst out/project.cst --sdc out/project.sdc
endif
ifeq ($(ARCH),ice40)
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --verilog $(HDL) -o out/project.v --pcf out/project.pcf --sdc out/project.sdc
endif
ifeq ($(ARCH),xilinx)
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --verilog $(HDL) -o out/project.v --xdc out/project.xdc --sdc out/project.sdc
endif

# ---- Synthesis: yosys + nextpnr + pack ----

synthesis: build
	@echo "Synthesis:"
ifeq ($(ARCH),gowin)
	$(YOSYS) -q -p "read_verilog ./out/project.v; synth_gowin -top top -json ./out/synthesis.json"
	$(PNR) --json out/synthesis.json --write out/bitstream.json --device $(DEVICE) --vopt family=$(FAMILY) --vopt cst=./out/project.cst --sdc ./out/project.sdc
	PATH="$(GOWIN_PATH):$(PATH)" gowin_pack -d $(FAMILY) -o out/project.fs out/bitstream.json
endif
ifeq ($(ARCH),ice40)
	$(YOSYS) -q -p "read_verilog ./out/project.v; synth_ice40 -top top -json ./out/synthesis.json"
	$(PNR) --$(DEVICE) --package $(PACKAGE) --json out/synthesis.json --pcf out/project.pcf --asc out/project.asc
	icepack out/project.asc out/project.bin
endif
ifeq ($(ARCH),xilinx)
	$(YOSYS) -q -p "read_verilog ./out/project.v; synth_xilinx -nocarry -top top -json ./out/synthesis.json"
	$(PNR) --device $(DEVICE) --json out/synthesis.json --vopt xdc=out/project.xdc --vopt fasm=out/project.fasm
	fasm2frames --db-root $(XRAY_DB_DIR) --part $(DEVICE) out/project.fasm > out/project.frames
	xc7frames2bit --part-file $(XRAY_DB_DIR)/$(DEVICE)/part.yaml --part-name $(DEVICE) --frm-file out/project.frames --output-file out/project.bit
endif

# ---- Load: program the board ----

load: synthesis
	@echo "Loading to flash:"
ifeq ($(ARCH),gowin)
	openFPGALoader -b $(BOARD) out/project.fs -f
endif
ifeq ($(ARCH),ice40)
	cp out/project.bin ${MOUNT}
endif
ifeq ($(ARCH),xilinx)
	openFPGALoader -c $(CABLE) --fpga-part $(FPGA_PART) out/project.bit -f
endif

fast_load: synthesis
ifeq ($(ARCH),gowin)
	@echo "ERROR: fast_load is not supported for Gowin boards"
	@exit 1
endif
ifeq ($(ARCH),ice40)
	@echo "ERROR: fast_load is not supported for Gowin boards"
	@exit 1
endif
ifeq ($(ARCH),xilinx)
	@echo "Loading to SRAM (volatile):"
	openFPGALoader -c $(CABLE) --fpga-part $(FPGA_PART) out/project.bit
endif

clean:
	rm -rf out reports

# Swallow extra goal
%:
	@true
