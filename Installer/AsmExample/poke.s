;
; poke.s - C64: a few characters written straight into the screen and
; color RAM with LDA / STA, no KERNAL call
;
; Assemble and link (old WLA-DX, wla-6510, as in the Amiga packages):
;     wla-6510 -o poke.s poke.o
;     wlalink -bs poke.l poke.prg
; (s: also poke.sym, the labels: autostarting poke.prg in KVICE loads
; them in the monitor)
; (WLA-DX 10 and later: wla-6502 -o poke.o poke.s)
;
; Then in KVICE x64: Autostart poke.prg, or LOAD "POKE.PRG",8 and RUN.
;

.INCLUDE "c64.i"

; the C64 memory after the zero page and the stack, $0200-$FFFF, as one
; slot and one bank (see hello.s)
.MEMORYMAP
    DEFAULTSLOT 0
    SLOTSIZE $FE00
    SLOT 0 $0200
.ENDME

.ROMBANKSIZE $FE00
.ROMBANKS 1

.BANK 0 SLOT 0

; .ORG counts from the start of the slot ($0200)
.ORG $05FF                  ; $07FF: the .prg load address
    .DW $0801

.ORG $0601                  ; $0801: "10 SYS 2061"
    C64_BASIC_STUB

; where the text goes: row 12, column 18 of the 40 x 25 screen,
; 12 * 40 + 18 = 498 = $1F2 bytes after the start. The color RAM has the
; same layout as the screen.
.DEFINE TEXT_SCREEN         $05F2   ; SCREEN_RAM ($0400) + $1F2
.DEFINE TEXT_COLOR          $D9F2   ; COLOR_RAM ($D800) + $1F2

; at $080D (2061)
start:
    lda #COLOR_BLACK
    sta VIC_BORDER
    sta VIC_BG_COLOR0

    ; the screen holds screen codes, not PETSCII / ASCII:
    ; A-Z are 1-26, space 32, "!" 33, digits 48-57
    lda #8                  ; H
    sta TEXT_SCREEN
    lda #COLOR_YELLOW
    sta TEXT_COLOR

    lda #9                  ; I
    sta TEXT_SCREEN + 1
    lda #COLOR_LIGHT_GREEN
    sta TEXT_COLOR + 1

    lda #33                 ; !
    sta TEXT_SCREEN + 2
    lda #COLOR_LIGHT_RED
    sta TEXT_COLOR + 2

    rts                     ; back to BASIC: READY.
