# Generated from a native autotools headless build of VICE 3.10: the objects
# linked into each program ('ar t' of every linked library), mapped back to
# their sources. arch/headless replaced by arch/amiga, C++ linenoise dropped.
# VICE_SRC must point to vice-3.10/src.
# Sources only used by the VIC-20 (xvic) emulator, on top of VICE_COMMON_SRC.
set(VICE_XVIC_SRC
	# arch/amiga: the machine UI
	${VICE_SRC}/arch/amiga/vic20ui.c
	# c64/cart
	${VICE_SRC}/c64/cart/c64acia1.c
	${VICE_SRC}/c64/cart/cs8900io.c
	${VICE_SRC}/c64/cart/digimax.c
	${VICE_SRC}/c64/cart/ds12c887rtc.c
	${VICE_SRC}/c64/cart/ethernetcart.c
	${VICE_SRC}/c64/cart/georam.c
	${VICE_SRC}/c64/cart/sfx_soundexpander.c
	${VICE_SRC}/c64/cart/sfx_soundsampler.c
	# iecbus
	${VICE_SRC}/iecbus/iecbus.c
	# (src)
	${VICE_SRC}/midi.c
	# rs232drv
	${VICE_SRC}/rs232drv/rsuser.c
	# vic20/cart
	${VICE_SRC}/vic20/cart/behrbonz.c
	${VICE_SRC}/vic20/cart/debugcart.c
	${VICE_SRC}/vic20/cart/finalexpansion.c
	${VICE_SRC}/vic20/cart/ioramcart.c
	${VICE_SRC}/vic20/cart/mascuerade-stubs.c
	${VICE_SRC}/vic20/cart/megacart.c
	${VICE_SRC}/vic20/cart/mikroassembler.c
	${VICE_SRC}/vic20/cart/minimon.c
	${VICE_SRC}/vic20/cart/rabbit.c
	${VICE_SRC}/vic20/cart/superexpander.c
	${VICE_SRC}/vic20/cart/ultimem.c
	${VICE_SRC}/vic20/cart/vic-fp.c
	${VICE_SRC}/vic20/cart/vic20-generic.c
	${VICE_SRC}/vic20/cart/vic20-ieee488.c
	${VICE_SRC}/vic20/cart/vic20-midi.c
	${VICE_SRC}/vic20/cart/vic20-sidcart.c
	${VICE_SRC}/vic20/cart/vic20cart.c
	${VICE_SRC}/vic20/cart/vic20cartmem.c
	${VICE_SRC}/vic20/cart/writenow.c
	# vic20
	${VICE_SRC}/vic20/vic-cmdline-options.c
	${VICE_SRC}/vic20/vic-color.c
	${VICE_SRC}/vic20/vic-cycle.c
	${VICE_SRC}/vic20/vic-draw.c
	${VICE_SRC}/vic20/vic-mem.c
	${VICE_SRC}/vic20/vic-resources.c
	${VICE_SRC}/vic20/vic-snapshot.c
	${VICE_SRC}/vic20/vic-timing.c
	${VICE_SRC}/vic20/vic.c
	${VICE_SRC}/vic20/vic20-cmdline-options.c
	${VICE_SRC}/vic20/vic20-resources.c
	${VICE_SRC}/vic20/vic20-snapshot.c
	${VICE_SRC}/vic20/vic20-stubs.c
	${VICE_SRC}/vic20/vic20.c
	${VICE_SRC}/vic20/vic20bus.c
	${VICE_SRC}/vic20/vic20cpu.c
	${VICE_SRC}/vic20/vic20datasette.c
	${VICE_SRC}/vic20/vic20drive.c
	${VICE_SRC}/vic20/vic20export.c
	${VICE_SRC}/vic20/vic20iec.c
	${VICE_SRC}/vic20/vic20ieeevia1.c
	${VICE_SRC}/vic20/vic20ieeevia2.c
	${VICE_SRC}/vic20/vic20io.c
	${VICE_SRC}/vic20/vic20mem.c
	${VICE_SRC}/vic20/vic20memrom.c
	${VICE_SRC}/vic20/vic20memsnapshot.c
	${VICE_SRC}/vic20/vic20model.c
	${VICE_SRC}/vic20/vic20printer.c
	${VICE_SRC}/vic20/vic20rom.c
	${VICE_SRC}/vic20/vic20romset.c
	${VICE_SRC}/vic20/vic20rsuser.c
	${VICE_SRC}/vic20/vic20sound.c
	${VICE_SRC}/vic20/vic20via1.c
	${VICE_SRC}/vic20/vic20via2.c
	${VICE_SRC}/vic20/vic20video.c
)
