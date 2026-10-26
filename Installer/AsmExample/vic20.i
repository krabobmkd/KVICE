;
; vic20.i - Commodore VIC-20 hardware definitions for WLA-DX (wla-6502)
;
; Chip registers, KERNAL jump table, vectors, colors, and BASIC "10 SYS"
; lines so a .prg starts with RUN. Where BASIC, the screen and the color
; RAM are depends on the memory expansion: see "memory" and the stubs.
;
; Usage (unexpanded VIC-20):
;     .INCLUDE "vic20.i"
;     ...
;     .ORG $0E01                  ; $1001 with a slot from $0200 (hello.s)
;     VIC20_BASIC_STUB        ; the program goes on at $100D
;
; A .prg starts with its 2 bytes load address: write it 2 bytes before,
; with .ORG $0DFF ($0FFF) and .DW $1001, and link with -b (program
; file: from the first byte used). See hello.s, assembled with:
;     wla-6510 -o hello.s hello.o           (old WLA-DX, Amiga packages)
;     wla-6502 -o hello.o hello.s           (WLA-DX 10 and later)
;     wlalink -b linkerscript.l hello.prg
;

; ---------------------------------------------------------------------------
; memory

; unexpanded or +3K
.DEFINE SCREEN_RAM          $1E00   ; 22 x 23
.DEFINE COLOR_RAM           $9600
; +8K or more: the screen moves down
.DEFINE SCREEN_RAM_EXP      $1000
.DEFINE COLOR_RAM_EXP       $9400

.DEFINE BASIC_START         $1001   ; unexpanded
.DEFINE BASIC_START_3K      $0401   ; +3K at $0400
.DEFINE BASIC_START_8K      $1201   ; +8K or more at $2000
.DEFINE CHAR_ROM            $8000
.DEFINE BASIC_ROM           $C000
.DEFINE KERNAL_ROM          $E000

; ---------------------------------------------------------------------------
; VIC video and sound chip (6560 NTSC / 6561 PAL), $9000

.DEFINE VIC                 $9000
.DEFINE VIC_ORIGIN_X        $9000   ; 7: interlace, 0-6: screen left position
.DEFINE VIC_ORIGIN_Y        $9001   ; screen top position
.DEFINE VIC_COLUMNS         $9002   ; 0-6: columns, 7: screen address bit 9
.DEFINE VIC_ROWS            $9003   ; 1-6: rows, 0: 8x16 characters,
                                    ; 7: raster bit 0
.DEFINE VIC_RASTER          $9004   ; raster bits 8-1
.DEFINE VIC_MEMORY          $9005   ; 4-7: screen, 0-3: character memory
.DEFINE VIC_LP_X            $9006   ; light pen
.DEFINE VIC_LP_Y            $9007
.DEFINE VIC_PADDLE_X        $9008
.DEFINE VIC_PADDLE_Y        $9009
.DEFINE VIC_BASS            $900A   ; 7: on, 0-6: frequency
.DEFINE VIC_ALTO            $900B
.DEFINE VIC_SOPRANO         $900C
.DEFINE VIC_NOISE           $900D
.DEFINE VIC_VOLUME          $900E   ; 0-3: volume, 4-7: auxiliary color
.DEFINE VIC_COLORS          $900F   ; 4-7: background, 3: 1 = normal (0 =
                                    ; inverted), 0-2: border

; ---------------------------------------------------------------------------
; VIA 1 (6522), $9110: NMI (RESTORE key), user port, joystick bits 2-5

.DEFINE VIA1                $9110
.DEFINE VIA1_PRB            $9110   ; user port
.DEFINE VIA1_PRA            $9111   ; 2-4: joystick up, down, left, 5: fire
.DEFINE VIA1_DDRB           $9112
.DEFINE VIA1_DDRA           $9113
.DEFINE VIA1_T1_LO          $9114
.DEFINE VIA1_T1_HI          $9115
.DEFINE VIA1_T1_LATCH_LO    $9116
.DEFINE VIA1_T1_LATCH_HI    $9117
.DEFINE VIA1_T2_LO          $9118
.DEFINE VIA1_T2_HI          $9119
.DEFINE VIA1_SR             $911A
.DEFINE VIA1_ACR            $911B
.DEFINE VIA1_PCR            $911C
.DEFINE VIA1_IFR            $911D
.DEFINE VIA1_IER            $911E
.DEFINE VIA1_PRA_NH         $911F   ; port A without handshake

; ---------------------------------------------------------------------------
; VIA 2 (6522), $9120: IRQ (timer 1, 60 Hz), keyboard, joystick right

.DEFINE VIA2                $9120
.DEFINE VIA2_PRB            $9120   ; keyboard columns (write), 7: joystick
                                    ; right (with DDRB bit 7 = 0)
.DEFINE VIA2_PRA            $9121   ; keyboard rows (read)
.DEFINE VIA2_DDRB           $9122
.DEFINE VIA2_DDRA           $9123
.DEFINE VIA2_T1_LO          $9124
.DEFINE VIA2_T1_HI          $9125
.DEFINE VIA2_T1_LATCH_LO    $9126
.DEFINE VIA2_T1_LATCH_HI    $9127
.DEFINE VIA2_T2_LO          $9128
.DEFINE VIA2_T2_HI          $9129
.DEFINE VIA2_SR             $912A
.DEFINE VIA2_ACR            $912B
.DEFINE VIA2_PCR            $912C
.DEFINE VIA2_IFR            $912D
.DEFINE VIA2_IER            $912E
.DEFINE VIA2_PRA_NH         $912F

; ---------------------------------------------------------------------------
; vectors

.DEFINE IRQ_VECTOR          $0314   ; RAM vectors used by the KERNAL
.DEFINE BRK_VECTOR          $0316
.DEFINE NMI_VECTOR          $0318
.DEFINE HW_NMI              $FFFA   ; hardware vectors (KERNAL ROM)
.DEFINE HW_RESET            $FFFC
.DEFINE HW_IRQ              $FFFE

; ---------------------------------------------------------------------------
; KERNAL jump table (same on the C64 and the Plus/4)

.DEFINE CINT                $FF81   ; init screen editor and VIC
.DEFINE IOINIT              $FF84
.DEFINE RAMTAS              $FF87
.DEFINE RESTOR              $FF8A   ; default RAM vectors
.DEFINE VECTOR              $FF8D
.DEFINE SETMSG              $FF90
.DEFINE SECOND              $FF93
.DEFINE TKSA                $FF96
.DEFINE MEMTOP              $FF99
.DEFINE MEMBOT              $FF9C
.DEFINE SCNKEY              $FF9F
.DEFINE SETTMO              $FFA2
.DEFINE ACPTR               $FFA5
.DEFINE CIOUT               $FFA8
.DEFINE UNTLK               $FFAB
.DEFINE UNLSN               $FFAE
.DEFINE LISTEN              $FFB1
.DEFINE TALK                $FFB4
.DEFINE READST              $FFB7
.DEFINE SETLFS              $FFBA
.DEFINE SETNAM              $FFBD
.DEFINE OPEN                $FFC0
.DEFINE CLOSE               $FFC3
.DEFINE CHKIN               $FFC6
.DEFINE CHKOUT              $FFC9
.DEFINE CLRCHN              $FFCC
.DEFINE CHRIN               $FFCF
.DEFINE CHROUT              $FFD2   ; print the PETSCII character in A
.DEFINE LOAD                $FFD5
.DEFINE SAVE                $FFD8
.DEFINE SETTIM              $FFDB
.DEFINE RDTIM               $FFDE
.DEFINE STOP                $FFE1
.DEFINE GETIN               $FFE4   ; A: key from the buffer, 0 if none
.DEFINE CLALL               $FFE7
.DEFINE UDTIM               $FFEA
.DEFINE SCREEN              $FFED
.DEFINE PLOT                $FFF0   ; cursor position: C=1 read, C=0 set (X row, Y column)
.DEFINE IOBASE              $FFF3

; ---------------------------------------------------------------------------
; colors: characters (color RAM) and border use 0-7 only, the background
; and the auxiliary color 0-15

.DEFINE COLOR_BLACK         0
.DEFINE COLOR_WHITE         1
.DEFINE COLOR_RED           2
.DEFINE COLOR_CYAN          3
.DEFINE COLOR_PURPLE        4
.DEFINE COLOR_GREEN         5
.DEFINE COLOR_BLUE          6
.DEFINE COLOR_YELLOW        7
.DEFINE COLOR_ORANGE        8
.DEFINE COLOR_LIGHT_ORANGE  9
.DEFINE COLOR_PINK          10
.DEFINE COLOR_LIGHT_CYAN    11
.DEFINE COLOR_LIGHT_PURPLE  12
.DEFINE COLOR_LIGHT_GREEN   13
.DEFINE COLOR_LIGHT_BLUE    14
.DEFINE COLOR_LIGHT_YELLOW  15

; ---------------------------------------------------------------------------
; "10 SYS ..." BASIC lines: put one at the BASIC start of the expansion,
; the code follows 12 bytes after it

; unexpanded: at $1001, code at $100D
.MACRO VIC20_BASIC_STUB
    .DW $100B               ; next BASIC line
    .DW 10                  ; line number
    .DB $9E, "4109", 0      ; SYS 4109
    .DW 0                   ; end of the program
.ENDM

; +3K: at $0401, code at $040D
.MACRO VIC20_BASIC_STUB_3K
    .DW $040B
    .DW 10
    .DB $9E, "1037", 0      ; SYS 1037
    .DW 0
.ENDM

; +8K or more: at $1201, code at $120D
.MACRO VIC20_BASIC_STUB_8K
    .DW $120B
    .DW 10
    .DB $9E, "4621", 0      ; SYS 4621
    .DW 0
.ENDM
