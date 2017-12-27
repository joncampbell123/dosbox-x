cmake_minimum_required(VERSION 3.10)
project(dosbox-x C CXX) # C for CheckIncludeFiles

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(SRC_
	"src/dosbox.cpp"
	"src/dosbox.ico"
	#"src/dosbox.pkginfo"
	#"src/dosbox.plist"
	"src/dosbox.png"
	#"src/Makefile.am"
	#"src/Makefile.in"
	"src/resource.h"
	"src/winres.h"
	"src/winres.rc"
)
source_group("Sources" FILES ${SRC_})

set(SRC_AVIWRITER
	"src/aviwriter/avi.h"
	"src/aviwriter/avi_rw_iobuf.cpp"
	"src/aviwriter/avi_rw_iobuf.h"
	"src/aviwriter/avi_writer.cpp"
	"src/aviwriter/avi_writer.h"
	"src/aviwriter/guid.cpp"
	"src/aviwriter/guid.h"
	"src/aviwriter/ksdataformat.cpp"
	"src/aviwriter/ksdataformat.h"
	#"src/aviwriter/Makefile.am"
	#"src/aviwriter/Makefile.in"
	"src/aviwriter/riff.cpp"
	"src/aviwriter/riff.h"
	"src/aviwriter/riff_wav_writer.cpp"
	"src/aviwriter/riff_wav_writer.h"
)
source_group("Sources\\aviwriter" FILES ${SRC_AVIWRITER})

set(SRC_BUILTIN
	"src/builtin/25_com.cpp"
	"src/builtin/28_com.cpp"
	"src/builtin/50_com.cpp"
	"src/builtin/append_exe.cpp"
	"src/builtin/buffers_com.cpp"
	"src/builtin/copy_exe.cpp"
	"src/builtin/cwsdpmi_exe.cpp"
	"src/builtin/debug_exe.cpp"
	"src/builtin/device_com.cpp"
	"src/builtin/dos32a_exe.cpp"
	"src/builtin/dos4gw_exe.cpp"
	"src/builtin/dosidle_exe.cpp"
	"src/builtin/edit_com.cpp"
	"src/builtin/fcbs_com.cpp"
	"src/builtin/find_exe.cpp"
	"src/builtin/hexmem16_exe.cpp"
	"src/builtin/hexmem32_exe.cpp"
	"src/builtin/lastdriv_com.cpp"
	#"src/builtin/Makefile.am"
	#"src/builtin/Makefile.in"
	"src/builtin/mem_com.cpp"
	"src/builtin/move_exe.cpp"
	"src/builtin/tree_exe.cpp"
	"src/builtin/xcopy_exe.cpp"
)
source_group("Sources\\builtin" FILES ${SRC_BUILTIN})

set(SRC_CPU
	"src/cpu/callback.cpp"
	"src/cpu/core_dyn_x86.cpp"
	"src/cpu/core_full.cpp"
	"src/cpu/core_normal.cpp"
	"src/cpu/core_normal_286.cpp"
	"src/cpu/core_normal_8086.cpp"
	"src/cpu/core_prefetch.cpp"
	"src/cpu/core_simple.cpp"
	"src/cpu/cpu.cpp"
	"src/cpu/flags.cpp"
	"src/cpu/instructions.h"
	"src/cpu/lazyflags.h"
	#"src/cpu/Makefile.am"
	#"src/cpu/Makefile.in"
	"src/cpu/mmx.cpp"
	"src/cpu/modrm.cpp"
	"src/cpu/modrm.h"
	"src/cpu/paging.cpp"
)
source_group("Sources\\cpu" FILES ${SRC_CPU})

set(SRC_CPU_CORE_DYN

	"src/cpu/core_dyn_x86/cache.h"
	"src/cpu/core_dyn_x86/decoder.h"
	"src/cpu/core_dyn_x86/dyn_fpu.h"
	"src/cpu/core_dyn_x86/dyn_fpu_dh.h"
	"src/cpu/core_dyn_x86/helpers.h"
	#"src/cpu/core_dyn_x86/Makefile.am"
	#"src/cpu/core_dyn_x86/Makefile.in"
	"src/cpu/core_dyn_x86/mmx_gen.h"
	"src/cpu/core_dyn_x86/risc_x86.h"
	"src/cpu/core_dyn_x86/string.h"
)
source_group("Sources\\cpu\\core_dyn" FILES ${SRC_CPU_CORE_DYN})

set(SRC_CPU_CORE_FULL
	"src/cpu/core_full/ea_lookup.h"
	"src/cpu/core_full/load.h"
	"src/cpu/core_full/loadwrite.h"
	#"src/cpu/core_full/Makefile.am"
	#"src/cpu/core_full/Makefile.in"
	"src/cpu/core_full/op.h"
	"src/cpu/core_full/optable.h"
	"src/cpu/core_full/save.h"
	"src/cpu/core_full/string.h"
	"src/cpu/core_full/support.h"
)
source_group("Sources\\cpu\\core_full" FILES ${SRC_CPU_CORE_FULL})

set(SRC_CPU_CORE_NORMAL
	"src/cpu/core_normal/helpers.h"
	#"src/cpu/core_normal/Makefile.am"
	#"src/cpu/core_normal/Makefile.in"
	"src/cpu/core_normal/prefix_0f.h"
	"src/cpu/core_normal/prefix_0f_mmx.h"
	"src/cpu/core_normal/prefix_66.h"
	"src/cpu/core_normal/prefix_66_0f.h"
	"src/cpu/core_normal/prefix_none.h"
	"src/cpu/core_normal/string.h"
	"src/cpu/core_normal/support.h"
	"src/cpu/core_normal/table_ea.h"
	"src/cpu/core_normal/table_ea_8086.h"
)
source_group("Sources\\cpu\\core_normal" FILES ${SRC_CPU_CORE_NORMAL})

set(SRC_DEBUG
	"src/debug/debug.cpp"
	"src/debug/debug_disasm.cpp"
	"src/debug/debug_gui.cpp"
	"src/debug/debug_inc.h"
	"src/debug/debug_win32.cpp"
	"src/debug/disasm_tables.h"
	#"src/debug/Makefile.am"
	#"src/debug/Makefile.in"
)
source_group("Sources\\debug" FILES ${SRC_DEBUG})

set(SRC_DOS
	"src/dos/cdrom.cpp"
	"src/dos/cdrom.h"
	"src/dos/cdrom_aspi_win32.cpp"
	"src/dos/cdrom_image.cpp"
	"src/dos/cdrom_ioctl_linux.cpp"
	"src/dos/cdrom_ioctl_os2.cpp"
	"src/dos/cdrom_ioctl_win32.cpp"
	"src/dos/dev_con.h"
	"src/dos/dos.cpp"
	"src/dos/dos_classes.cpp"
	"src/dos/dos_codepages.h"
	"src/dos/dos_devices.cpp"
	"src/dos/dos_execute.cpp"
	"src/dos/dos_files.cpp"
	"src/dos/dos_ioctl.cpp"
	"src/dos/dos_keyboard_layout.cpp"
	"src/dos/dos_keyboard_layout_data.h"
	"src/dos/dos_memory.cpp"
	"src/dos/dos_misc.cpp"
	"src/dos/dos_mscdex.cpp"
	"src/dos/dos_network.h"
	"src/dos/dos_network2.h"
	"src/dos/dos_programs.cpp"
	"src/dos/dos_tables.cpp"
	"src/dos/drives.cpp"
	"src/dos/drives.h"
	"src/dos/drive_cache.cpp"
	"src/dos/drive_fat.cpp"
	"src/dos/drive_iso.cpp"
	"src/dos/drive_local.cpp"
	"src/dos/drive_virtual.cpp"
	#"src/dos/Makefile.am"
	#"src/dos/Makefile.in"
	"src/dos/scsidefs.h"
	"src/dos/wnaspi32.h"
)
source_group("Sources\\dos" FILES ${SRC_DOS})

set(SRC_FPU
	"src/fpu/fpu.cpp"
	"src/fpu/fpu_instructions.h"
	#"src/fpu/Makefile.am"
	#"src/fpu/Makefile.in"
)
source_group("Sources\\fpu" FILES ${SRC_FPU})

set(SRC_GUI
	"src/gui/d3d_components.h"
	"src/gui/direct3d.cpp"
	"src/gui/direct3d.h"
	"src/gui/dosbox.cga640.bmp.h"
	"src/gui/dosbox.vga16.bmp.h"
	"src/gui/dosbox_logo.h"
	"src/gui/hq2x_d3d.cpp"
	"src/gui/hq2x_d3d.h"
	#"src/gui/Makefile.am"
	#"src/gui/Makefile.in"
	"src/gui/menu.cpp"
	"src/gui/midi.cpp"
	"src/gui/midi_alsa.h"
	"src/gui/midi_coreaudio.h"
	"src/gui/midi_coremidi.h"
	"src/gui/midi_mt32.h"
	"src/gui/midi_oss.h"
	"src/gui/midi_synth.h"
	"src/gui/midi_timidity.h"
	"src/gui/midi_win32.h"
	"src/gui/render.cpp"
	"src/gui/render_loops.h"
	"src/gui/render_scalers.cpp"
	"src/gui/render_scalers.h"
	"src/gui/render_simple.h"
	"src/gui/render_templates.h"
	"src/gui/render_templates_hq.h"
	"src/gui/render_templates_hq2x.h"
	"src/gui/render_templates_hq3x.h"
	"src/gui/render_templates_sai.h"
	"src/gui/ScalingEffect.cpp"
	"src/gui/ScalingEffect.h"
	"src/gui/sdlmain.cpp"
	"src/gui/sdlmain_linux.cpp"
	"src/gui/sdl_gui.cpp"
	"src/gui/sdl_mapper.cpp"
)
source_group("Sources\\gui" FILES ${SRC_GUI})

set(SRC_HARDWARE
	"src/hardware/adlib.cpp"
	"src/hardware/adlib.h"
	"src/hardware/cmos.cpp"
	"src/hardware/dbopl.cpp"
	"src/hardware/dbopl.h"
	"src/hardware/disney.cpp"
	"src/hardware/dma.cpp"
	"src/hardware/dongle.cpp"
	"src/hardware/floppy.cpp"
	"src/hardware/gameblaster.cpp"
	"src/hardware/gus.cpp"
	"src/hardware/hardopl.cpp"
	"src/hardware/hardware.cpp"
	"src/hardware/ide.cpp"
	"src/hardware/innova.cpp"
	"src/hardware/iohandler.cpp"
	"src/hardware/ipx.cpp"
	"src/hardware/ipxserver.cpp"
	"src/hardware/joystick.cpp"
	"src/hardware/keyboard.cpp"
	#"src/hardware/Makefile.am"
	#"src/hardware/Makefile.in"
	"src/hardware/memory.cpp"
	"src/hardware/mixer.cpp"
	"src/hardware/mpu401.cpp"
	"src/hardware/ne2000.cpp"
	"src/hardware/nukedopl.cpp"
	"src/hardware/nukedopl.h"
	"src/hardware/opl.cpp"
	"src/hardware/opl.h"
	"src/hardware/pc98.cpp"
	"src/hardware/pc98_fm.cpp"
	"src/hardware/pci_bus.cpp"
	"src/hardware/pci_devices.h"
	"src/hardware/pcspeaker.cpp"
	"src/hardware/pic.cpp"
	"src/hardware/ps1_sound.cpp"
	"src/hardware/sblaster.cpp"
	"src/hardware/sn76496.h"
	"src/hardware/tandy_sound.cpp"
	"src/hardware/timer.cpp"
	"src/hardware/vga.cpp"
	"src/hardware/vga_attr.cpp"
	"src/hardware/vga_crtc.cpp"
	"src/hardware/vga_dac.cpp"
	"src/hardware/vga_draw.cpp"
	"src/hardware/vga_gfx.cpp"
	"src/hardware/vga_memory.cpp"
	"src/hardware/vga_misc.cpp"
	"src/hardware/vga_other.cpp"
	"src/hardware/vga_paradise.cpp"
	"src/hardware/vga_pc98_cg.cpp"
	"src/hardware/vga_pc98_crtc.cpp"
	"src/hardware/vga_pc98_dac.cpp"
	"src/hardware/vga_pc98_egc.cpp"
	"src/hardware/vga_pc98_gdc.cpp"
	"src/hardware/vga_s3.cpp"
	"src/hardware/vga_seq.cpp"
	"src/hardware/vga_tseng.cpp"
	"src/hardware/vga_xga.cpp"
	"src/hardware/voodoo.cpp"
	"src/hardware/voodoo_data.h"
	"src/hardware/voodoo_def.h"
	"src/hardware/voodoo_emu.cpp"
	"src/hardware/voodoo_emu.h"
	"src/hardware/voodoo_interface.cpp"
	"src/hardware/voodoo_interface.h"
	"src/hardware/voodoo_opengl.cpp"
	"src/hardware/voodoo_opengl.h"
	"src/hardware/voodoo_types.h"
	"src/hardware/voodoo_vogl.cpp"
	"src/hardware/voodoo_vogl.h"
)
source_group("Sources\\hardware" FILES ${SRC_HARDWARE})

set(SRC_HARDWARE_PARPORT
	"src/hardware/parport/directlpt_linux.cpp"
	"src/hardware/parport/directlpt_linux.h"
	"src/hardware/parport/directlpt_win32.cpp"
	"src/hardware/parport/directlpt_win32.h"
	"src/hardware/parport/filelpt.cpp"
	"src/hardware/parport/filelpt.h"
	#"src/hardware/parport/Makefile.am"
	#"src/hardware/parport/Makefile.in"
	"src/hardware/parport/parport.cpp"
	"src/hardware/parport/printer.cpp"
	"src/hardware/parport/printer.h"
	"src/hardware/parport/printer_charmaps.cpp"
	"src/hardware/parport/printer_charmaps.h"
	"src/hardware/parport/printer_if.h"
	"src/hardware/parport/printer_redir.h"
)
source_group("Sources\\hardware\\parport" FILES ${SRC_HARDWARE_PARPORT})

set(SRC_HARDWARE_RESID
	"src/hardware/reSID/envelope.cpp"
	"src/hardware/reSID/envelope.h"
	"src/hardware/reSID/extfilt.cpp"
	"src/hardware/reSID/extfilt.h"
	"src/hardware/reSID/filter.cpp"
	"src/hardware/reSID/filter.h"
	#"src/hardware/reSID/Makefile.am"
	#"src/hardware/reSID/Makefile.in"
	"src/hardware/reSID/pot.cpp"
	"src/hardware/reSID/pot.h"
	"src/hardware/reSID/q.txt"
	"src/hardware/reSID/sid.cpp"
	"src/hardware/reSID/sid.h"
	"src/hardware/reSID/siddefs.h"
	"src/hardware/reSID/spline.h"
	"src/hardware/reSID/version.cpp"
	"src/hardware/reSID/voice.cpp"
	"src/hardware/reSID/voice.h"
	"src/hardware/reSID/wave.cpp"
	"src/hardware/reSID/wave.h"
	"src/hardware/reSID/wave6581_PST.cpp"
	"src/hardware/reSID/wave6581_PS_.cpp"
	"src/hardware/reSID/wave6581_P_T.cpp"
	"src/hardware/reSID/wave6581__ST.cpp"
	"src/hardware/reSID/wave8580_PST.cpp"
	"src/hardware/reSID/wave8580_PS_.cpp"
	"src/hardware/reSID/wave8580_P_T.cpp"
	"src/hardware/reSID/wave8580__ST.cpp"
)
source_group("Sources\\hardware\\reSID" FILES ${SRC_HARDWARE_RESID})

set(SRC_HARDWARE_SERIALPORT
	"src/hardware/serialport/directserial.cpp"
	"src/hardware/serialport/directserial.h"
	"src/hardware/serialport/libserial.cpp"
	"src/hardware/serialport/libserial.h"
	#"src/hardware/serialport/Makefile.am"
	#"src/hardware/serialport/Makefile.in"
	"src/hardware/serialport/misc_util.cpp"
	"src/hardware/serialport/misc_util.h"
	"src/hardware/serialport/nullmodem.cpp"
	"src/hardware/serialport/nullmodem.h"
	"src/hardware/serialport/serialdummy.cpp"
	"src/hardware/serialport/serialdummy.h"
	"src/hardware/serialport/seriallog.cpp"
	"src/hardware/serialport/seriallog.h"
	"src/hardware/serialport/serialmouse.cpp"
	"src/hardware/serialport/serialmouse.h"
	"src/hardware/serialport/serialport.cpp"
	"src/hardware/serialport/softmodem.cpp"
	"src/hardware/serialport/softmodem.h"
)
source_group("Sources\\hardware\\serialport" FILES ${SRC_HARDWARE_SERIALPORT})

set(SRC_INTS
	"src/ints/bios.cpp"
	"src/ints/bios_disk.cpp"
	"src/ints/bios_keyboard.cpp"
	"src/ints/ems.cpp"
	"src/ints/int10.cpp"
	"src/ints/int10.h"
	"src/ints/int10_char.cpp"
	"src/ints/int10_memory.cpp"
	"src/ints/int10_misc.cpp"
	"src/ints/int10_modes.cpp"
	"src/ints/int10_pal.cpp"
	"src/ints/int10_put_pixel.cpp"
	"src/ints/int10_vesa.cpp"
	"src/ints/int10_video_state.cpp"
	"src/ints/int10_vptable.cpp"
	#"src/ints/Makefile.am"
	#"src/ints/Makefile.in"
	"src/ints/mouse.cpp"
	"src/ints/qcow2_disk.cpp"
	"src/ints/xms.cpp"
	"src/ints/xms.h"
)
source_group("Sources\\ints" FILES ${SRC_INTS})

set(SRC_LIBS
	#"src/libs/Makefile.am"
	#"src/libs/Makefile.in"
)
source_group("Sources\\libs" FILES ${SRC_LIBS})

set(SRC_LIBS_GUI_TK
	"src/libs/gui_tk/gui_tk.cpp"
	"src/libs/gui_tk/gui_tk.h"
	#"src/libs/gui_tk/Makefile.am"
	#"src/libs/gui_tk/Makefile.in"
)
source_group("Sources\\libs\\gui_tk" FILES ${SRC_LIBS_GUI_TK})

set(SRC_LIBS_PORTTALK
	#"src/libs/porttalk/Makefile.am"
	#"src/libs/porttalk/Makefile.in"
	"src/libs/porttalk/porttalk.cpp"
	"src/libs/porttalk/porttalk.h"
	"src/libs/porttalk/PortTalk_IOCTL.h"
)
source_group("Sources\\libs\\porttalk" FILES ${SRC_LIBS_PORTTALK})

set(SRC_LIBS_ZMBV
	"src/libs/zmbv/drvproc.cpp"
	#"src/libs/zmbv/Makefile.am"
	#"src/libs/zmbv/Makefile.in"
	"src/libs/zmbv/resource.h"
	"src/libs/zmbv/zmbv.cpp"
	"src/libs/zmbv/zmbv.def"
	"src/libs/zmbv/zmbv.h"
	"src/libs/zmbv/zmbv.inf"
	#"src/libs/zmbv/zmbv.sln"
	#"src/libs/zmbv/zmbv.vcproj"
	"src/libs/zmbv/zmbv_vfw.cpp"
	"src/libs/zmbv/zmbv_vfw.h"
	"src/libs/zmbv/zmbv_vfw.rc"
)
source_group("Sources\\libs\\zmbv" FILES ${SRC_LIBS_ZMBV})

set(SRC_MISC
	"src/misc/cross.cpp"
	#"src/misc/Makefile.am"
	#"src/misc/Makefile.in"
	"src/misc/messages.cpp"
	"src/misc/programs.cpp"
	"src/misc/regionalloctracking.cpp"
	"src/misc/setup.cpp"
	"src/misc/support.cpp"
)
source_group("Sources\\misc" FILES ${SRC_MISC})

set(SRC_MT32
	"src/mt32/AReverbModel.cpp"
	"src/mt32/AReverbModel.h"
	"src/mt32/BReverbModel.cpp"
	"src/mt32/BReverbModel.h"
	"src/mt32/DelayReverb.cpp"
	"src/mt32/DelayReverb.h"
	"src/mt32/File.cpp"
	"src/mt32/File.h"
	"src/mt32/FileStream.cpp"
	"src/mt32/FileStream.h"
	"src/mt32/FreeverbModel.cpp"
	"src/mt32/FreeverbModel.h"
	"src/mt32/LA32Ramp.cpp"
	"src/mt32/LA32Ramp.h"
	"src/mt32/LA32WaveGenerator.cpp"
	"src/mt32/LA32WaveGenerator.h"
	"src/mt32/LegacyWaveGenerator.cpp"
	"src/mt32/LegacyWaveGenerator.h"
	#"src/mt32/Makefile.am"
	#"src/mt32/Makefile.in"
	"src/mt32/mmath.h"
	"src/mt32/mt32emu.h"
	"src/mt32/Part.cpp"
	"src/mt32/Part.h"
	"src/mt32/Partial.cpp"
	"src/mt32/Partial.h"
	"src/mt32/PartialManager.cpp"
	"src/mt32/PartialManager.h"
	"src/mt32/Poly.cpp"
	"src/mt32/Poly.h"
	"src/mt32/ROMInfo.cpp"
	"src/mt32/ROMInfo.h"
	"src/mt32/Structures.h"
	"src/mt32/Synth.cpp"
	"src/mt32/Synth.h"
	"src/mt32/Tables.cpp"
	"src/mt32/Tables.h"
	"src/mt32/TVA.cpp"
	"src/mt32/TVA.h"
	"src/mt32/TVF.cpp"
	"src/mt32/TVF.h"
	"src/mt32/TVP.cpp"
	"src/mt32/TVP.h"
)
source_group("Sources\\mt32" FILES ${SRC_MT32})

set(SRC_MT32_FREEVERB
	"src/mt32/freeverb/allpass.cpp"
	"src/mt32/freeverb/allpass.h"
	"src/mt32/freeverb/comb.cpp"
	"src/mt32/freeverb/comb.h"
	"src/mt32/freeverb/denormals.h"
	"src/mt32/freeverb/revmodel.cpp"
	"src/mt32/freeverb/revmodel.h"
	"src/mt32/freeverb/tuning.h"
)
source_group("Sources\\mt32\\freeverb" FILES ${SRC_MT32_FREEVERB})

set(SRC_MT32_SHA1
	"src/mt32/sha1/license.txt"
	"src/mt32/sha1/sha1.cpp"
	"src/mt32/sha1/sha1.h"
)
source_group("Sources\\mt32\\sha1" FILES ${SRC_MT32_SHA1})

set(SRC_PLATFORM
	#"src/platform/Makefile.am"
	#"src/platform/Makefile.in"
)
source_group("Sources\\platform" FILES ${SRC_PLATFORM})

set(SRC_SHELL
	#"src/shell/Makefile.am"
	#"src/shell/Makefile.in"
	"src/shell/shell.cpp"
	"src/shell/shell_batch.cpp"
	"src/shell/shell_cmds.cpp"
	"src/shell/shell_misc.cpp"
)
source_group("Sources\\shell" FILES ${SRC_SHELL})

add_executable(dosbox-x
	${SRC_}
	${SRC_AVIWRITER}
	${SRC_BUILTIN}
	${SRC_CPU}
	${SRC_CPU_CORE_DYN}
	${SRC_CPU_CORE_FULL}
	${SRC_CPU_CORE_NORMAL}
	${SRC_DEBUG}
	${SRC_DOS}
	${SRC_FPU}
	${SRC_GUI}
	${SRC_HARDWARE}
	${SRC_HARDWARE_PARPORT}
	${SRC_HARDWARE_RESID}
	${SRC_HARDWARE_SERIALPORT}
	${SRC_INTS}
	${SRC_LIBS}
	${SRC_LIBS_GUI_TK}
	${SRC_LIBS_PORTTALK}
	${SRC_LIBS_ZMBV}
	${SRC_MISC}
	${SRC_MT32}
	${SRC_MT32_FREEVERB}
	${SRC_MT32_SHA1}
	${SRC_PLATFORM}
	${SRC_SHELL}
)

set_target_properties(dosbox-x PROPERTIES LINKER_LANGUAGE CXX)

