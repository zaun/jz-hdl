; Fibonacci generator
; writes first 10 Fibonacci numbers to the leds
; RAM layout
; 0x10 prev
; 0x11 curr
; 0x12 tmp
; 0x13 count
; 0x14 one

; @MEMSIZE(256)

init:
    LDI 0x00
    STA 0x10
    LDI 0x01
    STA 0x11
    LDI 0x00
    STA 0x12
    LDI 10
    STA 0x13
    LDI 0x01
    STA 0x14

loop_start:
    LDA 0x11
    STOU
    WAI 0xFA      ; Wait 250ms

    LDA 0x11
    STA 0x12
    LDA 0x10
    ADD 0x12
    STA 0x11
    LDA 0x12
    STA 0x10

    LDA 0x13
    SUB 0x14
    STA 0x13
    LDA 0x13
    JZ  end
    JMP loop_start

end:
    JMP init