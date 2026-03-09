# SOC Clock Architecture & DQCE Investigation

## Clock Tree

```
27 MHz Crystal (SCLK, pin 4)
  └─ rPLL (IDIV=3, FBDIV=54, ODIV=2)
       │   VCO = 27 * 55 * 2 / 4 = 742.5 MHz
       │   Output = 742.5 / 2 = 371.25 MHz
       │
       └─ serial_clk (371.25 MHz)
            ├─ CLKDIV (DIV_MODE=5) → pixel_clk (74.25 MHz)
            │     Used by: video_out, terminal_fb (read port), TMDS serializers
            │
            └─ CLKDIV (DIV_MODE=5) → sys_clk (74.25 MHz)
                  Used by: CPU, ROM, RAM, LEDs, UART, SDRAM, SD card, terminal_fb (write port)
```

Both pixel_clk and sys_clk are 74.25 MHz but are separate clock domains (independent CLKDIV outputs). The video module uses a 2-stage CDC synchronizer for the `video_mode` signal crossing from sys_clk to pixel_clk.

## DQCE Resource Issue

### Background

The GW2AR-18 has 24 DQCE (Dynamic Clock Enable) primitives. In the JZ-HDL chip data, the `BUF` clock generator type maps to `DQCE` with `CE=1'b1`, acting as a global clock buffer.

### Investigation: Adding a BUF for sys_clk

**Goal:** Put sys_clk on the global clock network (GCLK) for low-skew distribution.

**Attempt:** Added a `BUF` stage in `CLOCK_GEN` after the sys_clk CLKDIV:
```
CLKDIV {
    IN        serial_clk;
    OUT BASE  sys_clk_local;
    CONFIG { DIV_MODE = 5; };
};
BUF {
    IN        sys_clk_local;
    OUT BASE  sys_clk;
};
```

**Result:** DQCE usage went to 25/24 (104%), causing routing failures:
```
Warning: Failed to route net 'sys_clk' from X27Y54/CLKDIV_HCLK0_SECT1_CLKOUT to ...
```

### Root Cause

nextpnr-himbaechel automatically inserts 24 DQCE instances for clock enable management during placement. Adding our explicit DQCE (from the BUF) made 25, overflowing the resource.

Key observations:
- **Without BUF:** DQCE = 0/24. nextpnr routes clocks without needing DQCEs.
- **With BUF:** DQCE = 25/24. Our 1 explicit DQCE triggers nextpnr to also insert 24 more for clock enable management, overflowing the resource.
- **`-nodffe` flag:** Prevents yosys from inferring flip-flops with clock enable. Added ~3000 LUTs but did NOT reduce DQCE count — confirming the DQCEs come from nextpnr, not yosys.
- **`-noclkbuf` flag:** Not available in this version of `synth_gowin`.

### Current Status

BUF removed. sys_clk routes from CLKDIV output using local/regional clock routing. This works but may have higher clock skew than a global network clock.

Timing results without BUF:
- `sys_clk`: 120.99 MHz max (PASS at 74.25 MHz, ~63% margin)
- `pixel_clk`: 135.98 MHz max (PASS at 74.25 MHz, ~83% margin)

### Future Options

1. **Accept local routing** — Current approach. Works fine with large timing margin. Revisit if timing becomes tight at higher sys_clk frequencies.
2. **Investigate nextpnr DQCE insertion** — The 24 auto-inserted DQCEs when any explicit DQCE is present may be a nextpnr-himbaechel bug or configuration issue.
3. **Use DHCEN instead** — The chip has 24 DHCEN (Dynamic HCLK Clock Enable) primitives that might serve as an alternative clock buffer path without triggering DQCE overflow.
4. **Upgrade nextpnr** — The DQCE insertion behavior may be version-dependent.
