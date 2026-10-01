# Generated from a native autotools headless build of VICE 3.10: the objects
# linked into each program ('ar t' of every linked library), mapped back to
# their sources. arch/headless replaced by arch/amiga, C++ linenoise dropped.
# VICE_SRC must point to vice-3.10/src.
# Sources only used by the PET, CBM (xpet) emulator, on top of VICE_COMMON_SRC.
set(VICE_XPET_SRC
	# crtc
	${VICE_SRC}/crtc/crtc-cmdline-options.c
	${VICE_SRC}/crtc/crtc-color.c
	${VICE_SRC}/crtc/crtc-draw.c
	${VICE_SRC}/crtc/crtc-mem.c
	${VICE_SRC}/crtc/crtc-resources.c
	${VICE_SRC}/crtc/crtc-snapshot.c
	${VICE_SRC}/crtc/crtc.c
	# pet
	${VICE_SRC}/pet/6809.c
	${VICE_SRC}/pet/debugcart.c
	${VICE_SRC}/pet/pet-cmdline-options.c
	${VICE_SRC}/pet/pet-resources.c
	${VICE_SRC}/pet/pet-sidcart.c
	${VICE_SRC}/pet/pet-snapshot.c
	${VICE_SRC}/pet/pet-stubs.c
	${VICE_SRC}/pet/pet.c
	${VICE_SRC}/pet/petacia1.c
	${VICE_SRC}/pet/petbus.c
	${VICE_SRC}/pet/petcolour.c
	${VICE_SRC}/pet/petcpu.c
	${VICE_SRC}/pet/petdatasette.c
	${VICE_SRC}/pet/petdrive.c
	${VICE_SRC}/pet/petdww.c
	${VICE_SRC}/pet/pethre.c
	${VICE_SRC}/pet/petiec.c
	${VICE_SRC}/pet/petio.c
	${VICE_SRC}/pet/petmem.c
	${VICE_SRC}/pet/petmemsnapshot.c
	${VICE_SRC}/pet/petmodel.c
	${VICE_SRC}/pet/petpia1.c
	${VICE_SRC}/pet/petpia2.c
	${VICE_SRC}/pet/petprinter.c
	${VICE_SRC}/pet/petreu.c
	${VICE_SRC}/pet/petrom.c
	${VICE_SRC}/pet/petromset.c
	${VICE_SRC}/pet/petsound.c
	${VICE_SRC}/pet/petvia.c
	${VICE_SRC}/pet/petvideo.c
)
