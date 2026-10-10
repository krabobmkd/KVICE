Rainbow border: the 16 background colors, with the 8 border colors.
VIC-20 BASIC V2, the C64 example 1 of BASICv2Quick.guide on the VIC.

10 FOR C=0 TO 15
20 POKE 36879,C*16+8+(C AND 7)
30 FOR D=1 TO 300 : NEXT D
40 NEXT C
50 POKE 36879,27
60 PRINT "BACK TO NORMAL"
