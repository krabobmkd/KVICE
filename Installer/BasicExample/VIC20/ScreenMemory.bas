Screen memory: a line of colored balls written with POKE.
VIC-20 BASIC V2, example 3 of BASICv2Quick.guide on the VIC: the screen
and color RAM addresses are read, so it runs with any RAM expansion.

10 PRINT CHR$(147)
20 S=PEEK(648)*256 : C=37888+4*(PEEK(36866) AND 128)
30 FOR X=0 TO 21
40 POKE S+11*22+X,81
50 POKE C+11*22+X,X AND 7
60 NEXT X
70 GET K$ : IF K$="" THEN 70
