# Generated from a native autotools headless build of VICE 3.10: the objects
# linked into each program ('ar t' of every linked library), mapped back to
# their sources. arch/headless replaced by arch/amiga, C++ linenoise dropped.
# VICE_SRC must point to vice-3.10/src.
# Sources only used by the Plus/4, C16, C116 (xplus4) emulator, on top of VICE_COMMON_SRC.
set(VICE_XPLUS4_SRC
	# drive/iec/plus4exp
	${VICE_SRC}/drive/iec/plus4exp/iec-plus4exp.c
	${VICE_SRC}/drive/iec/plus4exp/plus4exp-cmdline-options.c
	${VICE_SRC}/drive/iec/plus4exp/plus4exp-resources.c
	# iecbus
	${VICE_SRC}/iecbus/iecbus.c
	# plus4/cart
	${VICE_SRC}/plus4/cart/debugcart.c
	${VICE_SRC}/plus4/cart/digiblaster.c
	${VICE_SRC}/plus4/cart/jacint1mb.c
	${VICE_SRC}/plus4/cart/magiccart.c
	${VICE_SRC}/plus4/cart/multicart.c
	${VICE_SRC}/plus4/cart/plus4-generic.c
	${VICE_SRC}/plus4/cart/plus4-sidcart.c
	${VICE_SRC}/plus4/cart/plus4cart.c
	${VICE_SRC}/plus4/cart/speedy.c
	# plus4
	${VICE_SRC}/plus4/plus4-cmdline-options.c
	${VICE_SRC}/plus4/plus4-resources.c
	${VICE_SRC}/plus4/plus4-snapshot.c
	${VICE_SRC}/plus4/plus4-stubs.c
	${VICE_SRC}/plus4/plus4.c
	${VICE_SRC}/plus4/plus4acia.c
	${VICE_SRC}/plus4/plus4bus.c
	${VICE_SRC}/plus4/plus4cpu.c
	${VICE_SRC}/plus4/plus4datasette.c
	${VICE_SRC}/plus4/plus4drive.c
	${VICE_SRC}/plus4/plus4export.c
	${VICE_SRC}/plus4/plus4iec.c
	${VICE_SRC}/plus4/plus4io.c
	${VICE_SRC}/plus4/plus4mem.c
	${VICE_SRC}/plus4/plus4memcsory256k.c
	${VICE_SRC}/plus4/plus4memhacks.c
	${VICE_SRC}/plus4/plus4memhannes256k.c
	${VICE_SRC}/plus4/plus4memlimit.c
	${VICE_SRC}/plus4/plus4memrom.c
	${VICE_SRC}/plus4/plus4memsnapshot.c
	${VICE_SRC}/plus4/plus4model.c
	${VICE_SRC}/plus4/plus4parallel.c
	${VICE_SRC}/plus4/plus4pio1.c
	${VICE_SRC}/plus4/plus4pio2.c
	${VICE_SRC}/plus4/plus4printer.c
	${VICE_SRC}/plus4/plus4rom.c
	${VICE_SRC}/plus4/plus4romset.c
	${VICE_SRC}/plus4/plus4speech.c
	${VICE_SRC}/plus4/plus4tcbm.c
	${VICE_SRC}/plus4/plus4video.c
	${VICE_SRC}/plus4/ted-badline.c
	${VICE_SRC}/plus4/ted-cmdline-options.c
	${VICE_SRC}/plus4/ted-color.c
	${VICE_SRC}/plus4/ted-draw.c
	${VICE_SRC}/plus4/ted-fetch.c
	${VICE_SRC}/plus4/ted-irq.c
	${VICE_SRC}/plus4/ted-mem.c
	${VICE_SRC}/plus4/ted-resources.c
	${VICE_SRC}/plus4/ted-snapshot.c
	${VICE_SRC}/plus4/ted-sound.c
	${VICE_SRC}/plus4/ted-timer.c
	${VICE_SRC}/plus4/ted-timing.c
	${VICE_SRC}/plus4/ted.c
	# rs232drv
	${VICE_SRC}/rs232drv/rsuser.c
)
