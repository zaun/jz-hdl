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
    LDI 0x01      ; Store 1 at address 0x01 for increment
    STA 0x01      
    LDI 0x00      ; Start counter at 0

loop:
    STOU          ; Display counter on LEDs
    WAI 0xFA      ; Wait 250ms
    ADD 0x01      ; Increment by 1
    JMP loop      ; Repeat
