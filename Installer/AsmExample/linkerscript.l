; use aminet package dev/cross/wla_dx_020_7.1.lzh
;
; the WLA-DX link file of hello.s:
;     wlalink -b linkerscript.l hello.prg
; -b: a program file, from the first byte used to the last one (the .prg
; load address is the first one, written by hello.s)

[objects]
hello.o
