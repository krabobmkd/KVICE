;
; plus4.i - Commodore Plus/4, C16, C116 hardware definitions for WLA-DX
; (wla-6502: the 7501 / 8501 CPU runs the 6502 instructions)
;
; TED registers, I/O chips, KERNAL jump table, vectors, colors, and a BASIC
; "10 SYS 4109" line so a .prg starts with RUN.
;
; Usage:
;     .INCLUDE "plus4.i"
;     ...
;     .ORG $0E01                  ; $1001 with a slot from $0200 (hello.s)
;     PLUS4_BASIC_STUB        ; the program goes on at $100D
;
; A .prg starts with its 2 bytes load address: write it 2 bytes before,
; with .ORG $0DFF ($0FFF) and .DW $1001, and link with -b (program
; file: from the first byte used). See hello.s, assembled with:
;     wla-6510 -o hello.s hello.o           (old WLA-DX, Amiga packages)
;     wla-6502 -o hello.o hello.s           (WLA-DX 10 and later)
;     wlalink -b linkerscript.l hello.prg
;

; ---------------------------------------------------------------------------
; 7501 / 8501 processor port

.DEFINE CPU_DDR             $00     ; data direction of $01
.DEFINE CPU_PORT            $01     ; serial bus, cassette

; ---------------------------------------------------------------------------
; memory

.DEFINE COLOR_RAM           $0800   ; attributes: color and luminance,
                                    ; 7: flash (40 x 25)
.DEFINE SCREEN_RAM          $0C00   ; characters (40 x 25)
.DEFINE BASIC_START         $1001
.DEFINE BASIC_ROM           $8000
.DEFINE CHAR_ROM            $D000
.DEFINE KERNAL_ROM          $C000   ; with $E000-$FCFF

; ---------------------------------------------------------------------------
; I/O chips

.DEFINE ACIA_DATA           $FD00   ; 6551 serial port (Plus/4 only)
.DEFINE ACIA_STATUS         $FD01
.DEFINE ACIA_COMMAND        $FD02
.DEFINE ACIA_CONTROL        $FD03
.DEFINE USER_PORT           $FD10   ; 6529 (Plus/4 only)
.DEFINE KEYBOARD_LATCH      $FD30   ; 6529: keyboard columns, then read
                                    ; TED_KEYBOARD
.DEFINE ROM_BANK            $FDD0   ; write $FDD0 + bank: ROM banks

; ---------------------------------------------------------------------------
; TED video, sound and timer chip (7360 / 8360), $FF00

.DEFINE TED                 $FF00
.DEFINE TED_T1_LO           $FF00   ; 3 timers counting down, IRQ at 0
.DEFINE TED_T1_HI           $FF01
.DEFINE TED_T2_LO           $FF02
.DEFINE TED_T2_HI           $FF03
.DEFINE TED_T3_LO           $FF04
.DEFINE TED_T3_HI           $FF05
.DEFINE TED_CTRL1           $FF06   ; 6: ECM, 5: BMM, 4: DEN, 3: RSEL (25 rows),
                                    ; 0-2: Y scroll
.DEFINE TED_CTRL2           $FF07   ; 7: no reverse characters (256), 6: NTSC,
                                    ; 5: freeze, 4: MCM, 3: CSEL (40 columns),
                                    ; 0-2: X scroll
.DEFINE TED_KEYBOARD        $FF08   ; write: latch the joysticks / KEYBOARD_LATCH
                                    ; columns, read: keyboard rows
.DEFINE TED_IRQ             $FF09   ; IRQ flags, write 1 to acknowledge
.DEFINE TED_IRQ_ENABLE      $FF0A   ; 6: timer 3, 4: timer 2, 3: timer 1,
                                    ; 1: raster, 0: raster compare bit 8
.DEFINE TED_RASTER_CMP      $FF0B   ; raster compare bits 0-7
.DEFINE TED_CURSOR_HI       $FF0C   ; hardware cursor position
.DEFINE TED_CURSOR_LO       $FF0D
.DEFINE TED_V1_FREQ_LO      $FF0E   ; voice 1 frequency bits 0-7
.DEFINE TED_V2_FREQ_LO      $FF0F   ; voice 2 frequency bits 0-7
.DEFINE TED_V2_FREQ_HI      $FF10   ; voice 2 frequency bits 8-9
.DEFINE TED_SOUND           $FF11   ; 7: D/A mode, 6: voice 2 noise,
                                    ; 5: voice 2 on, 4: voice 1 on, 0-3: volume
.DEFINE TED_BITMAP          $FF12   ; 3-5: bitmap / 8K, 2: character ROM (1)
                                    ; or RAM (0), 0-1: voice 1 frequency bits 8-9
.DEFINE TED_CHARSET         $FF13   ; 2-7: character set / 1K, 1: single clock,
                                    ; 0: ROM state (read)
.DEFINE TED_SCREEN          $FF14   ; 3-7: screen and attributes / 2K
.DEFINE TED_BG_COLOR0       $FF15
.DEFINE TED_BG_COLOR1       $FF16
.DEFINE TED_BG_COLOR2       $FF17
.DEFINE TED_BG_COLOR3       $FF18
.DEFINE TED_BORDER          $FF19
.DEFINE TED_CHAR_POS_HI     $FF1A   ; character position (reload)
.DEFINE TED_CHAR_POS_LO     $FF1B
.DEFINE TED_RASTER_HI       $FF1C   ; current raster line bit 8
.DEFINE TED_RASTER          $FF1D   ; current raster line bits 0-7
.DEFINE TED_RASTER_X        $FF1E   ; horizontal position
.DEFINE TED_FLASH           $FF1F   ; 3-6: flash counter, 0-2: vertical sub-address
.DEFINE TED_ROM_ON          $FF3E   ; write anything: ROMs in $8000-$FFFF
.DEFINE TED_RAM_ON          $FF3F   ; write anything: RAM in $8000-$FFFF

; ---------------------------------------------------------------------------
; vectors

.DEFINE IRQ_VECTOR          $0314   ; RAM vectors used by the KERNAL
.DEFINE BRK_VECTOR          $0316
.DEFINE HW_NMI              $FFFA   ; hardware vectors (KERNAL ROM, or RAM
.DEFINE HW_RESET            $FFFC   ; after TED_RAM_ON)
.DEFINE HW_IRQ              $FFFE

; ---------------------------------------------------------------------------
; KERNAL jump table (same on the C64 and the VIC-20)

.DEFINE CINT                $FF81   ; init screen editor and TED
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
; colors: a TED color is LUMINANCE * 16 + COLOR, luminance 0 (dark) to 7

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
.DEFINE COLOR_YELLOW_GREEN  10
.DEFINE COLOR_PINK          11
.DEFINE COLOR_BLUE_GREEN    12
.DEFINE COLOR_LIGHT_BLUE    13
.DEFINE COLOR_DARK_BLUE     14
.DEFINE COLOR_LIGHT_GREEN   15
.DEFINE LUMINANCE           16      ; e.g. 5 * LUMINANCE + COLOR_RED

; ---------------------------------------------------------------------------
; "10 SYS 4109" at $1001: put it at $1001 (.ORG $1001 - slot start), the code follows at $100D

.MACRO PLUS4_BASIC_STUB
    .DW $100B               ; next BASIC line
    .DW 10                  ; line number
    .DB $9E, "4109", 0      ; SYS 4109
    .DW 0                   ; end of the program
.ENDM
