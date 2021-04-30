/* Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011-2021 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MT32EMU_CPP_INTERFACE_H
#define MT32EMU_CPP_INTERFACE_H

#include <cstdarg>

#include "../globals.h"
#include "c_types.h"

#include "../Types.h"
#include "../Enumerations.h"

#if MT32EMU_API_TYPE == 2

extern "C" {

/** Returns mt32emu_service_i interface. */
mt32emu_service_i mt32emu_get_service_i();

}

#define mt32emu_get_supported_report_handler_version i.v0->getSupportedReportHandlerVersionID
#define mt32emu_get_supported_midi_receiver_version i.v0->getSupportedMIDIReceiverVersionID
#define mt32emu_get_library_version_int i.v0->getLibraryVersionInt
#define mt32emu_get_library_version_string i.v0->getLibraryVersionString
#define mt32emu_get_stereo_output_samplerate i.v0->getStereoOutputSamplerate
#define mt32emu_get_best_analog_output_mode iV1()->getBestAnalogOutputMode
#define mt32emu_get_machine_ids iV4()->getMachineIDs
#define mt32emu_get_rom_ids iV4()->getROMIDs
#define mt32emu_identify_rom_data iV4()->identifyROMData
#define mt32emu_identify_rom_file iV4()->identifyROMFile
#define mt32emu_create_context i.v0->createContext
#define mt32emu_free_context i.v0->freeContext
#define mt32emu_add_rom_data i.v0->addROMData
#define mt32emu_add_rom_file i.v0->addROMFile
#define mt32emu_merge_and_add_rom_data iV4()->mergeAndAddROMData
#define mt32emu_merge_and_add_rom_files iV4()->mergeAndAddROMFiles
#define mt32emu_add_machine_rom_file iV4()->addMachineROMFile
#define mt32emu_get_rom_info i.v0->getROMInfo
#define mt32emu_set_partial_count i.v0->setPartialCount
#define mt32emu_set_analog_output_mode i.v0->setAnalogOutputMode
#define mt32emu_set_stereo_output_samplerate iV1()->setStereoOutputSampleRate
#define mt32emu_set_samplerate_conversion_quality iV1()->setSamplerateConversionQuality
#define mt32emu_select_renderer_type iV1()->selectRendererType
#define mt32emu_get_selected_renderer_type iV1()->getSelectedRendererType
#define mt32emu_open_synth i.v0->openSynth
#define mt32emu_close_synth i.v0->closeSynth
#define mt32emu_is_open i.v0->isOpen
#define mt32emu_get_actual_stereo_output_samplerate i.v0->getActualStereoOutputSamplerate
#define mt32emu_convert_output_to_synth_timestamp iV1()->convertOutputToSynthTimestamp
#define mt32emu_convert_synth_to_output_timestamp iV1()->convertSynthToOutputTimestamp
#define mt32emu_flush_midi_queue i.v0->flushMIDIQueue
#define mt32emu_set_midi_event_queue_size i.v0->setMIDIEventQueueSize
#define mt32emu_configure_midi_event_queue_sysex_storage iV3()->configureMIDIEventQueueSysexStorage
#define mt32emu_set_midi_receiver i.v0->setMIDIReceiver
#define mt32emu_get_internal_rendered_sample_count iV2()->getInternalRenderedSampleCount
#define mt32emu_parse_stream i.v0->parseStream
#define mt32emu_parse_stream_at i.v0->parseStream_At
#define mt32emu_play_short_message i.v0->playShortMessage
#define mt32emu_play_short_message_at i.v0->playShortMessageAt
#define mt32emu_play_msg i.v0->playMsg
#define mt32emu_play_sysex i.v0->playSysex
#define mt32emu_play_msg_at i.v0->playMsgAt
#define mt32emu_play_sysex_at i.v0->playSysexAt
#define mt32emu_play_msg_now i.v0->playMsgNow
#define mt32emu_play_msg_on_part i.v0->playMsgOnPart
#define mt32emu_play_sysex_now i.v0->playSysexNow
#define mt32emu_write_sysex i.v0->writeSysex
#define mt32emu_set_reverb_enabled i.v0->setReverbEnabled
#define mt32emu_is_reverb_enabled i.v0->isReverbEnabled
#define mt32emu_set_reverb_overridden i.v0->setReverbOverridden
#define mt32emu_is_reverb_overridden i.v0->isReverbOverridden
#define mt32emu_set_reverb_compatibility_mode i.v0->setReverbCompatibilityMode
#define mt32emu_is_mt32_reverb_compatibility_mode i.v0->isMT32ReverbCompatibilityMode
#define mt32emu_is_default_reverb_mt32_compatible i.v0->isDefaultReverbMT32Compatible
#define mt32emu_preallocate_reverb_memory iV3()->preallocateReverbMemory
#define mt32emu_set_dac_input_mode i.v0->setDACInputMode
#define mt32emu_get_dac_input_mode i.v0->getDACInputMode
#define mt32emu_set_midi_delay_mode i.v0->setMIDIDelayMode
#define mt32emu_get_midi_delay_mode i.v0->getMIDIDelayMode
#define mt32emu_set_output_gain i.v0->setOutputGain
#define mt32emu_get_output_gain i.v0->getOutputGain
#define mt32emu_set_reverb_output_gain i.v0->setReverbOutputGain
#define mt32emu_get_reverb_output_gain i.v0->getReverbOutputGain
#define mt32emu_set_reversed_stereo_enabled i.v0->setReversedStereoEnabled
#define mt32emu_is_reversed_stereo_enabled i.v0->isReversedStereoEnabled
#define mt32emu_set_nice_amp_ramp_enabled iV2()->setNiceAmpRampEnabled
#define mt32emu_is_nice_amp_ramp_enabled iV2()->isNiceAmpRampEnabled
#define mt32emu_set_nice_panning_enabled iV3()->setNicePanningEnabled
#define mt32emu_is_nice_panning_enabled iV3()->isNicePanningEnabled
#define mt32emu_set_nice_partial_mixing_enabled iV3()->setNicePartialMixingEnabled
#define mt32emu_is_nice_partial_mixing_enabled iV3()->isNicePartialMixingEnabled
#define mt32emu_render_int16_t i.v0->renderint16_t
#define mt32emu_render_float i.v0->renderFloat
#define mt32emu_render_int16_t_streams i.v0->renderint16_tStreams
#define mt32emu_render_float_streams i.v0->renderFloatStreams
#define mt32emu_has_active_partials i.v0->hasActivePartials
#define mt32emu_is_active i.v0->isActive
#define mt32emu_get_partial_count i.v0->getPartialCount
#define mt32emu_get_part_states i.v0->getPartStates
#define mt32emu_get_partial_states i.v0->getPartialStates
#define mt32emu_get_playing_notes i.v0->getPlayingNotes
#define mt32emu_get_patch_name i.v0->getPatchName
#define mt32emu_read_memory i.v0->readMemory

#else // #if MT32EMU_API_TYPE == 2

#include "c_interface.h"

#endif // #if MT32EMU_API_TYPE == 2

namespace MT32Emu {

namespace CppInterfaceImpl {

static const mt32emu_report_handler_i NULL_REPORT_HANDLER = { NULL };
static mt32emu_report_handler_i getReportHandlerThunk();
static mt32emu_midi_receiver_i getMidiReceiverThunk();

}

/*
 * The classes below correspond to the interfaces defined in c_types.h and provided for convenience when using C++.
 * The approach used makes no assumption of any internal class data memory layout, since the C++ standard does not
 * provide any detail in this area and leaves it up to the implementation. Therefore, this way portability is guaranteed,
 * despite the implementation may be a little inefficient.
 * See c_types.h and c_interface.h for description of the corresponding interface methods.
 */

// Defines the interface for handling reported events.
// Corresponds to the current version of mt32emu_report_handler_i interface.
class IReportHandler {
public:
	virtual void printDebug(const char *fmt, va_list list) = 0;
	virtual void onErrorControlROM() = 0;
	virtual void onErrorPCMROM() = 0;
	virtual void showLCDMessage(const char *message) = 0;
	virtual void onMIDIMessagePlayed() = 0;
	virtual bool onMIDIQueueOverflow() = 0;
	virtual void onMIDISystemRealtime(uint8_t system_realtime) = 0;
	virtual void onDeviceReset() = 0;
	virtual void onDeviceReconfig() = 0;
	virtual void onNewReverbMode(uint8_t mode) = 0;
	virtual void onNewReverbTime(uint8_t time) = 0;
	virtual void onNewReverbLevel(uint8_t level) = 0;
	virtual void onPolyStateChanged(uint8_t part_num) = 0;
	virtual void onProgramChanged(uint8_t part_num, const char *sound_group_name, const char *patch_name) = 0;

protected:
	~IReportHandler() {}
};

// Defines the interface for receiving MIDI messages generated by MIDI stream parser.
// Corresponds to the current version of mt32emu_midi_receiver_i interface.
class IMidiReceiver {
public:
	virtual void handleShortMessage(const uint32_t message) = 0;
	virtual void handleSysex(const uint8_t stream[], const uint32_t length) = 0;
	virtual void handleSystemRealtimeMessage(const uint8_t realtime) = 0;

protected:
	~IMidiReceiver() {}
};

// Defines all the library services.
// Corresponds to the current version of mt32emu_service_i interface.
class Service {
public:
#if MT32EMU_API_TYPE == 2
	explicit Service(mt32emu_service_i interface, mt32emu_context context = NULL) : i(interface), c(context) {}
#else
	explicit Service(mt32emu_context context = NULL) : c(context) {}
#endif
	~Service() { if (c != NULL) mt32emu_free_context(c); }

	// Context-independent methods

#if MT32EMU_API_TYPE == 2
	mt32emu_service_version getVersionID() { return i.v0->getVersionID(i); }
#endif
	mt32emu_report_handler_version getSupportedReportHandlerVersionID() { return mt32emu_get_supported_report_handler_version(); }
	mt32emu_midi_receiver_version getSupportedMIDIReceiverVersionID() { return mt32emu_get_supported_midi_receiver_version(); }

	uint32_t getLibraryVersionInt() { return mt32emu_get_library_version_int(); }
	const char *getLibraryVersionString() { return mt32emu_get_library_version_string(); }

	uint32_t getStereoOutputSamplerate(const AnalogOutputMode analog_output_mode) { return mt32emu_get_stereo_output_samplerate(static_cast<mt32emu_analog_output_mode>(analog_output_mode)); }
	AnalogOutputMode getBestAnalogOutputMode(const double target_samplerate) { return static_cast<AnalogOutputMode>(mt32emu_get_best_analog_output_mode(target_samplerate)); }

	size_t getMachineIDs(const char **machine_ids, size_t machine_ids_size) { return mt32emu_get_machine_ids(machine_ids, machine_ids_size); }
	size_t getROMIDs(const char **rom_ids, size_t rom_ids_size, const char *machine_id) { return mt32emu_get_rom_ids(rom_ids, rom_ids_size, machine_id); }
	mt32emu_return_code identifyROMData(mt32emu_rom_info *rom_info, const uint8_t *data, size_t data_size, const char *machine_id) { return mt32emu_identify_rom_data(rom_info, data, data_size, machine_id); }
	mt32emu_return_code identifyROMFile(mt32emu_rom_info *rom_info, const char *filename, const char *machine_id) { return mt32emu_identify_rom_file(rom_info, filename, machine_id); }

	// Context-dependent methods

	mt32emu_context getContext() { return c; }
	void createContext(mt32emu_report_handler_i report_handler = CppInterfaceImpl::NULL_REPORT_HANDLER, void *instance_data = NULL) { freeContext(); c = mt32emu_create_context(report_handler, instance_data); }
	void createContext(IReportHandler &report_handler) { createContext(CppInterfaceImpl::getReportHandlerThunk(), &report_handler); }
	void freeContext() { if (c != NULL) { mt32emu_free_context(c); c = NULL; } }
	mt32emu_return_code addROMData(const uint8_t *data, size_t data_size, const mt32emu_sha1_digest *sha1_digest = NULL) { return mt32emu_add_rom_data(c, data, data_size, sha1_digest); }
	mt32emu_return_code addROMFile(const char *filename) { return mt32emu_add_rom_file(c, filename); }
	mt32emu_return_code mergeAndAddROMData(const uint8_t *part1_data, size_t part1_data_size, const uint8_t *part2_data, size_t part2_data_size) { return mt32emu_merge_and_add_rom_data(c, part1_data, part1_data_size, NULL, part2_data, part2_data_size, NULL); }
	mt32emu_return_code mergeAndAddROMData(const uint8_t *part1_data, size_t part1_data_size, const mt32emu_sha1_digest *part1_sha1_digest, const uint8_t *part2_data, size_t part2_data_size, const mt32emu_sha1_digest *part2_sha1_digest) { return mt32emu_merge_and_add_rom_data(c, part1_data, part1_data_size, part1_sha1_digest, part2_data, part2_data_size, part2_sha1_digest); }
	mt32emu_return_code mergeAndAddROMFiles(const char *part1_filename, const char *part2_filename) { return mt32emu_merge_and_add_rom_files(c, part1_filename, part2_filename); }
	mt32emu_return_code addMachineROMFile(const char *machine_id, const char *filename) { return mt32emu_add_machine_rom_file(c, machine_id, filename); }
	void getROMInfo(mt32emu_rom_info *rom_info) { mt32emu_get_rom_info(c, rom_info); }
	void setPartialCount(const uint32_t partial_count) { mt32emu_set_partial_count(c, partial_count); }
	void setAnalogOutputMode(const AnalogOutputMode analog_output_mode) { mt32emu_set_analog_output_mode(c, static_cast<mt32emu_analog_output_mode>(analog_output_mode)); }
	void setStereoOutputSampleRate(const double samplerate) { mt32emu_set_stereo_output_samplerate(c, samplerate); }
	void setSamplerateConversionQuality(const SamplerateConversionQuality quality) { mt32emu_set_samplerate_conversion_quality(c, static_cast<mt32emu_samplerate_conversion_quality>(quality)); }
	void selectRendererType(const RendererType newRendererType) { mt32emu_select_renderer_type(c, static_cast<mt32emu_renderer_type>(newRendererType)); }
	RendererType getSelectedRendererType() { return static_cast<RendererType>(mt32emu_get_selected_renderer_type(c)); }
	mt32emu_return_code openSynth() { return mt32emu_open_synth(c); }
	void closeSynth() { mt32emu_close_synth(c); }
	bool isOpen() { return mt32emu_is_open(c) != MT32EMU_BOOL_FALSE; }
	uint32_t getActualStereoOutputSamplerate() { return mt32emu_get_actual_stereo_output_samplerate(c); }
	uint32_t convertOutputToSynthTimestamp(uint32_t output_timestamp) { return mt32emu_convert_output_to_synth_timestamp(c, output_timestamp); }
	uint32_t convertSynthToOutputTimestamp(uint32_t synth_timestamp) { return mt32emu_convert_synth_to_output_timestamp(c, synth_timestamp); }
	void flushMIDIQueue() { mt32emu_flush_midi_queue(c); }
	uint32_t setMIDIEventQueueSize(const uint32_t queue_size) { return mt32emu_set_midi_event_queue_size(c, queue_size); }
	void configureMIDIEventQueueSysexStorage(const uint32_t storage_buffer_size) { mt32emu_configure_midi_event_queue_sysex_storage(c, storage_buffer_size); }
	void setMIDIReceiver(mt32emu_midi_receiver_i midi_receiver, void *instance_data) { mt32emu_set_midi_receiver(c, midi_receiver, instance_data); }
	void setMIDIReceiver(IMidiReceiver &midi_receiver) { setMIDIReceiver(CppInterfaceImpl::getMidiReceiverThunk(), &midi_receiver); }

	uint32_t getInternalRenderedSampleCount() { return mt32emu_get_internal_rendered_sample_count(c); }
	void parseStream(const uint8_t *stream, uint32_t length) { mt32emu_parse_stream(c, stream, length); }
	void parseStream_At(const uint8_t *stream, uint32_t length, uint32_t timestamp) { mt32emu_parse_stream_at(c, stream, length, timestamp); }
	void playShortMessage(uint32_t message) { mt32emu_play_short_message(c, message); }
	void playShortMessageAt(uint32_t message, uint32_t timestamp) { mt32emu_play_short_message_at(c, message, timestamp); }
	mt32emu_return_code playMsg(uint32_t msg) { return mt32emu_play_msg(c, msg); }
	mt32emu_return_code playSysex(const uint8_t *sysex, uint32_t len) { return mt32emu_play_sysex(c, sysex, len); }
	mt32emu_return_code playMsgAt(uint32_t msg, uint32_t timestamp) { return mt32emu_play_msg_at(c, msg, timestamp); }
	mt32emu_return_code playSysexAt(const uint8_t *sysex, uint32_t len, uint32_t timestamp) { return mt32emu_play_sysex_at(c, sysex, len, timestamp); }

	void playMsgNow(uint32_t msg) { mt32emu_play_msg_now(c, msg); }
	void playMsgOnPart(uint8_t part, uint8_t code, uint8_t note, uint8_t velocity) { mt32emu_play_msg_on_part(c, part, code, note, velocity); }
	void playSysexNow(const uint8_t *sysex, uint32_t len) { mt32emu_play_sysex_now(c, sysex, len); }
	void writeSysex(uint8_t channel, const uint8_t *sysex, uint32_t len) { mt32emu_write_sysex(c, channel, sysex, len); }

	void setReverbEnabled(const bool reverb_enabled) { mt32emu_set_reverb_enabled(c, reverb_enabled ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE); }
	bool isReverbEnabled() { return mt32emu_is_reverb_enabled(c) != MT32EMU_BOOL_FALSE; }
	void setReverbOverridden(const bool reverb_overridden) { mt32emu_set_reverb_overridden(c, reverb_overridden ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE); }
	bool isReverbOverridden() { return mt32emu_is_reverb_overridden(c) != MT32EMU_BOOL_FALSE; }
	void setReverbCompatibilityMode(const bool mt32_compatible_mode) { mt32emu_set_reverb_compatibility_mode(c, mt32_compatible_mode ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE); }
	bool isMT32ReverbCompatibilityMode() { return mt32emu_is_mt32_reverb_compatibility_mode(c) != MT32EMU_BOOL_FALSE; }
	bool isDefaultReverbMT32Compatible() { return mt32emu_is_default_reverb_mt32_compatible(c) != MT32EMU_BOOL_FALSE; }
	void preallocateReverbMemory(const bool enabled) { mt32emu_preallocate_reverb_memory(c, enabled ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE); }

	void setDACInputMode(const DACInputMode mode) { mt32emu_set_dac_input_mode(c, static_cast<mt32emu_dac_input_mode>(mode)); }
	DACInputMode getDACInputMode() { return static_cast<DACInputMode>(mt32emu_get_dac_input_mode(c)); }

	void setMIDIDelayMode(const MIDIDelayMode mode) { mt32emu_set_midi_delay_mode(c, static_cast<mt32emu_midi_delay_mode>(mode)); }
	MIDIDelayMode getMIDIDelayMode() { return static_cast<MIDIDelayMode>(mt32emu_get_midi_delay_mode(c)); }

	void setOutputGain(float gain) { mt32emu_set_output_gain(c, gain); }
	float getOutputGain() { return mt32emu_get_output_gain(c); }
	void setReverbOutputGain(float gain) { mt32emu_set_reverb_output_gain(c, gain); }
	float getReverbOutputGain() { return mt32emu_get_reverb_output_gain(c); }

	void setReversedStereoEnabled(const bool enabled) { mt32emu_set_reversed_stereo_enabled(c, enabled ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE); }
	bool isReversedStereoEnabled() { return mt32emu_is_reversed_stereo_enabled(c) != MT32EMU_BOOL_FALSE; }

	void setNiceAmpRampEnabled(const bool enabled) { mt32emu_set_nice_amp_ramp_enabled(c, enabled ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE); }
	bool isNiceAmpRampEnabled() { return mt32emu_is_nice_amp_ramp_enabled(c) != MT32EMU_BOOL_FALSE; }

	void setNicePanningEnabled(const bool enabled) { mt32emu_set_nice_panning_enabled(c, enabled ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE); }
	bool isNicePanningEnabled() { return mt32emu_is_nice_panning_enabled(c) != MT32EMU_BOOL_FALSE; }

	void setNicePartialMixingEnabled(const bool enabled) { mt32emu_set_nice_partial_mixing_enabled(c, enabled ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE); }
	bool isNicePartialMixingEnabled() { return mt32emu_is_nice_partial_mixing_enabled(c) != MT32EMU_BOOL_FALSE; }

	void renderint16_t(int16_t *stream, uint32_t len) { mt32emu_render_int16_t(c, stream, len); }
	void renderFloat(float *stream, uint32_t len) { mt32emu_render_float(c, stream, len); }
	void renderint16_tStreams(const mt32emu_dac_output_int16_t_streams *streams, uint32_t len) { mt32emu_render_int16_t_streams(c, streams, len); }
	void renderFloatStreams(const mt32emu_dac_output_float_streams *streams, uint32_t len) { mt32emu_render_float_streams(c, streams, len); }

	bool hasActivePartials() { return mt32emu_has_active_partials(c) != MT32EMU_BOOL_FALSE; }
	bool isActive() { return mt32emu_is_active(c) != MT32EMU_BOOL_FALSE; }
	uint32_t getPartialCount() { return mt32emu_get_partial_count(c); }
	uint32_t getPartStates() { return mt32emu_get_part_states(c); }
	void getPartialStates(uint8_t *partial_states) { mt32emu_get_partial_states(c, partial_states); }
	uint32_t getPlayingNotes(uint8_t part_number, uint8_t *keys, uint8_t *velocities) { return mt32emu_get_playing_notes(c, part_number, keys, velocities); }
	const char *getPatchName(uint8_t part_number) { return mt32emu_get_patch_name(c, part_number); }
	void readMemory(uint32_t addr, uint32_t len, uint8_t *data) { mt32emu_read_memory(c, addr, len, data); }

private:
#if MT32EMU_API_TYPE == 2
	const mt32emu_service_i i;
#endif
	mt32emu_context c;

#if MT32EMU_API_TYPE == 2
	const mt32emu_service_i_v1 *iV1() { return (getVersionID() < MT32EMU_SERVICE_VERSION_1) ? NULL : i.v1; }
	const mt32emu_service_i_v2 *iV2() { return (getVersionID() < MT32EMU_SERVICE_VERSION_2) ? NULL : i.v2; }
	const mt32emu_service_i_v3 *iV3() { return (getVersionID() < MT32EMU_SERVICE_VERSION_3) ? NULL : i.v3; }
	const mt32emu_service_i_v4 *iV4() { return (getVersionID() < MT32EMU_SERVICE_VERSION_4) ? NULL : i.v4; }
#endif

	Service(const Service &);            // prevent copy-construction
	Service& operator=(const Service &); // prevent assignment
};

namespace CppInterfaceImpl {

static mt32emu_report_handler_version getReportHandlerVersionID(mt32emu_report_handler_i) {
	return MT32EMU_REPORT_HANDLER_VERSION_CURRENT;
}

static void printDebug(void *instance_data, const char *fmt, va_list list) {
	static_cast<IReportHandler *>(instance_data)->printDebug(fmt, list);
}

static void onErrorControlROM(void *instance_data) {
	static_cast<IReportHandler *>(instance_data)->onErrorControlROM();
}

static void onErrorPCMROM(void *instance_data) {
	static_cast<IReportHandler *>(instance_data)->onErrorPCMROM();
}

static void showLCDMessage(void *instance_data, const char *message) {
	static_cast<IReportHandler *>(instance_data)->showLCDMessage(message);
}

static void onMIDIMessagePlayed(void *instance_data) {
	static_cast<IReportHandler *>(instance_data)->onMIDIMessagePlayed();
}

static mt32emu_boolean onMIDIQueueOverflow(void *instance_data) {
	return static_cast<IReportHandler *>(instance_data)->onMIDIQueueOverflow() ? MT32EMU_BOOL_TRUE : MT32EMU_BOOL_FALSE;
}

static void onMIDISystemRealtime(void *instance_data, mt32emu_uint8_t system_realtime) {
	static_cast<IReportHandler *>(instance_data)->onMIDISystemRealtime(system_realtime);
}

static void onDeviceReset(void *instance_data) {
	static_cast<IReportHandler *>(instance_data)->onDeviceReset();
}

static void onDeviceReconfig(void *instance_data) {
	static_cast<IReportHandler *>(instance_data)->onDeviceReconfig();
}

static void onNewReverbMode(void *instance_data, mt32emu_uint8_t mode) {
	static_cast<IReportHandler *>(instance_data)->onNewReverbMode(mode);
}

static void onNewReverbTime(void *instance_data, mt32emu_uint8_t time) {
	static_cast<IReportHandler *>(instance_data)->onNewReverbTime(time);
}

static void onNewReverbLevel(void *instance_data, mt32emu_uint8_t level) {
	static_cast<IReportHandler *>(instance_data)->onNewReverbLevel(level);
}

static void onPolyStateChanged(void *instance_data, mt32emu_uint8_t part_num) {
	static_cast<IReportHandler *>(instance_data)->onPolyStateChanged(part_num);
}

static void onProgramChanged(void *instance_data, mt32emu_uint8_t part_num, const char *sound_group_name, const char *patch_name) {
	static_cast<IReportHandler *>(instance_data)->onProgramChanged(part_num, sound_group_name, patch_name);
}

static mt32emu_report_handler_i getReportHandlerThunk() {
	static const mt32emu_report_handler_i_v0 REPORT_HANDLER_V0_THUNK = {
		getReportHandlerVersionID,
		printDebug,
		onErrorControlROM,
		onErrorPCMROM,
		showLCDMessage,
		onMIDIMessagePlayed,
		onMIDIQueueOverflow,
		onMIDISystemRealtime,
		onDeviceReset,
		onDeviceReconfig,
		onNewReverbMode,
		onNewReverbTime,
		onNewReverbLevel,
		onPolyStateChanged,
		onProgramChanged
	};

	static const mt32emu_report_handler_i REPORT_HANDLER_THUNK = { &REPORT_HANDLER_V0_THUNK };

	return REPORT_HANDLER_THUNK;
}

static mt32emu_midi_receiver_version getMidiReceiverVersionID(mt32emu_midi_receiver_i) {
	return MT32EMU_MIDI_RECEIVER_VERSION_CURRENT;
}

static void handleShortMessage(void *instance_data, const mt32emu_uint32_t message) {
	static_cast<IMidiReceiver *>(instance_data)->handleShortMessage(message);
}

static void handleSysex(void *instance_data, const mt32emu_uint8_t stream[], const mt32emu_uint32_t length) {
	static_cast<IMidiReceiver *>(instance_data)->handleSysex(stream, length);
}

static void handleSystemRealtimeMessage(void *instance_data, const mt32emu_uint8_t realtime) {
	static_cast<IMidiReceiver *>(instance_data)->handleSystemRealtimeMessage(realtime);
}

static mt32emu_midi_receiver_i getMidiReceiverThunk() {
	static const mt32emu_midi_receiver_i_v0 MIDI_RECEIVER_V0_THUNK = {
		getMidiReceiverVersionID,
		handleShortMessage,
		handleSysex,
		handleSystemRealtimeMessage
	};

	static const mt32emu_midi_receiver_i MIDI_RECEIVER_THUNK = { &MIDI_RECEIVER_V0_THUNK };

	return MIDI_RECEIVER_THUNK;
}

} // namespace CppInterfaceImpl

} // namespace MT32Emu

#if MT32EMU_API_TYPE == 2

#undef mt32emu_get_supported_report_handler_version
#undef mt32emu_get_supported_midi_receiver_version
#undef mt32emu_get_library_version_int
#undef mt32emu_get_library_version_string
#undef mt32emu_get_stereo_output_samplerate
#undef mt32emu_get_best_analog_output_mode
#undef mt32emu_get_machine_ids
#undef mt32emu_get_rom_ids
#undef mt32emu_identify_rom_data
#undef mt32emu_identify_rom_file
#undef mt32emu_create_context
#undef mt32emu_free_context
#undef mt32emu_add_rom_data
#undef mt32emu_add_rom_file
#undef mt32emu_merge_and_add_rom_data
#undef mt32emu_merge_and_add_rom_files
#undef mt32emu_add_machine_rom_file
#undef mt32emu_get_rom_info
#undef mt32emu_set_partial_count
#undef mt32emu_set_analog_output_mode
#undef mt32emu_set_stereo_output_samplerate
#undef mt32emu_set_samplerate_conversion_quality
#undef mt32emu_select_renderer_type
#undef mt32emu_get_selected_renderer_type
#undef mt32emu_open_synth
#undef mt32emu_close_synth
#undef mt32emu_is_open
#undef mt32emu_get_actual_stereo_output_samplerate
#undef mt32emu_convert_output_to_synth_timestamp
#undef mt32emu_convert_synth_to_output_timestamp
#undef mt32emu_flush_midi_queue
#undef mt32emu_set_midi_event_queue_size
#undef mt32emu_configure_midi_event_queue_sysex_storage
#undef mt32emu_set_midi_receiver
#undef mt32emu_get_internal_rendered_sample_count
#undef mt32emu_parse_stream
#undef mt32emu_parse_stream_at
#undef mt32emu_play_short_message
#undef mt32emu_play_short_message_at
#undef mt32emu_play_msg
#undef mt32emu_play_sysex
#undef mt32emu_play_msg_at
#undef mt32emu_play_sysex_at
#undef mt32emu_play_msg_now
#undef mt32emu_play_msg_on_part
#undef mt32emu_play_sysex_now
#undef mt32emu_write_sysex
#undef mt32emu_set_reverb_enabled
#undef mt32emu_is_reverb_enabled
#undef mt32emu_set_reverb_overridden
#undef mt32emu_is_reverb_overridden
#undef mt32emu_set_reverb_compatibility_mode
#undef mt32emu_is_mt32_reverb_compatibility_mode
#undef mt32emu_is_default_reverb_mt32_compatible
#undef mt32emu_preallocate_reverb_memory
#undef mt32emu_set_dac_input_mode
#undef mt32emu_get_dac_input_mode
#undef mt32emu_set_midi_delay_mode
#undef mt32emu_get_midi_delay_mode
#undef mt32emu_set_output_gain
#undef mt32emu_get_output_gain
#undef mt32emu_set_reverb_output_gain
#undef mt32emu_get_reverb_output_gain
#undef mt32emu_set_reversed_stereo_enabled
#undef mt32emu_is_reversed_stereo_enabled
#undef mt32emu_set_nice_amp_ramp_enabled
#undef mt32emu_is_nice_amp_ramp_enabled
#undef mt32emu_set_nice_panning_enabled
#undef mt32emu_is_nice_panning_enabled
#undef mt32emu_set_nice_partial_mixing_enabled
#undef mt32emu_is_nice_partial_mixing_enabled
#undef mt32emu_render_int16_t
#undef mt32emu_render_float
#undef mt32emu_render_int16_t_streams
#undef mt32emu_render_float_streams
#undef mt32emu_has_active_partials
#undef mt32emu_is_active
#undef mt32emu_get_partial_count
#undef mt32emu_get_part_states
#undef mt32emu_get_partial_states
#undef mt32emu_get_playing_notes
#undef mt32emu_get_patch_name
#undef mt32emu_read_memory

#endif // #if MT32EMU_API_TYPE == 2

#endif /* #ifndef MT32EMU_CPP_INTERFACE_H */
