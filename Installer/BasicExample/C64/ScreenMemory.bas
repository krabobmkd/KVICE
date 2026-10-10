Screen memory: a line of colored balls written with POKE.
C64 BASIC V2, from BASICv2Quick.guide, example 3.

10 PRINT CHR$(147)
20 FOR X=0 TO 39
30 POKE 1024+12*40+X,81
40 POKE 55296+12*40+X,X AND 15
50 NEXT X
60 GET K$ : IF K$="" THEN 60
