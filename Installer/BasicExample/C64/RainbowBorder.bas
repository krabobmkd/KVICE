Rainbow border: the 16 colors in the border and background.
C64 BASIC V2, from BASICv2Quick.guide, example 1.

10 FOR C=0 TO 15
20 POKE 53280,C : POKE 53281,C
30 FOR D=1 TO 300 : NEXT D
40 NEXT C
50 POKE 53280,14 : POKE 53281,6
60 PRINT "BACK TO THE USUAL COLORS"
