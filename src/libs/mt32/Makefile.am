AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES =
libmt32_a_SOURCES =

if C_MT32
noinst_LIBRARIES += libmt32.a
libmt32_a_SOURCES += \
	SampleRateConverter.cpp SampleRateConverter.h File.cpp File.h \
	fmath.h MidiStreamParser.cpp MidiStreamParser.h BReverbModel.cpp BReverbModel.h \
	mt32emu.h Part.cpp Part.h Partial.cpp Partial.h PartialManager.cpp \
	mmath.h PartialManager.h Poly.cpp Poly.h Structures.h Synth.cpp FileStream.cpp FileStream.h \
	Synth.h Tables.cpp Tables.h TVA.cpp TVA.h TVF.cpp TVF.h TVP.cpp \
	LA32Ramp.cpp LA32Ramp.h TVP.h Analog.cpp Analog.h \
	LA32WaveGenerator.cpp LA32WaveGenerator.h LA32FloatWaveGenerator.cpp LA32FloatWaveGenerator.h \
	./c_interface/c_interface.cpp ./c_interface/c_interface.h ./c_interface/c_types.h \
	./c_interface/cpp_interface.h ROMInfo.cpp ROMInfo.h \
	./sha1/sha1.cpp ./sha1/sha1.h
endif

