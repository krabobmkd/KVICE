All 121 colors: every color and luminance in the border.
Plus/4 BASIC 3.5, from BASICv3Quick.guide, example 1.

10 FOR L=0 TO 7
20 FOR C=1 TO 16
30 COLOR 4,C,L
40 FOR D=1 TO 100 : NEXT D
50 NEXT C
60 NEXT L
70 PRINT "COLOR";RCLR(4);"LUMINANCE";RLUM(4)
