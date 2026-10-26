;
; c64.i - Commodore 64 hardware definitions for WLA-DX (wla-6502 / wla-6510)
;
; Chip registers, KERNAL jump table, vectors, colors, and a BASIC
; "10 SYS 2061" line so a .prg starts with RUN.
;
; Usage:
;     .INCLUDE "c64.i"
;     ...
;     .ORG $0601                  ; $0801 with a slot from $0200 (hello.s)
;     C64_BASIC_STUB          ; the program goes on at $080D
;
; A .prg starts with its 2 bytes load address: write it 2 bytes before,
; with .ORG $05FF ($07FF) and .DW $0801, and link with -b (program
; file: from the first byte used). See hello.s, assembled with:
;     wla-6510 -o hello.s hello.o           (old WLA-DX, Amiga packages)
;     wla-6502 -o hello.o hello.s           (WLA-DX 10 and later)
;     wlalink -b linkerscript.l hello.prg
;

; ---------------------------------------------------------------------------
; 6510 processor port

.DEFINE CPU_DDR             $00     ; data direction of $01
.DEFINE CPU_PORT            $01     ; bits 0-2: LORAM, HIRAM, CHAREN (memory
                                    ; configuration), 3-5: cassette

; ---------------------------------------------------------------------------
; memory

.DEFINE SCREEN_RAM          $0400   ; default screen (40 x 25)
.DEFINE SPRITE_POINTERS     $07F8   ; 8 sprite pointers of the default screen
.DEFINE BASIC_START         $0801
.DEFINE BASIC_ROM           $A000
.DEFINE CHAR_ROM            $D000   ; when CHAREN = 0 in CPU_PORT
.DEFINE COLOR_RAM           $D800   ; 1000 nybbles
.DEFINE KERNAL_ROM          $E000

; ---------------------------------------------------------------------------
; VIC-II video chip (6567 NTSC / 6569 PAL), $D000

.DEFINE VIC                 $D000
.DEFINE VIC_SPR0_X          $D000
.DEFINE VIC_SPR0_Y          $D001
.DEFINE VIC_SPR1_X          $D002
.DEFINE VIC_SPR1_Y          $D003
.DEFINE VIC_SPR2_X          $D004
.DEFINE VIC_SPR2_Y          $D005
.DEFINE VIC_SPR3_X          $D006
.DEFINE VIC_SPR3_Y          $D007
.DEFINE VIC_SPR4_X          $D008
.DEFINE VIC_SPR4_Y          $D009
.DEFINE VIC_SPR5_X          $D00A
.DEFINE VIC_SPR5_Y          $D00B
.DEFINE VIC_SPR6_X          $D00C
.DEFINE VIC_SPR6_Y          $D00D
.DEFINE VIC_SPR7_X          $D00E
.DEFINE VIC_SPR7_Y          $D00F
.DEFINE VIC_SPR_X_MSB       $D010   ; bit 8 of the 8 sprite X
.DEFINE VIC_CTRL1           $D011   ; 7: raster bit 8, 6: ECM, 5: BMM, 4: DEN,
                                    ; 3: RSEL (25 rows), 0-2: Y scroll
.DEFINE VIC_RASTER          $D012   ; read: raster line, write: IRQ line
.DEFINE VIC_LP_X            $D013   ; light pen
.DEFINE VIC_LP_Y            $D014
.DEFINE VIC_SPR_ENABLE      $D015
.DEFINE VIC_CTRL2           $D016   ; 4: MCM, 3: CSEL (40 columns), 0-2: X scroll
.DEFINE VIC_SPR_EXPAND_Y    $D017
.DEFINE VIC_MEMORY          $D018   ; 4-7: screen / 1K, 1-3: charset / 2K
.DEFINE VIC_IRQ             $D019   ; IRQ flags, write 1 to acknowledge
.DEFINE VIC_IRQ_ENABLE      $D01A   ; 0: raster, 1: sprite-bg, 2: sprite-sprite,
                                    ; 3: light pen
.DEFINE VIC_SPR_PRIORITY    $D01B   ; 1: sprite behind the background
.DEFINE VIC_SPR_MULTICOLOR  $D01C
.DEFINE VIC_SPR_EXPAND_X    $D01D
.DEFINE VIC_SPR_SPR_COLL    $D01E   ; sprite-sprite collisions (read clears)
.DEFINE VIC_SPR_BG_COLL     $D01F   ; sprite-background collisions (read clears)
.DEFINE VIC_BORDER          $D020
.DEFINE VIC_BG_COLOR0       $D021
.DEFINE VIC_BG_COLOR1       $D022
.DEFINE VIC_BG_COLOR2       $D023
.DEFINE VIC_BG_COLOR3       $D024
.DEFINE VIC_SPR_MCOLOR0     $D025
.DEFINE VIC_SPR_MCOLOR1     $D026
.DEFINE VIC_SPR0_COLOR      $D027
.DEFINE VIC_SPR1_COLOR      $D028
.DEFINE VIC_SPR2_COLOR      $D029
.DEFINE VIC_SPR3_COLOR      $D02A
.DEFINE VIC_SPR4_COLOR      $D02B
.DEFINE VIC_SPR5_COLOR      $D02C
.DEFINE VIC_SPR6_COLOR      $D02D
.DEFINE VIC_SPR7_COLOR      $D02E

; ---------------------------------------------------------------------------
; SID sound chip (6581 / 8580), $D400, write only but the 4 last ones

.DEFINE SID                 $D400
.DEFINE SID_V1_FREQ_LO      $D400
.DEFINE SID_V1_FREQ_HI      $D401
.DEFINE SID_V1_PW_LO        $D402   ; pulse width, 12 bits
.DEFINE SID_V1_PW_HI        $D403
.DEFINE SID_V1_CTRL         $D404   ; 7: noise, 6: pulse, 5: saw, 4: triangle,
                                    ; 3: test, 2: ring, 1: sync, 0: gate
.DEFINE SID_V1_AD           $D405   ; attack (4-7), decay (0-3)
.DEFINE SID_V1_SR           $D406   ; sustain (4-7), release (0-3)
.DEFINE SID_V2_FREQ_LO      $D407
.DEFINE SID_V2_FREQ_HI      $D408
.DEFINE SID_V2_PW_LO        $D409
.DEFINE SID_V2_PW_HI        $D40A
.DEFINE SID_V2_CTRL         $D40B
.DEFINE SID_V2_AD           $D40C
.DEFINE SID_V2_SR           $D40D
.DEFINE SID_V3_FREQ_LO      $D40E
.DEFINE SID_V3_FREQ_HI      $D40F
.DEFINE SID_V3_PW_LO        $D410
.DEFINE SID_V3_PW_HI        $D411
.DEFINE SID_V3_CTRL         $D412
.DEFINE SID_V3_AD           $D413
.DEFINE SID_V3_SR           $D414
.DEFINE SID_FC_LO           $D415   ; filter cutoff, bits 0-2
.DEFINE SID_FC_HI           $D416   ; filter cutoff, bits 3-10
.DEFINE SID_RES_FILT        $D417   ; resonance (4-7), filtered voices (0-3)
.DEFINE SID_MODE_VOL        $D418   ; 7: voice 3 off, 6: HP, 5: BP, 4: LP,
                                    ; 0-3: volume
.DEFINE SID_POT_X           $D419   ; read
.DEFINE SID_POT_Y           $D41A   ; read
.DEFINE SID_OSC3            $D41B   ; read: voice 3 waveform (random numbers
                                    ; with noise)
.DEFINE SID_ENV3            $D41C   ; read: voice 3 envelope

; ---------------------------------------------------------------------------
; CIA 1 (6526), $DC00: keyboard, joysticks, IRQ timers

.DEFINE CIA1                $DC00
.DEFINE CIA1_PRA            $DC00   ; keyboard columns (write), joystick 2
.DEFINE CIA1_PRB            $DC01   ; keyboard rows (read), joystick 1
.DEFINE CIA1_DDRA           $DC02
.DEFINE CIA1_DDRB           $DC03
.DEFINE CIA1_TA_LO          $DC04
.DEFINE CIA1_TA_HI          $DC05
.DEFINE CIA1_TB_LO          $DC06
.DEFINE CIA1_TB_HI          $DC07
.DEFINE CIA1_TOD_10THS      $DC08
.DEFINE CIA1_TOD_SEC        $DC09
.DEFINE CIA1_TOD_MIN        $DC0A
.DEFINE CIA1_TOD_HR         $DC0B
.DEFINE CIA1_SDR            $DC0C
.DEFINE CIA1_ICR            $DC0D   ; interrupt control, read clears
.DEFINE CIA1_CRA            $DC0E
.DEFINE CIA1_CRB            $DC0F

; ---------------------------------------------------------------------------
; CIA 2 (6526), $DD00: serial bus, VIC bank, user port, NMI timers

.DEFINE CIA2                $DD00
.DEFINE CIA2_PRA            $DD00   ; bits 0-1: VIC bank (inverted), 3-7: serial
.DEFINE CIA2_PRB            $DD01   ; user port
.DEFINE CIA2_DDRA           $DD02
.DEFINE CIA2_DDRB           $DD03
.DEFINE CIA2_TA_LO          $DD04
.DEFINE CIA2_TA_HI          $DD05
.DEFINE CIA2_TB_LO          $DD06
.DEFINE CIA2_TB_HI          $DD07
.DEFINE CIA2_TOD_10THS      $DD08
.DEFINE CIA2_TOD_SEC        $DD09
.DEFINE CIA2_TOD_MIN        $DD0A
.DEFINE CIA2_TOD_HR         $DD0B
.DEFINE CIA2_SDR            $DD0C
.DEFINE CIA2_ICR            $DD0D   ; NMI control, read clears
.DEFINE CIA2_CRA            $DD0E
.DEFINE CIA2_CRB            $DD0F

; ---------------------------------------------------------------------------
; vectors

.DEFINE IRQ_VECTOR          $0314   ; RAM vectors used by the KERNAL
.DEFINE BRK_VECTOR          $0316
.DEFINE NMI_VECTOR          $0318
.DEFINE HW_NMI              $FFFA   ; hardware vectors (KERNAL ROM, or RAM
.DEFINE HW_RESET            $FFFC   ; with the KERNAL banked out)
.DEFINE HW_IRQ              $FFFE

; end of an IRQ handler installed in IRQ_VECTOR
.DEFINE KERNAL_IRQ          $EA31   ; the KERNAL IRQ (keyboard, clock...)
.DEFINE KERNAL_IRQ_EXIT     $EA81   ; restore A, X, Y and RTI

; ---------------------------------------------------------------------------
; KERNAL jump table (same on the VIC-20 and the Plus/4)

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
; colors

.DEFINE COLOR_BLACK         0
.DEFINE COLOR_WHITE         1
.DEFINE COLOR_RED           2
.DEFINE COLOR_CYAN          3
.DEFINE COLOR_PURPLE        4
.DEFINE COLOR_GREEN         5
.DEFINE COLOR_BLUE          6
.DEFINE COLOR_YELLOW        7
.DEFINE COLOR_ORANGE        8
.DEFINE COLOR_BROWN         9
.DEFINE COLOR_LIGHT_RED     10
.DEFINE COLOR_DARK_GREY     11
.DEFINE COLOR_GREY          12
.DEFINE COLOR_LIGHT_GREEN   13
.DEFINE COLOR_LIGHT_BLUE    14
.DEFINE COLOR_LIGHT_GREY    15

; ---------------------------------------------------------------------------
; "10 SYS 2061" at $0801: put it at $0801 (.ORG $0801 - slot start), the code follows at $080D

.MACRO C64_BASIC_STUB
    .DW $080B               ; next BASIC line
    .DW 10                  ; line number
    .DB $9E, "2061", 0      ; SYS 2061
    .DW 0                   ; end of the program
.ENDM
