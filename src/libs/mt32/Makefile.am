AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES =
libmt32_a_SOURCES =

if C_MT32
noinst_LIBRARIES += libmt32.a
libmt32_a_SOURCES += \
	DelayReverb.cpp DelayReverb.h File.cpp File.h \
	fmath.h FreeverbModel.cpp FreeverbModel.h BReverbModel.cpp BReverbModel.h \
	mt32emu.h Part.cpp Part.h Partial.cpp Partial.h PartialManager.cpp \
	mmath.h PartialManager.h Poly.cpp Poly.h Structures.h Synth.cpp FileStream.cpp FileStream.h \
	Synth.h Tables.cpp Tables.h TVA.cpp TVA.h TVF.cpp TVF.h TVP.cpp \
	LA32Ramp.cpp LA32Ramp.h TVP.h AReverbModel.cpp AReverbModel.h \
	LA32WaveGenerator.cpp LA32WaveGenerator.h LegacyWaveGenerator.cpp LegacyWaveGenerator.h \
	./freeverb/allpass.cpp ./freeverb/allpass.h ./freeverb/comb.cpp \
	./freeverb/comb.h ./freeverb/denormals.h ./freeverb/revmodel.cpp \
	./freeverb/revmodel.h ./freeverb/tuning.h ROMInfo.cpp ROMInfo.h \
	./sha1/sha1.cpp ./sha1/sha1.h
endif

