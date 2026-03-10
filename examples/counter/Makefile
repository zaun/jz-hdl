.PHONY: check tang_nano_9k tang_nano_20k build simulate synthesis load clean

JZ_HDL := ../../compiler/build/jz-hdl
PYTHON3 := $(shell which python3)
GOWIN_PATH := $(shell dirname $(shell command -v gowin_pack))
YOSYS := yosys

# ---- Board selection from command line ----

BOARD_GOAL := $(firstword $(MAKECMDGOALS))

ifeq ($(BOARD_GOAL),tang_nano_9k)
BOARD := tangnano9k
FAMILY := GW1N-9C
DEVICE := GW1NR-LV9QN88PC6/I5
HDL := src/tang_nano_9k.jz
endif

ifeq ($(BOARD_GOAL),tang_nano_20k)
BOARD := tangnano20k
FAMILY := GW2A-18C
DEVICE := GW2AR-LV18QN88C8/I7
HDL := src/tang_nano_20k.jz
endif

check:
	@command -v $(YOSYS) >/dev/null 2>&1 && echo "Yosys: Found" || echo "Yosys: Not found"
	@test -f $(JZ_HDL) && echo "jz-hdl: Found" || echo "jz-hdl: Not found"
	@command echo "\nBoards:"
	@command echo "tang_nano_9k"
	@command echo "tang_nano_20k"
	@command echo "\nCommands:"
	@command echo "make <board> build     - Transpile Verilog"
	@command echo "make <board> synthesis - Synthesis the Verilog"
	@command echo "make <board> load      - Load bytestrem on board"

# Dummy board targets (so make doesn't error)
tang_nano_9k:
	@true

tang_nano_20k:
	@true

simulate:
	@mkdir -p out
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --simulate src/simulation.jz --vcd -o out/simulation.vcd
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --simulate src/simulation.jz --fst -o out/simulation.fst

build:
	@test -n "$(HDL)" || (echo "ERROR: board not selected"; exit 1)
	@mkdir -p out
	@mkdir -p reports
	@command echo "Building:"
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --explain --lint $(HDL)
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --alias-report $(HDL) -o reports/alias.txt
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --tristate-report $(HDL) -o reports/tristate.txt
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --memory-report $(HDL) -o reports/memory.txt
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --ast $(HDL) -o out/project.ast
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --ir $(HDL) -o out/project.ir
	$(JZ_HDL) --sandbox-root=./ --allow-traversal --verilog $(HDL) -o out/project.v --cst out/project.cst --sdc out/project.sdc

synthesis: build
	@command echo "Synthesis:"
	yosys -q -p "read_verilog ./out/project.v; synth_gowin -top top -json ./out/synthesis.json"
	nextpnr-himbaechel --json out/synthesis.json --write out/bitstream.json --device $(DEVICE) --vopt family=$(FAMILY) --vopt cst=./out/project.cst --sdc ./out/project.sdc
	PATH="$(GOWIN_PATH):$(PATH)" gowin_pack -d $(FAMILY) -o out/project.fs out/bitstream.json

load: synthesis
	@command echo "Loading:"
	openFPGALoader -b $(BOARD) out/project.fs -f

clean:
	rm -f project.ast project.cst project.fs project.ir project.v synthesis.json bitstream.json

# Swallow extra goal
%:
	@true
