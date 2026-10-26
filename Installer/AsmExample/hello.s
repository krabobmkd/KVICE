;
; hello.s - C64 "hello world" for the WLA-DX assembler
; from aminet: dev/cross/wla_dx_020_7.1.lzh
;
; Assemble and link (old WLA-DX, wla-6510, as in the Amiga packages):
;     wla-6510 -o hello.s hello.o
;     wlalink -bs linkerscript.l hello.prg
; (s: also hello.sym, the labels: autostarting hello.prg in KVICE loads
; them in the monitor)
; (WLA-DX 10 and later: wla-6502 -o hello.o hello.s)
;
; Then in KVICE x64: Autostart hello.prg, or LOAD "HELLO.PRG",8 and RUN.
;

.INCLUDE "c64.i"

; the C64 memory after the zero page and the stack, $0200-$FFFF, as one
; slot and one bank: the program is loaded in RAM, the "ROM" is just where
; WLA puts the bytes. Sizes up to $FFFF: old WLA versions read 16 bit
; numbers only.
.MEMORYMAP
    DEFAULTSLOT 0
    SLOTSIZE $FE00
    SLOT 0 $0200
.ENDME

.ROMBANKSIZE $FE00
.ROMBANKS 1

.BANK 0 SLOT 0

; .ORG counts from the start of the slot ($0200): .ORG $05FF is at $07FF.
; (newer WLA versions also have .ORGA, absolute: .ORGA $07FF)

; a .prg starts with its load address: written here, 2 bytes before
; $0801, so the linker output (-b: from the first byte used) is the .prg
.ORG $05FF                  ; $07FF
    .DW $0801

; at $0801: the BASIC line "10 SYS 2061", so RUN starts the program
.ORG $0601                  ; $0801
    C64_BASIC_STUB

; at $080D (2061)
start:
    lda #COLOR_BLUE
    sta VIC_BORDER
    lda #COLOR_BLACK
    sta VIC_BG_COLOR0

    ; print the text with the KERNAL, up to the 0
    ldx #0
print:
    ; lda message,x with a 16 bit address. WLA takes a label as a zero
    ; page (8 bit) address: newer versions accept "lda message.w,x", old
    ; ones have no size hint, so the instruction is written as data,
    ; which every version assembles
    .DB $BD                 ; LDA absolute,X
    .DW message
    beq done
    jsr CHROUT
    inx
    bne print
done:
    rts                     ; back to BASIC

; PETSCII: upper case letters are the same codes as in ASCII,
; 147 clears the screen, 13 is a new line
message:
    .DB 147, "HELLO FROM WLA-DX!", 13, 0
