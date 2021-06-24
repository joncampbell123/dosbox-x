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

#include <cstdio>
#include <cstring>

#include "internals.h"

#include "MidiStreamParser.h"
#include "Synth.h"

using namespace MT32Emu;

DefaultMidiStreamParser::DefaultMidiStreamParser(Synth &useSynth, uint32_t initialStreamBufferCapacity) :
	MidiStreamParser(initialStreamBufferCapacity), synth(useSynth), timestampSet(false) {}

void DefaultMidiStreamParser::setTimestamp(const uint32_t useTimestamp) {
	timestampSet = true;
	timestamp = useTimestamp;
}

void DefaultMidiStreamParser::resetTimestamp() {
	timestampSet = false;
}

void DefaultMidiStreamParser::handleShortMessage(const uint32_t message) {
	do {
		if (timestampSet) {
			if (synth.playMsg(message, timestamp)) return;
		}
		else {
			if (synth.playMsg(message)) return;
		}
	} while (synth.reportHandler->onMIDIQueueOverflow());
}

void DefaultMidiStreamParser::handleSysex(const uint8_t *stream, const uint32_t length) {
	do {
		if (timestampSet) {
			if (synth.playSysex(stream, length, timestamp)) return;
		}
		else {
			if (synth.playSysex(stream, length)) return;
		}
	} while (synth.reportHandler->onMIDIQueueOverflow());
}

void DefaultMidiStreamParser::handleSystemRealtimeMessage(const uint8_t realtime) {
	synth.reportHandler->onMIDISystemRealtime(realtime);
}

void DefaultMidiStreamParser::printDebug(const char *debugMessage) {
	synth.printDebug("%s", debugMessage);
}

MidiStreamParser::MidiStreamParser(uint32_t initialStreamBufferCapacity) :
	MidiStreamParserImpl(*this, *this, initialStreamBufferCapacity) {}

MidiStreamParserImpl::MidiStreamParserImpl(MidiReceiver &useReceiver, MidiReporter &useReporter, uint32_t initialStreamBufferCapacity) :
	midiReceiver(useReceiver), midiReporter(useReporter)
{
	if (initialStreamBufferCapacity < SYSEX_BUFFER_SIZE) initialStreamBufferCapacity = SYSEX_BUFFER_SIZE;
	if (MAX_STREAM_BUFFER_SIZE < initialStreamBufferCapacity) initialStreamBufferCapacity = MAX_STREAM_BUFFER_SIZE;
	streamBufferCapacity = initialStreamBufferCapacity;
	streamBuffer = new uint8_t[streamBufferCapacity];
	streamBufferSize = 0;
	runningStatus = 0;

	reserved = NULL;
}

MidiStreamParserImpl::~MidiStreamParserImpl() {
	delete[] streamBuffer;
}

void MidiStreamParserImpl::parseStream(const uint8_t *stream, uint32_t length) {
	while (length > 0) {
		uint32_t parsedMessageLength = 0;
		if (0xF8 <= *stream) {
			// Process System Realtime immediately and go on
			midiReceiver.handleSystemRealtimeMessage(*stream);
			parsedMessageLength = 1;
			// No effect on the running status
		} else if (streamBufferSize > 0) {
			// Check if there is something in streamBuffer waiting for being processed
			if (*streamBuffer == 0xF0) {
				parsedMessageLength = parseSysexFragment(stream, length);
			} else {
				parsedMessageLength = parseShortMessageDataBytes(stream, length);
			}
		} else {
			if (*stream == 0xF0) {
				runningStatus = 0; // SysEx clears the running status
				parsedMessageLength = parseSysex(stream, length);
			} else {
				parsedMessageLength = parseShortMessageStatus(stream);
			}
		}

		// Parsed successfully
		stream += parsedMessageLength;
		length -= parsedMessageLength;
	}
}

void MidiStreamParserImpl::processShortMessage(const uint32_t message) {
	// Adds running status to the MIDI message if it doesn't contain one
	uint8_t status = uint8_t(message & 0xFF);
	if (0xF8 <= status) {
		midiReceiver.handleSystemRealtimeMessage(status);
	} else if (processStatusByte(status)) {
		midiReceiver.handleShortMessage((message << 8) | status);
	} else if (0x80 <= status) { // If no running status available yet, skip this message
		midiReceiver.handleShortMessage(message);
	}
}

// We deal with SysEx messages below 512 bytes long in most cases. Nevertheless, it seems reasonable to support a possibility
// to load bulk dumps using a single message. However, this is known to fail with a real device due to limited input buffer size.
bool MidiStreamParserImpl::checkStreamBufferCapacity(const bool preserveContent) {
	if (streamBufferSize < streamBufferCapacity) return true;
	if (streamBufferCapacity < MAX_STREAM_BUFFER_SIZE) {
		uint8_t *oldStreamBuffer = streamBuffer;
		streamBufferCapacity = MAX_STREAM_BUFFER_SIZE;
		streamBuffer = new uint8_t[streamBufferCapacity];
		if (preserveContent) memcpy(streamBuffer, oldStreamBuffer, streamBufferSize);
		delete[] oldStreamBuffer;
		return true;
	}
	return false;
}

// Checks input byte whether it is a status byte. If not, replaces it with running status when available.
// Returns true if the input byte was changed to running status.
bool MidiStreamParserImpl::processStatusByte(uint8_t &status) {
	if (status < 0x80) {
		// First byte isn't status, try running status
		if (runningStatus < 0x80) {
			// No running status available yet
			midiReporter.printDebug("processStatusByte: No valid running status yet, MIDI message ignored");
			return false;
		}
		status = runningStatus;
		return true;
	} else if (status < 0xF0) {
		// Store current status as running for a Voice message
		runningStatus = status;
	} else if (status < 0xF8) {
		// System Common clears running status
		runningStatus = 0;
	} // System Realtime doesn't affect running status
	return false;
}

// Returns # of bytes parsed
uint32_t MidiStreamParserImpl::parseShortMessageStatus(const uint8_t stream[]) {
	uint8_t status = *stream;
	uint32_t parsedLength = processStatusByte(status) ? 0 : 1;
	if (0x80 <= status) { // If no running status available yet, skip one byte
		*streamBuffer = status;
		++streamBufferSize;
	}
	return parsedLength;
}

// Returns # of bytes parsed
uint32_t MidiStreamParserImpl::parseShortMessageDataBytes(const uint8_t stream[], uint32_t length) {
	const uint32_t shortMessageLength = Synth::getShortMessageLength(*streamBuffer);
	uint32_t parsedLength = 0;

	// Append incoming bytes to streamBuffer
	while ((streamBufferSize < shortMessageLength) && (length-- > 0)) {
		uint8_t dataByte = *(stream++);
		if (dataByte < 0x80) {
			// Add data byte to streamBuffer
			streamBuffer[streamBufferSize++] = dataByte;
		} else if (dataByte < 0xF8) {
			// Discard invalid bytes and start over
			char s[128];
			sprintf(s, "parseShortMessageDataBytes: Invalid short message: status %02x, expected length %i, actual %i -> ignored", *streamBuffer, shortMessageLength, streamBufferSize);
			midiReporter.printDebug(s);
			streamBufferSize = 0; // Clear streamBuffer
			return parsedLength;
		} else {
			// Bypass System Realtime message
			midiReceiver.handleSystemRealtimeMessage(dataByte);
		}
		++parsedLength;
	}
	if (streamBufferSize < shortMessageLength) return parsedLength; // Still lacks data bytes

	// Assemble short message
	uint32_t shortMessage = streamBuffer[0];
	for (uint32_t i = 1; i < shortMessageLength; ++i) {
		shortMessage |= streamBuffer[i] << (i << 3);
	}
	midiReceiver.handleShortMessage(shortMessage);
	streamBufferSize = 0; // Clear streamBuffer
	return parsedLength;
}

// Returns # of bytes parsed
uint32_t MidiStreamParserImpl::parseSysex(const uint8_t stream[], const uint32_t length) {
	// Find SysEx length
	uint32_t sysexLength = 1;
	while (sysexLength < length) {
		uint8_t nextByte = stream[sysexLength++];
		if (0x80 <= nextByte) {
			if (nextByte == 0xF7) {
				// End of SysEx
				midiReceiver.handleSysex(stream, sysexLength);
				return sysexLength;
			}
			if (0xF8 <= nextByte) {
				// The System Realtime message must be processed right after return
				// but the SysEx is actually fragmented and to be reconstructed in streamBuffer
				--sysexLength;
				break;
			}
			// Illegal status byte in SysEx message, aborting
			midiReporter.printDebug("parseSysex: SysEx message lacks end-of-sysex (0xf7), ignored");
			// Continue parsing from that point
			return sysexLength - 1;
		}
	}

	// Store incomplete SysEx message for further processing
	streamBufferSize = sysexLength;
	if (checkStreamBufferCapacity(false)) {
		memcpy(streamBuffer, stream, sysexLength);
	} else {
		// Not enough buffer capacity, don't care about the real buffer content, just mark the first byte
		*streamBuffer = *stream;
		streamBufferSize = streamBufferCapacity;
	}
	return sysexLength;
}

// Returns # of bytes parsed
uint32_t MidiStreamParserImpl::parseSysexFragment(const uint8_t stream[], const uint32_t length) {
	uint32_t parsedLength = 0;
	while (parsedLength < length) {
		uint8_t nextByte = stream[parsedLength++];
		if (nextByte < 0x80) {
			// Add SysEx data byte to streamBuffer
			if (checkStreamBufferCapacity(true)) streamBuffer[streamBufferSize++] = nextByte;
			continue;
		}
		if (0xF8 <= nextByte) {
			// Bypass System Realtime message
			midiReceiver.handleSystemRealtimeMessage(nextByte);
			continue;
		}
		if (nextByte != 0xF7) {
			// Illegal status byte in SysEx message, aborting
			midiReporter.printDebug("parseSysexFragment: SysEx message lacks end-of-sysex (0xf7), ignored");
			// Clear streamBuffer and continue parsing from that point
			streamBufferSize = 0;
			--parsedLength;
			break;
		}
		// End of SysEx
		if (checkStreamBufferCapacity(true)) {
			streamBuffer[streamBufferSize++] = nextByte;
			midiReceiver.handleSysex(streamBuffer, streamBufferSize);
			streamBufferSize = 0; // Clear streamBuffer
			break;
		}
		// Encountered streamBuffer overrun
		midiReporter.printDebug("parseSysexFragment: streamBuffer overrun while receiving SysEx message, ignored. Max allowed size of fragmented SysEx is 32768 bytes.");
		streamBufferSize = 0; // Clear streamBuffer
		break;
	}
	return parsedLength;
}
