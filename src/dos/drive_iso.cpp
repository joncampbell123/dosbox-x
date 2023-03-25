/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <cctype>
#include <cstring>
#include <assert.h>
#if defined(WIN32)
#include <winsock2.h>
#endif
#include "cdrom.h"
#include "dosbox.h"
#include "byteorder.h"
#include "dos_system.h"
#include "logging.h"
#include "support.h"
#include "drives.h"

#define FLAGS1	((iso) ? de.fileFlags : de.timeZone)
#define FLAGS2	((iso) ? de->fileFlags : de->timeZone)

char fullname[LFN_NAMELENGTH];
static uint16_t sdid[256];
extern int lfn_filefind_handle;
extern bool gbk, isDBCSCP(), isKanji1_gbk(uint8_t chr), shiftjis_lead_byte(int c);
extern bool filename_not_8x3(const char *n), filename_not_strict_8x3(const char *n);
extern bool CodePageHostToGuestUTF16(char *d/*CROSS_LEN*/,const uint16_t *s/*CROSS_LEN*/);

using namespace std;

static bool islfnchar(const char *s) {
    return (unsigned char)*s <= 32 || (unsigned char)*s == 127 || *s == '\"' || *s == '+' || *s == '=' || *s == ',' || *s == ';' || *s == ':' || *s == '<' || *s == '>' || *s == '?' || *s == '*';
}

////////////////////////////////////

/*From Linux kernel source*/
/** CRC table for the CRC ITU-T V.41 0x1021 (x^16 + x^12 + x^15 + 1) */
const uint16_t UDF_crc_itu_t_table[256] = {
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

uint16_t UDF_crc_itu_t(uint16_t crc, const uint8_t *buffer, size_t len)
{
	while (len--)
		crc = UDF_crc_itu_t_byte(crc, *buffer++);

	return crc;
}

////////////////////////////////////

void UDFTagId::parse(const unsigned int sz,const unsigned char *b) {
	if (sz >= 16) {
		TagIdentifier = le16toh( *((uint16_t*)(&b[0])) );
		DescriptorVersion = le16toh( *((uint16_t*)(&b[2])) );
		TagChecksum = b[4];
		Reserved = b[5];
		TagSerialNumber = le16toh( *((uint16_t*)(&b[6])) );
		DescriptorCRC = le16toh( *((uint16_t*)(&b[8])) );
		DescriptorCRCLength = le16toh( *((uint16_t*)(&b[10])) );
		TagLocation = le32toh( *((uint32_t*)(&b[12])) );
	}
	else {
		TagIdentifier = 0;
		TagChecksum = 0;
		Reserved = 0;
		TagLocation = 0;
	}
}

bool UDFTagId::tagChecksumOK(const unsigned int sz,const unsigned char *b) {
	uint8_t chksum = 0;

	if (sz < 16)
		return false;

	for (unsigned int i=0;i <= 3;i++)
		chksum += b[i];
	for (unsigned int i=5;i <= 15;i++)
		chksum += b[i];

	if (chksum != TagChecksum)
		return false;

	return true;
}

bool UDFTagId::dataChecksumOK(const unsigned int sz,const unsigned char *b) {
	if (DescriptorCRCLength != 0) {
		if ((DescriptorCRCLength+16u) > sz)
			return false;
		if (DescriptorCRC != UDF_crc_itu_t(0,b+16,DescriptorCRCLength))
			return false;
	}

	return true;
}

bool UDFTagId::get(const unsigned int sz,const unsigned char *b) {
	parse(sz,b);

	if (TagLocation == 0 && TagIdentifier == 0)
		return false;

	return tagChecksumOK(sz,b) && dataChecksumOK(sz,b);
}

UDFTagId::UDFTagId(const unsigned int sz,const unsigned char *b) {
	parse(sz,b);
}

UDFTagId::UDFTagId() {
}

////////////////////////////////////

void UDFextent_ad::get(const unsigned int sz,const unsigned char *b) {
	if (sz >= 8u) {
		ExtentLength = le32toh( *((uint32_t*)(&b[0])) );
		ExtentLocation = le32toh( *((uint32_t*)(&b[4])) );
	}
}

UDFextent_ad::UDFextent_ad(const unsigned int sz,const unsigned char *b) {
	get(sz,b);
}

UDFextent_ad::UDFextent_ad() {
}

////////////////////////////////////

void UDFAnchorVolumeDescriptorPointer::get(UDFTagId &tag,const unsigned int sz,const unsigned char *b) {
	if (sz >= 32u) {
		DescriptorTag = tag;
		MainVolumeDescriptorSequenceExtent.get(8,b+16);
		ReserveVolumeDescriptorSequenceExtent.get(8,b+24);
	}
}

UDFAnchorVolumeDescriptorPointer::UDFAnchorVolumeDescriptorPointer(UDFTagId &tag,const unsigned int sz,const unsigned char *b) {
	get(tag,sz,b);
}

UDFAnchorVolumeDescriptorPointer::UDFAnchorVolumeDescriptorPointer() {
}

////////////////////////////////////

UDFextent_ad isoDrive::convertToUDFextent_ad(const UDFshort_ad &s,const uint32_t partition_ref_id) const {
	UDFextent_ad r;

	if (!convertToUDFextent_ad(r,s,partition_ref_id))
		r.ExtentLocation = r.ExtentLength = 0;

	return r;
}

UDFextent_ad isoDrive::convertToUDFextent_ad(const UDFlong_ad &s) const {
	UDFextent_ad r;

	if (!convertToUDFextent_ad(r,s))
		r.ExtentLocation = r.ExtentLength = 0;

	return r;
}

UDFextent_ad isoDrive::convertToUDFextent_ad(const UDFext_ad &s) const {
	UDFextent_ad r;

	if (!convertToUDFextent_ad(r,s))
		r.ExtentLocation = r.ExtentLength = 0;

	return r;
}

bool isoDrive::convertToUDFextent_ad(UDFextent_ad &d,const UDFextent_ad &s) const {
	d = s;
	return true;
}

bool isoDrive::convertToUDFextent_ad(UDFextent_ad &d,const UDFshort_ad &s,const uint32_t partition_ref_id) const {
	if (partd.DescriptorTag.TagIdentifier != 0 && (partition_ref_id == partd.PartitionNumber || partition_ref_id == 0xFFFFFFFFu)) {
		d.ExtentLocation = s.ExtentPosition + partd.PartitionStartingLocation;
		d.ExtentLength = s.ExtentLength;
		return true;
	}

	return false;
}

bool isoDrive::convertToUDFextent_ad(UDFextent_ad &d,const UDFlong_ad &s) const {
	if (partd.DescriptorTag.TagIdentifier != 0 && s.ExtentLocation.PartitionReferenceNumber == partd.PartitionNumber) {
		d.ExtentLocation = s.ExtentLocation.LogicalBlockNumber + partd.PartitionStartingLocation;
		d.ExtentLength = s.ExtentLength;
		return true;
	}

	return false;
}

bool isoDrive::convertToUDFextent_ad(UDFextent_ad &d,const UDFext_ad &s) const {
	if (partd.DescriptorTag.TagIdentifier != 0 && s.ExtentLocation.PartitionReferenceNumber == partd.PartitionNumber) {
		d.ExtentLocation = s.ExtentLocation.LogicalBlockNumber + partd.PartitionStartingLocation;
		d.ExtentLength = s.ExtentLength;
		return true;
	}

	return false;
}

////////////////////////////////////

void UDFPrimaryVolumeDescriptor::get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b) {
	if (sz >= 490) {
		DescriptorTag = tag;
		VolumeDescriptorSequenceNumber = le32toh( *((uint32_t*)(&b[16])) );
		PrimaryVolumeDescriptorNumber = le32toh( *((uint32_t*)(&b[20])) );
		VolumeIdentifier.get(32,b+24);
		VolumeSequenceNumber = le16toh( *((uint16_t*)(&b[56])) );
		MaximumVolumeSequenceNumber = le16toh( *((uint16_t*)(&b[58])) );
		InterchangeLevel = le16toh( *((uint16_t*)(&b[60])) );
		MaximumInterchangeLevel = le16toh( *((uint16_t*)(&b[62])) );
		CharacterSetList = le32toh( *((uint32_t*)(&b[64])) );
		MaximumCharacterSetList = le32toh( *((uint32_t*)(&b[68])) );
		VolumeSetIdentifier.get(128,b+72);
		DescriptorCharacterSet.get(64,b+200);
		ExplanatoryCharacterSet.get(64,b+264);
		VolumeAbstract.get(8,b+328);
		VolumeCopyrightNotice.get(8,b+336);
		ApplicationIdentifier.get(32,b+344);
		RecordingDateAndTime.get(12,b+376);
		ImplementationIdentifier.get(32,b+388);

		static_assert(sizeof(ImplementationUse) == 64,"sizeof err");
		memcpy(&ImplementationUse,b+420,64);

		PredecessorVolumeDescriptorSequenceLocation = le32toh( *((uint32_t*)(&b[484])) );
		Flags = le16toh( *((uint16_t*)(&b[488])) );
	}
}

UDFPrimaryVolumeDescriptor::UDFPrimaryVolumeDescriptor(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b) {
	get(tag,sz,b);
}

UDFPrimaryVolumeDescriptor::UDFPrimaryVolumeDescriptor() {
}

////////////////////////////////////////////////

void UDFFileSetDescriptor::get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b) {
	if (sz >= 480) {
		DescriptorTag = tag;
		RecordingDateAndType.get(12,b+16);
		InterchangeLevel = le16toh( *((uint16_t*)(&b[28])) );
		MaximumInterchangeLevel = le16toh( *((uint16_t*)(&b[30])) );
		CharacterSetList = le32toh( *((uint32_t*)(&b[32])) );
		MaximumCharacterSetList = le32toh( *((uint32_t*)(&b[36])) );
		FileSetNumber = le32toh( *((uint32_t*)(&b[40])) );
		FileSetDescriptorNumber = le32toh( *((uint32_t*)(&b[44])) );
		LogicalVolumeIdentifierCharacterSet.get(64,b+48);
		LogicalVolumeIdentifier.get(128,b+112);
		FileSetCharacterSet.get(64,b+240);
		FileSetIdentifier.get(32,b+304);
		CopyrightFileIdentifier.get(32,b+336);
		AbstractFileIdentifier.get(32,b+368);
		RootDirectoryICB.get(16,b+400);
		DomainIdentifier.get(32,b+416);
		NextExtent.get(16,b+448);
		SystemStreamDirectoryICB.get(16,b+464);
	}
}

UDFFileSetDescriptor::UDFFileSetDescriptor(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b) {
	get(tag,sz,b);
}

UDFFileSetDescriptor::UDFFileSetDescriptor() {
}

////////////////////////////////////////////////////////

void UDFext_ad::get(const unsigned int sz,const unsigned char *b) {
	if (sz >= 20) {
		ExtentLength = le32toh( *((uint32_t*)(&b[0])) );
		RecordedLength = le32toh( *((uint32_t*)(&b[4])) );
		InformationLength = le32toh( *((uint32_t*)(&b[8])) );
		ExtentLocation.get(6,b+12);

		static_assert(sizeof(ImplementationUse) == 2,"sizeof err");
		memcpy(ImplementationUse,b+18,2);
	}
}

UDFext_ad::UDFext_ad(const unsigned int sz,const unsigned char *b) {
	get(sz,b);
}

UDFext_ad::UDFext_ad() {
}

////////////////////////////////////////////////////////

uint8_t UDFicbtag::AllocationDescriptorType(void) const {
	return (uint8_t)(Flags & 7u);
}

void UDFicbtag::get(const unsigned int sz,const unsigned char *b) {
	if (sz >= 20) {
		PriorRecordedNumberOfDirectEntries = le32toh( *((uint32_t*)(&b[0])) );
		StrategyType = le16toh( *((uint16_t*)(&b[4])) );

		static_assert(sizeof(StrategyParameter) == 2,"sizeof err");
		memcpy(StrategyParameter,b+6,2);

		MaximumNumberOfEntries = le16toh( *((uint16_t*)(&b[8])) );
		Reserved = b[10];
		FileType = b[11];
		ParentICBLocation.get(6,b+12);
		Flags = le16toh( *((uint16_t*)(&b[18])) );
	}
}

UDFicbtag::UDFicbtag(const unsigned int sz,const unsigned char *b) {
	get(sz,b);
}

UDFicbtag::UDFicbtag() {
}

////////////////////////////////////////////////////////

void UDFFileEntry::get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b) {
	if (sz >= 176) {
		DescriptorTag = tag;
		ICBTag.get(20,b+16);
		Uid = le32toh( *((uint32_t*)(&b[36])) );
		Gid = le32toh( *((uint32_t*)(&b[40])) );
		Permissions = le32toh( *((uint32_t*)(&b[44])) );
		FileLinkCount = le16toh( *((uint16_t*)(&b[48])) );
		RecordFormat = b[50];
		RecordDisplayAttributes = b[51];
		RecordLength = le32toh( *((uint32_t*)(&b[52])) );
		InformationLength = le64toh( *((uint64_t*)(&b[56])) );
		LogicalBlocksRecorded = le64toh( *((uint64_t*)(&b[64])) );
		AccessDateAndTime.get(12,b+72);
		ModificationDateAndTime.get(12,b+84);
		AttributeDateAndTime.get(12,b+96);
		Checkpoint = le32toh( *((uint32_t*)(&b[108])) );
		ExtendedAttributeICB.get(16,b+112);
		ImplementationIdentifier.get(32,b+128);
		UniqueId = le64toh( *((uint64_t*)(&b[160])) );
		LengthOfExtendedAttributes = le32toh( *((uint32_t*)(&b[168])) );
		LengthOfAllocationDescriptors = le32toh( *((uint32_t*)(&b[172])) );

		const size_t allo = 176u + LengthOfExtendedAttributes;

		switch (ICBTag.AllocationDescriptorType()) {
			case 0: // short_ad
				for (unsigned int i=0;(i+8u) <= LengthOfAllocationDescriptors;i += 8) {
					const size_t si = i + allo;
					UDFshort_ad sad;

					if ((si+8u) > sz) break;

					sad.get(8,&b[si]);
					AllocationDescriptors_short_ad.push_back(sad);
				}
				break;
			case 1: // long_ad
				for (unsigned int i=0;(i+16u) <= LengthOfAllocationDescriptors;i += 16) {
					const size_t si = i + allo;
					UDFlong_ad sad;

					if ((si+16u) > sz) break;

					sad.get(16,&b[si]);
					AllocationDescriptors_long_ad.push_back(sad);
				}
				break;
			case 2: // ext_ad
				for (unsigned int i=0;(i+20u) <= LengthOfAllocationDescriptors;i += 20) {
					const size_t si = i + allo;
					UDFext_ad sad;

					if ((si+20u) > sz) break;

					sad.get(20,&b[si]);
					AllocationDescriptors_ext_ad.push_back(sad);
				}
				break;
			case 3: // The file itself resides in the allocation descriptor region
				if (allo < sz) {
					size_t cpy = std::min(sz - allo,(size_t)LengthOfAllocationDescriptors);
					if (cpy != 0) {
						assert((allo+cpy) <= sz);
						AllocationDescriptors_file.resize(cpy);
						memcpy(&AllocationDescriptors_file[0],&b[allo],cpy);
					}
				}
				break;
		}
	}
}

UDFFileEntry::UDFFileEntry(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b) {
	get(tag,sz,b);
}

UDFFileEntry::UDFFileEntry() {
}

////////////////////////////////////////////////////////

void UDFFileIdentifierDescriptor::get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b) {
	if (sz >= 38) {
		DescriptorTag = tag;
		FileVersionNumber = le16toh( *((uint16_t*)(&b[16])) );
		FileCharacteristics = b[18];
		LengthOfFileIdentifier = b[19];
		ICB.get(16,b+20);
		LengthOfImplementationUse = le16toh( *((uint16_t*)(&b[36])) );

		size_t ofs = 38;

		if ((ofs+LengthOfImplementationUse) <= sz && LengthOfImplementationUse != 0) {
			ImplementationUse.resize(LengthOfImplementationUse);
			memcpy(&ImplementationUse[0],&b[ofs],LengthOfImplementationUse);
			ofs += LengthOfImplementationUse;
		}
		if ((ofs+LengthOfFileIdentifier) <= sz && LengthOfFileIdentifier != 0) {
			FileIdentifier.resize(LengthOfFileIdentifier);
			memcpy(&FileIdentifier[0],&b[ofs],LengthOfFileIdentifier);
			ofs += LengthOfFileIdentifier;
		}
	}
}

UDFFileIdentifierDescriptor::UDFFileIdentifierDescriptor(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b) {
	get(tag,sz,b);
}

UDFFileIdentifierDescriptor::UDFFileIdentifierDescriptor() {
}

////////////////////////////////////

void UDFPartitionDescriptor::get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b) {
	if (sz >= 356) {
		DescriptorTag = tag;
		VolumeDescriptorSequenceNumber = le32toh( *((uint32_t*)(&b[16])) );
		PartitionFlags = le16toh( *((uint16_t*)(&b[20])) );
		PartitionNumber = le16toh( *((uint16_t*)(&b[22])) );
		PartitionContents.get(32,b+24);

		static_assert(sizeof(PartitionContentsUse) == 128,"sizeof err");
		memcpy(PartitionContentsUse,b+56,128);

		AccessType = le32toh( *((uint32_t*)(&b[184])) );
		PartitionStartingLocation = le32toh( *((uint32_t*)(&b[188])) );
		PartitionLength = le32toh( *((uint32_t*)(&b[192])) );
		ImplementationIdentifier.get(32,b+196);

		static_assert(sizeof(ImplementationUse) == 128,"sizeof err");
		memcpy(ImplementationUse,b+228,128);
	}
}

UDFPartitionDescriptor::UDFPartitionDescriptor(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b) {
	get(tag,sz,b);
}

UDFPartitionDescriptor::UDFPartitionDescriptor() {
}

////////////////////////////////////////////////////////

void UDFLogicalVolumeDescriptor::get(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b) {
	if (sz >= 440) {
		DescriptorTag = tag;
		VolumeDescriptorSequenceNumber = le32toh( *((uint32_t*)(&b[16])) );
		DescriptorCharacterSet.get(64,b+20);
		LogicalVolumeIdentifier.get(128,b+84);
		LogicalBlockSize = le32toh( *((uint32_t*)(&b[212])) );
		DomainIdentifier.get(32,b+216);

		static_assert(sizeof(LogicalVolumeContentsUse) == 16,"sizeof err");
		memcpy(LogicalVolumeContentsUse,b+248,16);

		MapTableLength = le32toh( *((uint32_t*)(&b[264])) );
		NumberOfPartitionMaps = le32toh( *((uint32_t*)(&b[268])) );
		ImplementationIdentifier.get(32,b+272);

		static_assert(sizeof(ImplementationUse) == 128,"sizeof err");
		memcpy(ImplementationUse,b+304,128);
		IntegritySequenceExtent.get(8,b+432);

		if (MapTableLength != 0 && (440+MapTableLength) <= sz) {
			PartitionMaps.resize(MapTableLength);
			memcpy(&PartitionMaps[0],&b[440],MapTableLength);
		}
	}
}

UDFLogicalVolumeDescriptor::UDFLogicalVolumeDescriptor(UDFTagId &tag/*already parsed, why parse again?*/,const unsigned int sz,const unsigned char *b) {
	get(tag,sz,b);
}

UDFLogicalVolumeDescriptor::UDFLogicalVolumeDescriptor() {
}

////////////////////////////////////

// FIXME: Why do the "strings" start with a control code like \x08?
void UDFdstring::get(const unsigned int sz,const unsigned char *b) {
	/* the LAST byte is the string length (?) */
	if (sz >= 2) {
		unsigned int maxsz = sz-1u;
		uint8_t len = b[maxsz];

		if (len > maxsz) len = maxsz;

		resize(len);
		if (len != 0) memcpy(&(operator[](0)),b,len);
	}
}

UDFdstring::UDFdstring(const unsigned int sz,const unsigned char *b) {
	get(sz,b);
}

UDFdstring::UDFdstring() {
}

////////////////////////////////////

void UDFtimestamp::get(const unsigned int sz,const unsigned char *b) {
	if (sz >= 12u) {
		TypeAndTimeZone = le16toh( *((uint16_t*)(&b[0])) );
		Year = (int16_t)le16toh( *((uint16_t*)(&b[2])) );
		Month = b[4];
		Day = b[5];
		Hour = b[6];
		Minute = b[7];
		Second = b[8];
		Centiseconds = b[9];
		HundredsOfMicroseconds = b[10];
		Microseconds = b[11];
	}
}

UDFtimestamp::UDFtimestamp(const unsigned int sz,const unsigned char *b) {
	get(sz,b);
}

UDFtimestamp::UDFtimestamp() {
}

////////////////////////////////////////////////////////

void UDFregid::get(const unsigned int sz,const unsigned char *b) {
	if (sz >= 32u) {
		Flags = b[0];
		memcpy(Identifier,b+1,23);
		memcpy(IdentifierSuffix,b+24,8);
	}
}

UDFregid::UDFregid(const unsigned int sz,const unsigned char *b) {
	get(sz,b);
}

UDFregid::UDFregid() {
}

////////////////////////////////////////////////////////

void UDFcharspec::get(const unsigned int sz,const unsigned char *b) {
	if (sz >= 64u) {
		CharacterSetType = b[0];
		memcpy(CharacterSetInformation,b+1,63);
	}
}

UDFcharspec::UDFcharspec(const unsigned int sz,const unsigned char *b) {
	get(sz,b);
}

UDFcharspec::UDFcharspec() {
}

////////////////////////////////////////////////////////

void UDFlb_addr::get(const unsigned int sz,const unsigned char *b) {
	if (sz >= 6u) {
		LogicalBlockNumber = le32toh( *((uint32_t*)(&b[0])) );
		PartitionReferenceNumber = le16toh( *((uint16_t*)(&b[4])) );
	}
}

UDFlb_addr::UDFlb_addr(const unsigned int sz,const unsigned char *b) {
	get(sz,b);
}

UDFlb_addr::UDFlb_addr() {
}

////////////////////////////////////////////////////////

void UDFshort_ad::get(const unsigned int sz,const unsigned char *b) {
	if (sz >= 8u) {
		ExtentLength = le32toh( *((uint32_t*)(&b[0])) );
		ExtentPosition = le32toh( *((uint32_t*)(&b[4])) );
	}
}

UDFshort_ad::UDFshort_ad(const unsigned int sz,const unsigned char *b) {
	get(sz,b);
}

UDFshort_ad::UDFshort_ad() {
}

////////////////////////////////////////////////////////

void UDFlong_ad::get(const unsigned int sz,const unsigned char *b) {
	if (sz >= 16) {
		ExtentLength = le32toh( *((uint32_t*)(&b[0])) );
		ExtentLocation.get(6,b+4);

		static_assert(sizeof(ImplementationUse) == 6,"sizeof err");
		memcpy(ImplementationUse,b+10,6);
	}
	else {
		ExtentLength = 0;
	}
}

UDFlong_ad::UDFlong_ad(const unsigned int sz,const unsigned char *b) {
	get(sz,b);
}

UDFlong_ad::UDFlong_ad() {
}

////////////////////////////////////

UDFextent::UDFextent() {
}

UDFextent::UDFextent(const struct UDFextent_ad &s) {
	ex = s;
}

////////////////////////////////////

UDFextents::UDFextents() {
}

UDFextents::UDFextents(const struct UDFextent_ad &s) {
	xl.push_back(s);
}

////////////////////////////////////

void isoDrive::UDFFileEntryToExtents(UDFextents &ex,UDFFileEntry &fe) {
	ex.xl.clear();
	ex.indata.clear();
	ex.is_indata = false;
	switch (fe.ICBTag.AllocationDescriptorType()) {
                case 0: /* short_ad */
                        for (auto ad : fe.AllocationDescriptors_short_ad)
                                ex.xl.push_back(convertToUDFextent_ad(ad,0xFFFFFFFFu));
                        break;
                case 1: /* long_ad */
                        for (auto ad : fe.AllocationDescriptors_long_ad)
                                ex.xl.push_back(convertToUDFextent_ad(ad));
                        break;
                case 2: /* ext_ad */
                        for (auto ad : fe.AllocationDescriptors_ext_ad)
                                ex.xl.push_back(convertToUDFextent_ad(ad));
                        break;
                case 3: /* file replaces allocation descriptors (very small files) */
                        ex.indata = fe.AllocationDescriptors_file;
                        ex.is_indata = true;
                        break;
	}

	UDFextent_rewind(ex);
}

////////////////////////////////////

void isoDrive::UDFextent_rewind(struct UDFextents &ex) {
	ex.relofs = 0;
	ex.extent = 0;
	ex.extofs = 0;
	ex.filesz = 0;
}

uint64_t isoDrive::UDFtotalsize(struct UDFextents &ex) const {
        uint64_t total = 0;

        for (size_t i=0;i < ex.xl.size();i++)
                total += (uint64_t)ex.xl[i].ex.ExtentLength;

        return total;
}

uint64_t isoDrive::UDFextent_seek(struct UDFextents &ex,uint64_t ofs) {
	UDFextent_rewind(ex);

	if (ex.is_indata) {
		if (ofs > (uint64_t)ex.indata.size())
			ofs = (uint64_t)ex.indata.size();

		ex.relofs = (uint32_t)ofs; // NTS: Assume no sane authoring program would put 4GB of data in extent area!
	}
	else {
		while (ex.extent < ex.xl.size()) {
			const auto &exs = ex.xl[ex.extent];

			assert(ofs >= ex.extofs);
			ex.relofs = ofs - ex.extofs;

			if (ex.relofs < (uint64_t)exs.ex.ExtentLength)
				break;

			ex.extofs += (uint64_t)exs.ex.ExtentLength;
			ex.relofs = 0;
			ex.extent++;
		}
	}

	return (uint64_t)ex.extofs + (uint64_t)ex.relofs;
}

int isoDrive::UDFextent_read(struct UDFextents &ex,unsigned char *buf,size_t count) {
	int rd = 0;

	if (ex.is_indata) {
		assert(ex.relofs <= (uint32_t)ex.indata.size());
		size_t rem = ex.indata.size() - size_t(ex.relofs);
		size_t todo = std::min(rem,count);

		if (todo != 0) {
			memcpy(buf,&ex.indata[ex.relofs],todo);
			ex.relofs += todo;
			count -= todo;
			buf += todo;
			rd += todo;
		}
	}
	else {
		while (count != size_t(0)) {
			if (ex.extent >= ex.xl.size()) break;

			const auto &exs = ex.xl[ex.extent];

			assert(ex.relofs <= exs.ex.ExtentLength);
			size_t extodo = (size_t)std::min((uint64_t)(exs.ex.ExtentLength - ex.relofs),(uint64_t)count);
			size_t sctofs = (size_t)(ex.relofs % (uint32_t)COOKED_SECTOR_SIZE);
			size_t sctodo = std::min(extodo,COOKED_SECTOR_SIZE - sctofs);
			uint32_t sector = exs.ex.ExtentLocation + ((ex.relofs-sctofs) / (uint32_t)COOKED_SECTOR_SIZE);

			if (sctodo != size_t(0)) {
				if (ex.sector_buffer_n != sector) {
					ex.sector_buffer_n = 0xFFFFFFFFu;
					ex.sector_buffer.resize(COOKED_SECTOR_SIZE);
					if (readSector(&ex.sector_buffer[0], (unsigned int)sector))
						ex.sector_buffer_n = sector;
					else
						break;
				}

				memcpy(buf,&ex.sector_buffer[sctofs],sctodo);
				ex.relofs += sctodo;
				count -= sctodo;
				buf += sctodo;
				rd += sctodo;
			}

			assert(ex.relofs <= exs.ex.ExtentLength);
			if (ex.relofs == exs.ex.ExtentLength) {
				ex.extofs += (uint64_t)exs.ex.ExtentLength;
				ex.relofs = 0;
				ex.extent++;
			}
		}
	}

	return rd;
}

////////////////////////////////////

class isoFile : public DOS_File {
public:
    isoFile(isoDrive* drive, const char* name, const FileStat_Block* stat, uint32_t offset);
	bool Read(uint8_t *data, uint16_t *size);
	bool Write(const uint8_t *data, uint16_t *size);
	bool Seek(uint32_t *pos, uint32_t type);
	bool Close();
	uint16_t GetInformation(void);
	uint32_t GetSeekPos(void);
public:
	UDFextents udffext;
	bool udf = false;
private:
	isoDrive *drive;
    uint8_t buffer[ISO_FRAMESIZE] = {};
    int cachedSector = -1;
	uint32_t fileBegin;
	uint32_t filePos;
	uint32_t fileEnd;
//	uint16_t info;
};

isoFile::isoFile(isoDrive* drive, const char* name, const FileStat_Block* stat, uint32_t offset) : drive(drive), fileBegin(offset) {
	time = stat->time;
	date = stat->date;
	attr = stat->attr;
	filePos = fileBegin;
	fileEnd = fileBegin + stat->size;
	open = true;
	this->name = NULL;
	SetName(name);
}

bool isoFile::Read(uint8_t *data, uint16_t *size) {
	if (udf) {
		*size = (uint16_t)(drive->UDFextent_read(udffext,data,*size));
		filePos = *size;
		return true;
	}

	if (filePos + *size > fileEnd)
		*size = (uint16_t)(fileEnd - filePos);
	
	uint16_t nowSize = 0;
	int sector = (int)(filePos / ISO_FRAMESIZE);
	uint16_t sectorPos = (uint16_t)(filePos % ISO_FRAMESIZE);
	
	if (sector != cachedSector) {
		if (drive->readSector(buffer, (unsigned int)sector)) cachedSector = sector;
		else { *size = 0; cachedSector = -1; }
	}
	while (nowSize < *size) {
		uint16_t remSector = ISO_FRAMESIZE - sectorPos;
		uint16_t remSize = *size - nowSize;
		if(remSector < remSize) {
			memcpy(&data[nowSize], &buffer[sectorPos], remSector);
			nowSize += remSector;
			sectorPos = 0;
			sector++;
			cachedSector++;
			if (!drive->readSector(buffer, (unsigned int)sector)) {
				*size = nowSize;
				cachedSector = -1;
			}
		} else {
			memcpy(&data[nowSize], &buffer[sectorPos], remSize);
			nowSize += remSize;
		}
			
	}
	
	*size = nowSize;
	filePos += *size;
	return true;
}

bool isoFile::Write(const uint8_t* /*data*/, uint16_t* /*size*/) {
	return false;
}

bool isoFile::Seek(uint32_t *pos, uint32_t type) {
	switch (type) {
		case DOS_SEEK_SET:
			filePos = fileBegin + *pos;
			break;
		case DOS_SEEK_CUR:
			filePos += *pos;
			break;
		case DOS_SEEK_END:
			filePos = fileEnd + *pos;
			break;
		default:
			return false;
	}
	if (filePos > fileEnd || filePos < fileBegin)
		filePos = fileEnd;
	
	*pos = filePos - fileBegin;

	if (udf) {
		*pos = drive->UDFextent_seek(udffext,*pos);
		filePos = *pos + fileBegin;
	}

	return true;
}

bool isoFile::Close() {
	if (refCtr == 1) open = false;
	return true;
}

uint16_t isoFile::GetInformation(void) {
	return 0x40;		// read-only drive
}

uint32_t isoFile::GetSeekPos() {
	return filePos - fileBegin;
}

int   MSCDEX_RemoveDrive(char driveLetter);
int   MSCDEX_AddDrive(char driveLetter, const char* physicalPath, uint8_t& subUnit);
void  MSCDEX_ReplaceDrive(CDROM_Interface* cdrom, uint8_t subUnit);
bool  MSCDEX_HasDrive(char driveLetter);
bool  MSCDEX_GetVolumeName(uint8_t subUnit, char* name);
uint8_t MSCDEX_GetSubUnit(char driveLetter);

bool CDROM_Interface_Image::images_init = false;

isoDrive::isoDrive(char driveLetter, const char* fileName, uint8_t mediaid, int& error, std::vector<std::string>& options) {
    enable_udf = (dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10));//default
    enable_rock_ridge = (dos.version.major >= 7 || uselfn);//default
    enable_joliet = (dos.version.major >= 7 || uselfn);//default
    is_rock_ridge = false;
    is_joliet = false;
    is_udf = false;
    for (const auto &opt : options) {
        size_t equ = opt.find_first_of('=');
        std::string name,value;

        if (equ != std::string::npos) {
            name = opt.substr(0,equ);
            value = opt.substr(equ+1);
        }
        else {
            name = opt;
            value.clear();
        }

        if (name == "rr") { // Enable/disable Rock Ridge extensions
            if (value == "1" || value == "") enable_rock_ridge = true; // "-o rr" or "-o rr=1"
            else if (value == "0") enable_rock_ridge = false;
        }
        else if (name == "joliet") { // Enable/disable Joliet extensions
            if (value == "1" || value == "") enable_joliet = true; // "-o joliet" or "-o joliet=1"
            else if (value == "0") enable_joliet = false;
	}
        else if (name == "udf") { // Enable/disable UDF
            if (value == "1" || value == "") enable_udf = true; // "-o udf" or "-o udf=1"
            else if (value == "0") enable_udf = false;
	}
    }

    if (!CDROM_Interface_Image::images_init) {
        CDROM_Interface_Image::images_init = true;
        for (size_t i=0;i < 26;i++)
            CDROM_Interface_Image::images[i] = NULL;
    }

	this->fileName[0]  = '\0';
	this->discLabel[0] = '\0';
	subUnit = 0;
	nextFreeDirIterator = 0;
	memset(sectorHashEntries, 0, sizeof(sectorHashEntries));
	memset(&rootEntry, 0, sizeof(isoDirEntry));
	
	safe_strncpy(this->fileName, fileName, CROSS_LEN);
	error = UpdateMscdex(driveLetter, fileName, subUnit);

	if (!error) {
		if (loadImage()) {
			strcpy(info, "isoDrive ");
			strcat(info, fileName);
			this->driveLetter = driveLetter;
			this->mediaid = mediaid;
			char buffer[32] = { 0 };
			if (!MSCDEX_GetVolumeName(subUnit, buffer)) strcpy(buffer, "");
			Set_Label(buffer,discLabel,true);

		} else if (CDROM_Interface_Image::images[subUnit]->HasDataTrack() == false && CDROM_Interface_Image::images[subUnit]->HasAudioTrack() == true) { //Audio only cdrom
			strcpy(info, "isoDrive ");
			strcat(info, fileName);
			this->driveLetter = driveLetter;
			this->mediaid = mediaid;
			char buffer[32] = { 0 };
			strcpy(buffer, "Audio_CD");
			Set_Label(buffer,discLabel,true);
		} else error = 6; //Corrupt image
	}
}

isoDrive::~isoDrive() { }

void isoDrive::setFileName(const char* fileName) {
	safe_strncpy(this->fileName, fileName, CROSS_LEN);
	strcpy(info, "isoDrive ");
	strcat(info, fileName);
}

int isoDrive::UpdateMscdex(char driveLetter, const char* path, uint8_t& subUnit) {
	if (MSCDEX_HasDrive(driveLetter)) {
		subUnit = MSCDEX_GetSubUnit(driveLetter);
		CDROM_Interface_Image* oldCdrom = CDROM_Interface_Image::images[subUnit];
		CDROM_Interface* cdrom = new CDROM_Interface_Image(subUnit);
		char pathCopy[CROSS_LEN];
		safe_strncpy(pathCopy, path, CROSS_LEN);
		if (!cdrom->SetDevice(pathCopy, 0)) {
			CDROM_Interface_Image::images[subUnit] = oldCdrom;
			delete cdrom;
			return 3;
		}
		MSCDEX_ReplaceDrive(cdrom, subUnit);
		return 0;
	} else {
		return MSCDEX_AddDrive(driveLetter, path, subUnit);
	}
}

void isoDrive::Activate(void) {
	UpdateMscdex(driveLetter, fileName, subUnit);
}

bool isoDrive::FileOpen(DOS_File **file, const char *name, uint32_t flags) {
	FileStat_Block file_stat;
	bool success;

	if ((flags & 0x0f) == OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	if (is_udf) {
		UDFFileIdentifierDescriptor fid;
		UDFFileEntry fe;
		success = lookup(fid, fe, name) && !(fid.FileCharacteristics & 0x02/*Directory*/);
		if (success) {
			UDFextents fex;

			UDFFileEntryToExtents(fex,fe);

			file_stat.size = (uint32_t)std::min((uint64_t)fe.InformationLength,(uint64_t)0xFFFFFFFF);
			file_stat.date = DOS_PackDate(fe.ModificationDateAndTime.Year,fe.ModificationDateAndTime.Month,fe.ModificationDateAndTime.Day);
			file_stat.time = DOS_PackTime(fe.ModificationDateAndTime.Hour,fe.ModificationDateAndTime.Minute,fe.ModificationDateAndTime.Second);
			file_stat.attr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
			isoFile *ifile = new isoFile(this, name, &file_stat, 0);
			ifile->udffext = fex;
			ifile->udf = true;
			*file = ifile;
			(*file)->flags = flags;
		}
	}
	else {
		isoDirEntry de;
		success = lookup(&de, name) && !IS_DIR(FLAGS1);
		if (success) {
			file_stat.size = DATA_LENGTH(de);
			file_stat.date = DOS_PackDate(1900 + de.dateYear, de.dateMonth, de.dateDay);
			file_stat.time = DOS_PackTime(de.timeHour, de.timeMin, de.timeSec);
			file_stat.attr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
			*file = new isoFile(this, name, &file_stat, EXTENT_LOCATION(de) * ISO_FRAMESIZE);
			(*file)->flags = flags;
		}
	}

	return success;
}

bool isoDrive::FileCreate(DOS_File** /*file*/, const char* /*name*/, uint16_t /*attributes*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::FileUnlink(const char* /*name*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::RemoveDir(const char* /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::MakeDir(const char* /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::TestDir(const char *dir) {
	if (is_udf) {
		UDFFileIdentifierDescriptor fid;
		UDFFileEntry fe;
		return (lookup(fid, fe, dir) && (fid.FileCharacteristics & 0x02/*Directory*/));
	}
	else {
		isoDirEntry de;	
		return (lookup(&de, dir) && IS_DIR(FLAGS1));
	}
}

bool isoDrive::FindFirst(const char *dir, DOS_DTA &dta, bool fcb_findfirst) {
	if (is_udf) {
		UDFFileIdentifierDescriptor fid;
		UDFFileEntry fe;
		if (!lookup(fid, fe, dir)) {
			DOS_SetError(DOSERR_PATH_NOT_FOUND);
			return false;
		}

		// get a directory iterator and save its id in the dta
		int dirIterator = GetDirIterator(fe);
		bool isRoot = (*dir == 0);
		dirIterators[dirIterator].root = isRoot;
		if (lfn_filefind_handle>=LFN_FILEFIND_MAX)
			dta.SetDirID((uint16_t)dirIterator);
		else
			sdid[lfn_filefind_handle]=dirIterator;

		uint8_t attr;
		char pattern[CROSS_LEN];
		dta.GetSearchParams(attr, pattern, false);

		if (attr == DOS_ATTR_VOLUME) {
			dta.SetResult(discLabel, discLabel, 0, 0, 0, 0, DOS_ATTR_VOLUME);
			return true;
		} else if ((attr & DOS_ATTR_VOLUME) && isRoot && !fcb_findfirst) {
			if (WildFileCmp(discLabel,pattern)) {
				// Get Volume Label (DOS_ATTR_VOLUME) and only in basedir and if it matches the searchstring
				dta.SetResult(discLabel, discLabel, 0, 0, 0, 0, DOS_ATTR_VOLUME);
				return true;
			}
		}

		UDFFileEntryToExtents(dirIterators[dirIterator].udfdirext,fe);
		return FindNext(dta);
	}
	else {
		isoDirEntry de;
		if (!lookup(&de, dir)) {
			DOS_SetError(DOSERR_PATH_NOT_FOUND);
			return false;
		}

		// get a directory iterator and save its id in the dta
		int dirIterator = GetDirIterator(&de);
		bool isRoot = (*dir == 0);
		dirIterators[dirIterator].root = isRoot;
		if (lfn_filefind_handle>=LFN_FILEFIND_MAX)
			dta.SetDirID((uint16_t)dirIterator);
		else
			sdid[lfn_filefind_handle]=dirIterator;

		uint8_t attr;
		char pattern[CROSS_LEN];
		dta.GetSearchParams(attr, pattern, false);

		if (attr == DOS_ATTR_VOLUME) {
			dta.SetResult(discLabel, discLabel, 0, 0, 0, 0, DOS_ATTR_VOLUME);
			return true;
		} else if ((attr & DOS_ATTR_VOLUME) && isRoot && !fcb_findfirst) {
			if (WildFileCmp(discLabel,pattern)) {
				// Get Volume Label (DOS_ATTR_VOLUME) and only in basedir and if it matches the searchstring
				dta.SetResult(discLabel, discLabel, 0, 0, 0, 0, DOS_ATTR_VOLUME);
				return true;
			}
		}

		return FindNext(dta);
	}
}

bool isoDrive::FindNext(DOS_DTA &dta) {
	uint8_t attr;
	char fname[LFN_NAMELENGTH];
	char pattern[CROSS_LEN], findName[DOS_NAMELENGTH_ASCII], lfindName[ISO_MAXPATHNAME];
	dta.GetSearchParams(attr, pattern, false);

	int dirIterator = lfn_filefind_handle>=LFN_FILEFIND_MAX?dta.GetDirID():sdid[lfn_filefind_handle];
	bool isRoot = dirIterators[dirIterator].root;

	if (is_udf) {
		UDFFileIdentifierDescriptor fid;
		UDFFileEntry fe;
		while (GetNextDirEntry(dirIterator, fid, fe, dirIterators[dirIterator].udfdirext, fname, dirIterators[dirIterator].index)) {
			uint8_t findAttr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
			if (fid.FileCharacteristics & 0x02/*Directory*/) findAttr |= DOS_ATTR_DIRECTORY;

			/* skip parent directory */
			if (fid.LengthOfFileIdentifier == 0 || (fid.FileCharacteristics & 0x08)) continue;

			strcpy(lfindName,fullname);
			if (!(isRoot && fname[0]=='.') && (WildFileCmp((char*)fname, pattern) || LWildFileCmp(lfindName, pattern))
					&& !(~attr & findAttr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM))) {

				/* file is okay, setup everything to be copied in DTA Block */
				findName[0] = 0;
				if(strlen((char*)fname) < DOS_NAMELENGTH_ASCII) {
					strcpy(findName, (char*)fname);
					upcase(findName);
				}
				uint32_t findSize = (uint32_t)std::min((uint64_t)fe.InformationLength,(uint64_t)0xFFFFFFFF);
				uint16_t findDate = DOS_PackDate(fe.ModificationDateAndTime.Year,fe.ModificationDateAndTime.Month,fe.ModificationDateAndTime.Day);
				uint16_t findTime = DOS_PackTime(fe.ModificationDateAndTime.Hour,fe.ModificationDateAndTime.Minute,fe.ModificationDateAndTime.Second);
				dta.SetResult(findName, lfindName, findSize, 0, findDate, findTime, findAttr);
				return true;
			}
		}
	}
	else {
		isoDirEntry de = {};
		while (GetNextDirEntry(dirIterator, &de)) {
			uint8_t findAttr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
			if (IS_DIR(FLAGS1)) findAttr |= DOS_ATTR_DIRECTORY;
			if (IS_HIDDEN(FLAGS1)) findAttr |= DOS_ATTR_HIDDEN;
			strcpy(lfindName,fullname);
			if (!IS_ASSOC(FLAGS1) && !(isRoot && de.ident[0]=='.') && (WildFileCmp((char*)de.ident, pattern) || LWildFileCmp(lfindName, pattern))
					&& !(~attr & findAttr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM))) {

				/* file is okay, setup everything to be copied in DTA Block */
				findName[0] = 0;
				if(strlen((char*)de.ident) < DOS_NAMELENGTH_ASCII) {
					strcpy(findName, (char*)de.ident);
					upcase(findName);
				}
				uint32_t findSize = DATA_LENGTH(de);
				uint16_t findDate = DOS_PackDate(1900 + de.dateYear, de.dateMonth, de.dateDay);
				uint16_t findTime = DOS_PackTime(de.timeHour, de.timeMin, de.timeSec);
				dta.SetResult(findName, lfindName, findSize, 0, findDate, findTime, findAttr);
				return true;
			}
		}
	}

	// after searching the directory, free the iterator
	FreeDirIterator(dirIterator);

	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
}

bool isoDrive::Rename(const char* /*oldname*/, const char* /*newname*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool isoDrive::SetFileAttr(const char * name,uint16_t attr) {
	(void)attr;

	if (is_udf) {
		UDFFileIdentifierDescriptor fid;
		UDFFileEntry fe;
		DOS_SetError(lookup(fid, fe, name) ? DOSERR_ACCESS_DENIED : DOSERR_FILE_NOT_FOUND);
		return false;
	}
	else {
		isoDirEntry de;
		DOS_SetError(lookup(&de, name) ? DOSERR_ACCESS_DENIED : DOSERR_FILE_NOT_FOUND);
		return false;
	}
}

bool isoDrive::GetFileAttr(const char *name, uint16_t *attr) {
	*attr = 0;

	if (is_udf) {
		UDFFileIdentifierDescriptor fid;
		UDFFileEntry fe;
		bool success = lookup(fid, fe, name);
		if (success) {
			*attr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
			if (fid.FileCharacteristics & 0x02/*Directory*/) *attr |= DOS_ATTR_DIRECTORY;
		}
		return success;
	}
	else {
		isoDirEntry de;
		bool success = lookup(&de, name);
		if (success) {
			*attr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
			if (IS_HIDDEN(FLAGS1)) *attr |= DOS_ATTR_HIDDEN;
			if (IS_DIR(FLAGS1)) *attr |= DOS_ATTR_DIRECTORY;
		}
		return success;
	}
}

bool isoDrive::GetFileAttrEx(char* name, struct stat *status) {
	(void)name;
	(void)status;
	return false;
}

unsigned long isoDrive::GetCompressedSize(char* name) {
	(void)name;
	return 0;
}

#if defined (WIN32)
HANDLE isoDrive::CreateOpenFile(const char* name) {
	(void)name;
	DOS_SetError(1);
	return INVALID_HANDLE_VALUE;
}
#endif

bool isoDrive::AllocationInfo(uint16_t *bytes_sector, uint8_t *sectors_cluster, uint16_t *total_clusters, uint16_t *free_clusters) {
	*bytes_sector = 2048;
	*sectors_cluster = 1; // cluster size for cdroms ?
	*total_clusters = 65535;
	*free_clusters = 0;
	return true;
}

bool isoDrive::FileExists(const char *name) {
	if (is_udf) {
		UDFFileIdentifierDescriptor fid;
		UDFFileEntry fe;
		return (lookup(fid, fe, name) && !(fid.FileCharacteristics & 0x02/*Directory*/));
	}
	else {
		isoDirEntry de;
		return (lookup(&de, name) && !IS_DIR(FLAGS1));
	}
}

bool isoDrive::FileStat(const char *name, FileStat_Block *const stat_block) {
	if (is_udf) {
		UDFFileIdentifierDescriptor fid;
		UDFFileEntry fe;
		bool success = lookup(fid, fe, name);

		if (success) {
			stat_block->date = DOS_PackDate(fe.ModificationDateAndTime.Year,fe.ModificationDateAndTime.Month,fe.ModificationDateAndTime.Day);
			stat_block->time = DOS_PackTime(fe.ModificationDateAndTime.Hour,fe.ModificationDateAndTime.Minute,fe.ModificationDateAndTime.Second);
			stat_block->size = (uint32_t)std::min((uint64_t)fe.InformationLength,(uint64_t)0xFFFFFFFF);
			stat_block->attr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
			if (fid.FileCharacteristics & 0x02/*Directory*/) stat_block->attr |= DOS_ATTR_DIRECTORY;
		}

		return success;
	}
	else {
		isoDirEntry de;
		bool success = lookup(&de, name);

		if (success) {
			stat_block->date = DOS_PackDate(1900 + de.dateYear, de.dateMonth, de.dateDay);
			stat_block->time = DOS_PackTime(de.timeHour, de.timeMin, de.timeSec);
			stat_block->size = DATA_LENGTH(de);
			stat_block->attr = DOS_ATTR_ARCHIVE | DOS_ATTR_READ_ONLY;
			if (IS_DIR(FLAGS1)) stat_block->attr |= DOS_ATTR_DIRECTORY;
		}

		return success;
	}
}

uint8_t isoDrive::GetMediaByte(void) {
	return mediaid;
}

bool isoDrive::isRemote(void) {
	return true;
}

bool isoDrive::isRemovable(void) {
	return true;
}

Bits isoDrive::UnMount(void) {
	if(MSCDEX_RemoveDrive(driveLetter)) {
		delete this;
		return 0;
	}
	return 2;
}

int isoDrive::GetDirIterator(const UDFFileEntry &fe) {
    (void)fe;
	if (!is_udf) return 0;

	int dirIterator = nextFreeDirIterator;

	dirIterators[dirIterator].currentSector = 0; // irrelevant
	dirIterators[dirIterator].endSector = 0; // irrelevant

	// reset position and mark as valid
	dirIterators[dirIterator].pos = 0;
	dirIterators[dirIterator].index = 0;
	dirIterators[dirIterator].valid = true;

	// advance to next directory iterator (wrap around if necessary)
	nextFreeDirIterator = (nextFreeDirIterator + 1) % MAX_OPENDIRS;

	return dirIterator;
}

int isoDrive::GetDirIterator(const isoDirEntry* de) {
	// Not for UDF filesystem use!
	if (is_udf) return 0;

	int dirIterator = nextFreeDirIterator;

	// get start and end sector of the directory entry (pad end sector if necessary)
	dirIterators[dirIterator].currentSector = EXTENT_LOCATION(*de);
	dirIterators[dirIterator].endSector =
		EXTENT_LOCATION(*de) + DATA_LENGTH(*de) / ISO_FRAMESIZE - 1;
	if (DATA_LENGTH(*de) % ISO_FRAMESIZE != 0)
		dirIterators[dirIterator].endSector++;

	// reset position and mark as valid
	dirIterators[dirIterator].pos = 0;
	dirIterators[dirIterator].index = 0;
	dirIterators[dirIterator].valid = true;

	// advance to next directory iterator (wrap around if necessary)
	nextFreeDirIterator = (nextFreeDirIterator + 1) % MAX_OPENDIRS;

	return dirIterator;
}

bool isoDrive::GetNextDirEntry(const int dirIteratorHandle, UDFFileIdentifierDescriptor &fid, UDFFileEntry &fe, UDFextents &dirext, char fname[LFN_NAMELENGTH],unsigned int dirIteratorIndex) {
	if (!is_udf) return 0;

	UDFTagId ctag;
	uint8_t* buffer = NULL;
	unsigned char dirent[4096];
	DirIterator& dirIterator = dirIterators[dirIteratorHandle];

	/* this code only concerns itself with File Identifier Descriptors */
	if (UDFextent_read(dirext, dirent, 38) != 38) return false;
	ctag.parse(16,dirent);
	if (!ctag.tagChecksumOK(16,dirent) || ctag.TagIdentifier != 257/*File Identifier*/) {
		UDFextent_seek(dirext,0x7FFFFFFFu); // we're done
		return false;
	}

	uint8_t L_FI = dirent[19];
	uint16_t L_IU = le16toh( *((uint16_t*)(&dirent[36])) );
	size_t totallen = 38 + L_FI + L_IU;

	/* there is padding */
	if (totallen & 3u) totallen = (totallen | 3u) + 1u;

	if (totallen > sizeof(dirent)) {
		UDFextent_seek(dirext,0x7FFFFFFFu); // we're done
		return false;
	}

	/* now load the rest of the descriptor */
	if (totallen > 38u) {
		if (UDFextent_read(dirext, dirent+38, totallen-38) != (totallen-38)) {
			UDFextent_seek(dirext,0x7FFFFFFFu); // we're done
			return false;
		}
	}
	if (!ctag.dataChecksumOK(totallen,dirent)) {
		UDFextent_seek(dirext,0x7FFFFFFFu); // we're done
		return false;
	}

	++dirIterator.index;
	fid.get(ctag,totallen,dirent);

	{
		UDFextents iex(convertToUDFextent_ad(fid.ICB));
		bool fe_found = false;

		if (UDFextent_read(iex,dirent,COOKED_SECTOR_SIZE) == COOKED_SECTOR_SIZE) {
			if (ctag.get(COOKED_SECTOR_SIZE,dirent)) {
				if (ctag.TagIdentifier == 261/*File Entry*/) {
					fe = UDFFileEntry();
					fe.get(ctag,COOKED_SECTOR_SIZE,dirent);
					fe_found = true;
				}
			}
		}

		if (!fe_found) {
			LOG(LOG_MISC,LOG_DEBUG)("UDF: File Identifier cannot find File Entry for '%s'",fid.FileIdentifier.string_value().c_str());
			fe = UDFFileEntry();
		}
	}

	const std::string ennamepp = fid.FileIdentifier.string_value();
	const char *enname = ennamepp.c_str();

	// NTS: For whatever reason, the first byte may be a control value between 0x01 to 0x08.
	if (*enname >= 0x01 && *enname <= 0x08) enname++;

	strcpy((char*)fullname,(char*)enname);
	strcpy((char*)fname,(char*)enname);

	{
		const char *ext = NULL;
		size_t tailsize = 0;
		bool lfn = false;
		char tail[128];

		if (strcmp((char*)fullname,".") != 0 && strcmp((char*)fullname,"..")) {
			size_t nl = 0,el = 0,periods = 0;
			const char *s = (const char*)fullname;
			while (*s != 0) {
				if (*s == '.') break;
				if (islfnchar(s)) lfn = true;
				nl++;
				s++;
			}
			if (!nl || nl > 8) lfn = true;
			if (*s == '.') {
				periods++;
				if (nl) ext = s+1;
				s++;
			}
			while (*s != 0) {
				if (*s == '.') {
					periods++;
					ext = s+1;
				}
				if (islfnchar(s)) lfn = true;
				el++;
				s++;
			}
			if (periods > 1 || el > 3) lfn = true;
		}

		/* Windows 95 adds a ~number to the 8.3 name if effectively an LFN.
		 * I'm not 100% certain but the index appears to be related to the index of the ISO 9660 dirent.
		 * This is a guess as to how it works. */
		if (lfn) {
			tailsize = sprintf(tail,"~%u",dirIteratorIndex);
			const char *s = (const char*)fullname;
			char *d = (char*)fname;
			size_t c;

			c = 0;
			while (*s == '.'||*s == ' ') s++;
			bool lead = false;
			while (*s != 0) {
				if (s == ext) break; // doesn't match if ext == NULL, so no harm in that case
                if (!lead && ((IS_PC98_ARCH && shiftjis_lead_byte(*s & 0xFF)) || (isDBCSCP() && isKanji1_gbk(*s & 0xFF)))) {
                    if (c >= (7-tailsize)) break;
                    lead = true;
                    *d++ = *s;
                    c++;
				} else if ((unsigned char)*s <= 32 || (unsigned char)*s == 127 || *s == '.' || *s == '\"' || *s == '+' || *s == '=' || *s == ',' || *s == ';' || *s == ':' || *s == '<' || *s == '>' || ((*s == '[' || *s == ']' || *s == '|' || *s == '\\')&&(!lead||((dos.loaded_codepage==936||IS_PDOSV)&&!gbk)))||*s=='?'||*s=='*') {
                    lead = false;
                    if (*s != '.') {
                        *d++ = '_';
                        c++;
                    }
                } else if (c >= (8-tailsize)) {
                    if (s < ext) s = ext;
                    break;
                } else {
                    lead = false;
					*d++ = *s;
					c++;
				}
				s++;
			}
			if (tailsize != 0) {
				for (c=0;c < tailsize;c++) *d++ = tail[c];
			}
			lead = false;
			if (s == ext) { /* ext points to char after '.' */
				if (*s != 0) { /* anything after? */
					*d++ = '.';
					c = 0;
					while (*s != 0) {
						if (!lead && ((IS_PC98_ARCH && shiftjis_lead_byte(*s & 0xFF)) || (isDBCSCP() && isKanji1_gbk(*s & 0xFF)))) {
                            if (c >= 2) break;
                            lead = true;
                            *d++ = *s;
                            c++;
						} else if ((unsigned char)*s <= 32 || (unsigned char)*s == 127 || *s == '.' || *s == '\"' || *s == '+' || *s == '=' || *s == ',' || *s == ';' || *s == ':' || *s == '<' || *s == '>' || ((*s == '[' || *s == ']' || *s == '|' || *s == '\\')&&(!lead||((dos.loaded_codepage==936||IS_PDOSV)&&!gbk)))||*s=='?'||*s=='*') {
                            lead = false;
                            *d++ = '_';
                            c++;
                        } else if (c >= 3) break;
                        else {
							*d++ = *s;
							c++;
						}
						s++;
					}
				}
			}
			*d = 0;
		}
	}

	return true;
}

bool isoDrive::GetNextDirEntry(const int dirIteratorHandle, isoDirEntry* de) {
	// Not for UDF filesystem use!
	if (is_udf) return 0;

	bool result = false;
	uint8_t* buffer = NULL;
	DirIterator& dirIterator = dirIterators[dirIteratorHandle];

	// check if the directory entry is valid
	if (dirIterator.valid && ReadCachedSector(&buffer, dirIterator.currentSector)) {
		// check if the next sector has to be read
		if ((dirIterator.pos >= ISO_FRAMESIZE)
			|| (buffer[dirIterator.pos] == 0)
			|| (dirIterator.pos + buffer[dirIterator.pos] > ISO_FRAMESIZE)) {

			// check if there is another sector available
			if (dirIterator.currentSector < dirIterator.endSector) {
				dirIterator.pos = 0;
				dirIterator.currentSector++;
				if (!ReadCachedSector(&buffer, dirIterator.currentSector)) {
					return false;
				}
			} else {
				return false;
			}
		}
		// read sector and advance sector pointer
		++dirIterator.index;
		int length = readDirEntry(de, &buffer[dirIterator.pos], dirIterator.index);
		result = length >= 0;
		if (length > 0) dirIterator.pos += (unsigned int)length;
	}

	return result;
}

void isoDrive::FreeDirIterator(const int dirIterator) {
	dirIterators[dirIterator].valid = false;
	
	// if this was the last acquired iterator decrement nextFreeIterator
	if ((dirIterator + 1) % MAX_OPENDIRS == nextFreeDirIterator) {
		if (nextFreeDirIterator>0) {
			nextFreeDirIterator--;
		} else {
			nextFreeDirIterator = MAX_OPENDIRS-1;
		}
	}
}

bool isoDrive::ReadCachedSector(uint8_t** buffer, const uint32_t sector) {
	// get hash table entry
	unsigned int pos = sector % ISO_MAX_HASH_TABLE_SIZE;
	SectorHashEntry& he = sectorHashEntries[pos];
	
	// check if the entry is valid and contains the correct sector
	if (!he.valid || he.sector != sector) {
		if (!CDROM_Interface_Image::images[subUnit]->ReadSector(he.data, false, sector)) {
			return false;
		}
		he.valid = true;
		he.sector = sector;
	}
	
	*buffer = he.data;
	return true;
}

inline bool isoDrive :: readSector(uint8_t *buffer, uint32_t sector) {
	return CDROM_Interface_Image::images[subUnit]->ReadSector(buffer, false, sector);
}

int isoDrive::readDirEntry(isoDirEntry* de, const uint8_t* data,unsigned int dirIteratorIndex) {
	// This code is NOT for UDF filesystem access!
	if (is_udf) return -1;

	// copy data into isoDirEntry struct, data[0] = length of DirEntry
//	if (data[0] > sizeof(isoDirEntry)) return -1;//check disabled as isoDirentry is currently 258 bytes large. So it always fits
	memcpy(de, data, data[0]);//Perharps care about a zero at the end.
	
	// xa not supported
	if (de->extAttrLength != 0) return -1;
	// interleaved mode not supported
	if (de->fileUnitSize != 0 || de->interleaveGapSize != 0) return -1;
	
	// modify file identifier for use with dosbox
	if (de->length < 33 + de->fileIdentLength) return -1;
	if (IS_DIR(FLAGS2)) {
		if (de->fileIdentLength == 1 && de->ident[0] == 0) strcpy((char*)de->ident, ".");
		else if (de->fileIdentLength == 1 && de->ident[0] == 1) strcpy((char*)de->ident, "..");
		else {
			if (de->fileIdentLength > 200) return -1;
			de->ident[de->fileIdentLength] = 0;
			if (is_joliet) {
				de->ident[de->fileIdentLength+1] = 0; // for Joliet UCS-16
				// The string is big Endian UCS-16, convert to host Endian UCS-16
				for (size_t i=0;((const uint16_t*)de->ident)[i] != 0;i++) ((uint16_t*)de->ident)[i] = be16toh(((uint16_t*)de->ident)[i]);
				// finally, convert from UCS-16 to local code page, using C++ string construction to make a copy first
				CodePageHostToGuestUTF16((char*)de->ident,std::basic_string<uint16_t>((const uint16_t*)de->ident).c_str());
			}
		}
	} else {
		if (de->fileIdentLength > 200) return -1;
		de->ident[de->fileIdentLength] = 0;	
		if (is_joliet) {
			de->ident[de->fileIdentLength+1] = 0; // for Joliet UCS-16
			// remove any file version identifiers as there are some cdroms that don't have them
			uint16_t *w = (uint16_t*)(de->ident); // remember two NULs were written to make a UCS-16 NUL
			while (*w != 0) {
				if (be16toh(*w) == (uint16_t)(';')) *w = 0;
				w++;
			}
			// w happens to be at the end of the string now, step back one char and remove trailing period
			if (w != (uint16_t*)(de->ident)) {
				w--;
				if (be16toh(*w) == (uint16_t)('.')) *w = 0;
			}
			// The string is big Endian UCS-16, convert to host Endian UCS-16
			for (size_t i=0;((const uint16_t*)de->ident)[i] != 0;i++) ((uint16_t*)de->ident)[i] = be16toh(((uint16_t*)de->ident)[i]);
			// finally, convert from UCS-16 to local code page, using C++ string construction to make a copy first
			CodePageHostToGuestUTF16((char*)de->ident,std::basic_string<uint16_t>((const uint16_t*)de->ident).c_str());
		}
		else {
			// remove any file version identifiers as there are some cdroms that don't have them
			strreplace((char*)de->ident, ';', 0);	
			// if file has no extension remove the trailing dot
			size_t tmp = strlen((char*)de->ident);
			if (tmp > 0) {
				if (de->ident[tmp - 1] == '.') de->ident[tmp - 1] = 0;
			}
		}
	}

	bool is_rock_ridge_name = false;
	if (is_joliet) {
		strcpy((char*)fullname,(char*)de->ident);
	}
	else {
		strcpy((char*)fullname,(char*)de->ident);
		if (is_rock_ridge) {
			/* LEN_SKP bytes into the System Use Field (bytes after the final NUL byte of the identifier) */
			/* NTS: This code could never work with Joliet extensions because the code above (currently)
			 *      overwrites the first byte of the SUSP entries in order to make a NUL-terminated UCS-16
			 *      string */
			unsigned char *p = (unsigned char*)de->ident + de->fileIdentLength + 1/*NUL*/ + rr_susp_skip;
			unsigned char *f = (unsigned char*)de + de->length;

			/* SUSP basic structure:
			 *
			 * BP1/BP2   +0    <2 char signature>
			 * BP3       +2    <length including header>
			 * BP4       +3    <SUSP version>
			 * [payload] */
			while ((p+4) <= f) {
				unsigned char *entry = p;
				uint8_t len = entry[2];
				uint8_t ver = entry[3];

				if (len < 4) break;
				if ((p+len) > f) break;
				p += len;

				if (!memcmp(entry,"NM",2)) {
					/* BP5       +4         flags
					 *                         bit 0 = CONTINUE
					 *                         bit 1 = CURRENT
					 *                         bit 2 = PARENT
					 *                         bit [7:3] = RESERVED */
					if (len >= 5/*flags and at least one char*/ && ver == 1) {
						uint8_t flags = entry[4];

						if (flags & 0x7) {
							/* CONTINUE (bit 0) not supported yet, or CURRENT directory (bit 1), or PARENT directory (bit 2) */
						}
						else {
							/* BP6-payload end is the alternate name */
							size_t altnl = len - 5;

							if (altnl > 0) memcpy(fullname,entry+5,altnl);
							fullname[altnl] = 0;

							is_rock_ridge_name = true;
						}
					}
				}
			}
		}
	}

	bool jolietrr = is_joliet || (is_rock_ridge_name && filename_not_strict_8x3((char*)de->ident));
	if (!jolietrr && !(dos.version.major >= 7 || uselfn)) {
		char* dotpos = strchr((char*)de->ident, '.');
		if (dotpos!=NULL) {
			if (strlen(dotpos)>4) dotpos[4]=0;
			if (dotpos-(char*)de->ident>8) {
				strcpy((char*)(&de->ident[8]),dotpos);
			}
		} else if (strlen((char*)de->ident)>8) de->ident[8]=0;
	}
	if (jolietrr || filename_not_8x3((char*)de->ident)) {
		const char *ext = NULL;
		size_t tailsize = 0;
		bool lfn = false;
		char tail[128];

		if (strcmp((char*)fullname,".") != 0 && strcmp((char*)fullname,"..")) {
			size_t nl = 0,el = 0,periods = 0;
			const char *s = (const char*)fullname;
			while (*s != 0) {
				if (*s == '.') break;
				if (islfnchar(s)) lfn = true;
				nl++;
				s++;
			}
			if (!nl || nl > 8) lfn = true;
			if (*s == '.') {
				periods++;
				if (nl) ext = s+1;
				s++;
			}
			while (*s != 0) {
				if (*s == '.') {
					periods++;
					ext = s+1;
				}
				if (islfnchar(s)) lfn = true;
				el++;
				s++;
			}
			if (periods > 1 || el > 3) lfn = true;
		}

		/* Windows 95 adds a ~number to the 8.3 name if effectively an LFN.
		 * I'm not 100% certain but the index appears to be related to the index of the ISO 9660 dirent.
		 * This is a guess as to how it works. */
		if (lfn) {
			tailsize = sprintf(tail,"~%u",dirIteratorIndex);
			const char *s = (const char*)fullname;
			char *d = (char*)de->ident;
			size_t c;

			c = 0;
			while (*s == '.'||*s == ' ') s++;
			bool lead = false;
			while (*s != 0) {
				if (s == ext) break; // doesn't match if ext == NULL, so no harm in that case
                if (!lead && ((IS_PC98_ARCH && shiftjis_lead_byte(*s & 0xFF)) || (isDBCSCP() && isKanji1_gbk(*s & 0xFF)))) {
                    if (c >= (7-tailsize)) break;
                    lead = true;
                    *d++ = *s;
                    c++;
				} else if ((unsigned char)*s <= 32 || (unsigned char)*s == 127 || *s == '.' || *s == '\"' || *s == '+' || *s == '=' || *s == ',' || *s == ';' || *s == ':' || *s == '<' || *s == '>' || ((*s == '[' || *s == ']' || *s == '|' || *s == '\\')&&(!lead||((dos.loaded_codepage==936||IS_PDOSV)&&!gbk)))||*s=='?'||*s=='*') {
                    lead = false;
                    if (*s != '.') {
                        *d++ = '_';
                        c++;
                    }
                } else if (c >= (8-tailsize)) {
                    if (s < ext) s = ext;
                    break;
                } else {
                    lead = false;
					*d++ = *s;
					c++;
				}
				s++;
			}
			if (tailsize != 0) {
				for (c=0;c < tailsize;c++) *d++ = tail[c];
			}
			lead = false;
			if (s == ext) { /* ext points to char after '.' */
				if (*s != 0) { /* anything after? */
					*d++ = '.';
					c = 0;
					while (*s != 0) {
						if (!lead && ((IS_PC98_ARCH && shiftjis_lead_byte(*s & 0xFF)) || (isDBCSCP() && isKanji1_gbk(*s & 0xFF)))) {
                            if (c >= 2) break;
                            lead = true;
                            *d++ = *s;
                            c++;
						} else if ((unsigned char)*s <= 32 || (unsigned char)*s == 127 || *s == '.' || *s == '\"' || *s == '+' || *s == '=' || *s == ',' || *s == ';' || *s == ':' || *s == '<' || *s == '>' || ((*s == '[' || *s == ']' || *s == '|' || *s == '\\')&&(!lead||((dos.loaded_codepage==936||IS_PDOSV)&&!gbk)))||*s=='?'||*s=='*') {
                            lead = false;
                            *d++ = '_';
                            c++;
                        } else if (c >= 3) break;
                        else {
							*d++ = *s;
							c++;
						}
						s++;
					}
				}
			}
			*d = 0;
		}
	}
	return de->length;
}

static bool escape_is_joliet(const unsigned char *esc) {
	if (    !memcmp(esc,"\x25\x2f\x40",3) ||
		!memcmp(esc,"\x25\x2f\x43",3) ||
		!memcmp(esc,"\x25\x2f\x45",3)) {
		return true;
	}

	return false;
}

bool isoDrive :: loadImageUDFAnchorVolumePointer(UDFAnchorVolumeDescriptorPointer &avdp,uint8_t pvd[COOKED_SECTOR_SIZE],uint32_t sector) {
	UDFTagId aid;

	if (!aid.get(COOKED_SECTOR_SIZE,pvd)) return false;
	if (aid.TagIdentifier != 2/*Anchor volume descriptor*/ || aid.TagLocation != sector) return false;
	avdp.get(aid,COOKED_SECTOR_SIZE,pvd);
	return true;
}

bool isoDrive :: loadImageUDF() {
	uint8_t pvd[COOKED_SECTOR_SIZE];
	UDFextent_ad avdex;

	lvold.DescriptorTag = UDFTagId();
	pvold.DescriptorTag = UDFTagId();
	fsetd.DescriptorTag = UDFTagId();
	partd.DescriptorTag = UDFTagId();

	/* look for the anchor volume descriptor */
	{
		UDFAnchorVolumeDescriptorPointer avdp;

		// Try 1: Read the anchor descriptor at sector 256
		memset(pvd,0,16);
		readSector(pvd,256);
		if (!loadImageUDFAnchorVolumePointer(avdp,pvd,256)) {
			// TODO: If there is another one at sector N - 256.
			//       Figure out how to determine the number of
			//       sectors in the ISO.
			return false;
		}

		avdex = avdp.MainVolumeDescriptorSequenceExtent;
		if (avdex.ExtentLocation == 0 || avdex.ExtentLength == 0) return false;
	}

	LOG(LOG_MISC,LOG_DEBUG)("UDF Anchor Volume Descriptor points to location=%lu len=%lu",
		(unsigned long)avdex.ExtentLocation,
		(unsigned long)avdex.ExtentLength);

	/* within the AVD look for other descriptors */
	for (uint32_t rsec=0u;rsec < size_t(avdex.ExtentLength/COOKED_SECTOR_SIZE);rsec++) {
		uint32_t asec = rsec + avdex.ExtentLocation;
		UDFTagId ctag;

		memset(pvd,0,16);
		readSector(pvd,asec);
		if (!ctag.get(COOKED_SECTOR_SIZE,pvd)) continue;
		if (ctag.TagLocation != asec) continue;

		if (ctag.TagIdentifier == 0x01/*Primary Volume Descriptor*/) {
			pvold.get(ctag,COOKED_SECTOR_SIZE,pvd);
		}
		else if (ctag.TagIdentifier == 0x05/*Partition Descriptor*/) {
			UDFPartitionDescriptor tpartd;

			tpartd.get(ctag,COOKED_SECTOR_SIZE,pvd);

                        /* We're looking for +NSR02 or +NSR03 as per ECMA-167 */
                        if (    !strcmp((const char*)tpartd.PartitionContents.Identifier,"+NSR02") ||
                                !strcmp((const char*)tpartd.PartitionContents.Identifier,"+NSR03")) {
                                if (partd.DescriptorTag.TagIdentifier == 0)
                                        partd = tpartd;
                        }
		}
		else if (ctag.TagIdentifier == 0x06/*Logical Volume Descriptor*/) {
			lvold.get(ctag,COOKED_SECTOR_SIZE,pvd);
		}
		else if (ctag.TagIdentifier == 0x08/*Terminating Descriptor*/) {
			break;
		}
	}

	if (partd.DescriptorTag.TagIdentifier == 0) {
		LOG(LOG_MISC,LOG_DEBUG)("UDF: Failed to find partition descriptor");
		return false;
	}

	// NTS: Someday if DOSBox-X is expected to read UDF 2.50 type discs (like Blu-ray, lol), this code
	//      will need to search the logical volume descriptor for partition maps that point to a "metadata partition"
	//      which then magically defines a new partition ID and location, within which Blu-ray likes to put the
	//      root directory. Yes, really XD. Then you scan THAT partition for the root directory and all extents of
	//      the root directory referring to other folders are relative to that metadata partition.

	// DOSBox-X is unlikely to ever read Blu-ray discs, so a simple scan of the main partition is sufficient.
	// NTS: UDF descriptor tags in the partition have sector numbers relative to the partition!
	for (uint32_t rsec=0;rsec < 256;rsec++) {
		uint32_t asec = rsec + partd.PartitionStartingLocation;
		UDFTagId ctag;

		memset(pvd,0,16);
		readSector(pvd,asec);
		if (!ctag.get(COOKED_SECTOR_SIZE,pvd)) continue;
		if (ctag.TagLocation != rsec) continue; // "Tag Location" is relative to partition now!

		if (ctag.TagIdentifier == 0x08/*Terminator Descriptor*/) {
			break;
		}
		else if (ctag.TagIdentifier == 0x100/*FileSetDescriptor*/) {
			fsetd.get(ctag,COOKED_SECTOR_SIZE,pvd); /* This FSD points at the root directory */
		}
	}

	if (fsetd.DescriptorTag.TagIdentifier == 0) {
		LOG(LOG_MISC,LOG_DEBUG)("UDF: Did not find root directory FileSetDescriptor");
		return false;
	}

	return true;
}

bool isoDrive :: loadImage() {
	uint8_t pvd[COOKED_SECTOR_SIZE];
	unsigned int choose_iso9660 = 0;
	unsigned int choose_joliet = 0;
	unsigned int sector = 16;

	is_rock_ridge = false;
	is_udf = false;
	dataCD = false;

	if (loadImageUDF()) {
		LOG(LOG_MISC,LOG_DEBUG)("ISO: UDF filesystem detected");
		if (enable_udf) {
			is_udf = true;
			dataCD = true;
			return true;
		} else if (dos.version.major < 7 || (dos.version.major == 7 && dos.version.minor < 10)) {
			const char *msg = "Found UDF image which requires a reported DOS version of 7.10 to mount.\r\n";
			uint16_t n = (uint16_t)strlen(msg);
			DOS_WriteFile (STDOUT,(uint8_t *)msg, &n);
		}
	}

	/* scan the volume descriptors */
	while (sector < 256) {
		pvd[0] = 0xFF;
		readSector(pvd,sector);

		if (pvd[0] == 1) { // primary volume
			if (!strncmp((char*)(&pvd[1]), "CD001", 5) && pvd[6] == 1) {
				if (choose_iso9660 == 0) {
					choose_iso9660 = sector;
					iso = true;
				}
			}
		}
		else if (pvd[0] == 2) { // supplementary volume (usually Joliet, but not necessarily)
			if (!strncmp((char*)(&pvd[1]), "CD001", 5) && pvd[6] == 1) {
				bool joliet = escape_is_joliet(&pvd[88]);

				if (joliet) {
					if (choose_joliet == 0) {
						choose_joliet = sector;
					}
				}
				else {
					if (choose_iso9660 == 0) {
						choose_iso9660 = sector;
						iso = true;
					}
				}
			}
		}
		else if (pvd[0] == 0xFF) { // terminating descriptor
			break;
		}
		else if (pvd[0] >= 0x20) { // oddly out of range, maybe we're lost
			break;
		}

		// unknown check inherited from existing code
		if (pvd[8] == 1 && !strncmp((char*)(&pvd[9]), "CDROM", 5) && pvd[14] == 1) {
			if (choose_iso9660 == 0) {
				choose_iso9660 = sector;
				iso = false;
			}
		}

		sector++;
	}

	if (choose_joliet && enable_joliet) {
		sector = choose_joliet;
		LOG(LOG_MISC,LOG_DEBUG)("ISO 9660: Choosing Joliet (unicode) volume at sector %u",sector);
		is_joliet = true;
		iso = true;
	}
	else if (choose_iso9660) {
		sector = choose_iso9660;
		LOG(LOG_MISC,LOG_DEBUG)("ISO 9660: Choosing ISO 9660 volume at sector %u",sector);
		is_joliet = false;
	}
	else {
		return false;
	}

	readSector(pvd, sector);
	uint16_t offset = iso ? 156 : 180;
	if (readDirEntry(&this->rootEntry, &pvd[offset], 0) <= 0)
		return false;

	/* read the first root directory entry, look for Rock Ridge and/or System Use Sharing Protocol extensions.
	 * It is rare for Rock Ridge extensions to exist on the Joliet supplementary volume as far as I know.
	 * Furthermore the way this code is currently written, Rock Ridge extensions would be ignored anyway for
	 * Joliet extensions. */
	if (!is_joliet && enable_rock_ridge) {
		if (DATA_LENGTH(this->rootEntry) >= 33 && EXTENT_LOCATION(this->rootEntry) != 0) {
			readSector(pvd, EXTENT_LOCATION(this->rootEntry));

			isoDirEntry rde;
			static_assert(sizeof(rde) >= 255, "isoDirEntry less than 255 bytes");
			if (pvd[0] >= 33) {
				memcpy(&rde,pvd,pvd[0]);
				if (IS_DIR(rde.fileFlags) && (rde.fileIdentLength == 1 && rde.ident[0] == 0x00)/*The "." directory*/) {
					/* Good. Is there a SUSP "SP" signature following the 1-byte file identifier? */
					/* @Wengier: If your RR-based ISO images put the "SP" signature somewhere else, feel free to adapt this code.
					 *           The SUSP protocol says it's supposed to be at byte position 1 of the System User Field
					 *           of the First Directory Record of the root directory. */
					unsigned char *p = &rde.ident[1];/*first byte following file ident */
					unsigned char *f = &pvd[pvd[0]];

					if ((p+7/*SP minimum*/) <= f && p[2] >= 7/*length*/ && p[3] == 1/*version*/ &&
						p[4] == 0xBE && p[5] == 0xEF && !memcmp(p,"SP",2)) {
						/* NTS: The spec counts from 1. We count from zero. Both are provided.
						 *
						 * BP1/BP2  +0   "SP"
						 * BP3      +2   7
						 * BP4      +3   1
						 * BP5/BP6  +4   0xBE 0xEF
						 * BP7      +6   LEN_SKP
						 *
						 * LEN_SKP = number of bytes to skip in System Use field of each directory record
						 *           to get to SUSP entries.
						 *
						 *           @Wengier This is the value you should use when searching for "NM"
						 *           entries, rather than byte by byte scanning.
						 */
						LOG(LOG_MISC,LOG_DEBUG)("ISO 9660: Rock Ridge extensions detected");
						is_rock_ridge = true;
						rr_susp_skip = p[6];
					}
				}
			}
		}
	}

	/* Sanity check: This code does NOT support Rock Ridge extensions when reading the Joliet supplementary volume! */
	if (is_joliet) {
		assert(!is_rock_ridge);
	}

	dataCD = true;
	return true;
}

bool isoDrive :: lookup(UDFFileIdentifierDescriptor &fid, UDFFileEntry &fe, const char *path) {
	uint8_t pvd[COOKED_SECTOR_SIZE];
	char fname[LFN_NAMELENGTH];
	bool cisdir = false;
	UDFextents dirext;
	UDFTagId ctag;

	if (!is_udf) return false;
	if (!dataCD) return false;

	// Step 1: Root directory lookup
	if (fsetd.DescriptorTag.TagIdentifier == 0) return false;

	UDFextents rex(convertToUDFextent_ad(fsetd.RootDirectoryICB));
	if (UDFextent_read(rex,pvd,COOKED_SECTOR_SIZE) != COOKED_SECTOR_SIZE) return false;
	if (!ctag.get(COOKED_SECTOR_SIZE,pvd)) return false;

	if (ctag.TagIdentifier == 0x105/*FileEntry*/) {
		/* no file identifier */
		fe.get(ctag,COOKED_SECTOR_SIZE,pvd);
		UDFFileEntryToExtents(dirext,fe);
	}
	else {
		return false;
	}

	fid = UDFFileIdentifierDescriptor();
	fid.FileCharacteristics = 0x02; /* root directory */

	char isoPath[ISO_MAXPATHNAME];
	safe_strncpy(isoPath, path, ISO_MAXPATHNAME);
	strreplace_dbcs(isoPath, '\\', '/');
	cisdir = true;

	// iterate over all path elements (name), and search each of them in the current de
	for(char* name = strtok(isoPath, "/"); NULL != name; name = strtok(NULL, "/")) {
		bool found = false;

		// current entry must be a directory, abort otherwise
		if (cisdir) {
			// remove the trailing dot if present
			size_t nameLength = strlen(name);
			if (nameLength > 0) {
				if (name[nameLength - 1] == '.') name[nameLength - 1] = 0;
			}

			// look for the current path element
			int dirIterator = GetDirIterator(fe);
			while (!found && GetNextDirEntry(dirIterator, fid, fe, dirext, fname, dirIterators[dirIterator].index)) {
				/* skip parent directory */
				if (fid.LengthOfFileIdentifier == 0 || (fid.FileCharacteristics & 0x08)) continue;

				if (0 == strncasecmp((char*)fname, name, ISO_MAX_FILENAME_LENGTH) || 0 == strncasecmp((char*) fullname, name, ISO_MAXPATHNAME)) {
					cisdir = !!(fid.FileCharacteristics & 0x02/*Directory*/);
					UDFFileEntryToExtents(dirext,fe);
					found = true;
				}
			}
			FreeDirIterator(dirIterator);
		}

		if (!found) return false;
	}

	return true;
}

bool isoDrive :: lookup(isoDirEntry *de, const char *path) {
	// This code is not for UDF
	if (is_udf) return false;

	if (!dataCD) return false;
	*de = this->rootEntry;
	if (!strcmp(path, "")) return true;

	char isoPath[ISO_MAXPATHNAME];
	safe_strncpy(isoPath, path, ISO_MAXPATHNAME);
	strreplace_dbcs(isoPath, '\\', '/');
	
	// iterate over all path elements (name), and search each of them in the current de
	for(char* name = strtok(isoPath, "/"); NULL != name; name = strtok(NULL, "/")) {
		bool found = false;

		// current entry must be a directory, abort otherwise
		if (IS_DIR(FLAGS2)) {
			// remove the trailing dot if present
			size_t nameLength = strlen(name);
			if (nameLength > 0) {
				if (name[nameLength - 1] == '.') name[nameLength - 1] = 0;
			}

			// look for the current path element
			int dirIterator = GetDirIterator(de);
			while (!found && GetNextDirEntry(dirIterator, de)) {
				if (!IS_ASSOC(FLAGS2) && ((0 == strncasecmp((char*) de->ident, name, ISO_MAX_FILENAME_LENGTH)) || 0 == strncasecmp((char*) fullname, name, ISO_MAXPATHNAME))) {
					found = true;
				}
			}
			FreeDirIterator(dirIterator);
		}

		if (!found) return false;
	}

	return true;
}

void IDE_ATAPI_MediaChangeNotify(unsigned char drive_index);

void isoDrive :: MediaChange() {
	IDE_ATAPI_MediaChangeNotify(toupper(driveLetter) - 'A'); /* ewwww */
}

void isoDrive :: EmptyCache(void) {
	// Magical Girl Pretty Sammy installer
	// Installer copies files found by FindFirst/FindNext with "command /c copy",
	// this function is called at end of DOS_Shell::CMD_COPY and cache is cleared, so only one file is copied.
	if(IS_PC98_ARCH) {
		const char *label = GetLabel();
		if(!strncmp(label, "SAMY_A98", 8) || !strncmp(label, "SAMY_B98", 8)) {
			// Do not clear cache for Pretty Sammy CD-ROM
			return;
		}
	}
	enable_udf = (dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10));//default
	enable_rock_ridge = dos.version.major >= 7 || uselfn;
	enable_joliet = dos.version.major >= 7 || uselfn;
	is_joliet = false;
	//this->fileName[0]  = '\0'; /* deleted to fix issue #3848. Revert this if there are any flaws */
	//this->discLabel[0] = '\0'; /* deleted to fix issue #3848. Revert this if there are any flaws */
	nextFreeDirIterator = 0;
	memset(dirIterators, 0, sizeof(dirIterators));
	memset(sectorHashEntries, 0, sizeof(sectorHashEntries));
	memset(&rootEntry, 0, sizeof(isoDirEntry));
	//safe_strncpy(this->fileName, fileName, CROSS_LEN); /* deleted to fix issue #3848. Revert this if there are any flaws */
	loadImage();
}
