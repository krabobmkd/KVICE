Joystick and errors: a ball moved with the joystick in port 2, fire
changes the border; TRAP catches the errors (RUN/STOP too).
Plus/4 BASIC 3.5, from BASICv3Quick.guide, example 6.

10 TRAP 200 : SCNCLR : X=20 : Y=12
20 DO
30 CHAR 1,X,Y,"O"
40 DO : J=JOY(2) : LOOP WHILE J=0
50 CHAR 1,X,Y," "
60 IF J AND 128 THEN COLOR 4,INT(RND(1)*16)+1,5 : J=J AND 127
70 IF J=1 OR J=2 OR J=8 THEN Y=Y-1
80 IF J>=4 AND J<=6 THEN Y=Y+1
90 IF J>=2 AND J<=4 THEN X=X+1
100 IF J>=6 AND J<=8 THEN X=X-1
110 X=X-(X<0)+(X>39) : Y=Y-(Y<0)+(Y>24)
120 FOR D=1 TO 30 : NEXT D
130 LOOP
200 PRINT ERR$(ER);" IN";EL : END
