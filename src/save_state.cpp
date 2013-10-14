#include "save_state.h"
#include "zlib.h"
#ifdef WIN32
#include "direct.h"
#endif
#include "cross.h"
#include "logging.h"
#if defined (__APPLE__)
#else
#include <malloc.h>
#endif
#include <cstring>
#include <fstream>

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <stdlib.h>
#include "SDL.h"

#ifndef WIN32
/* The itoa code is in the puiblic domain */
char* itoa(int value, char* str, int radix) {
	static char dig[] =
		"0123456789"
		"abcdefghijklmnopqrstuvwxyz";
	int n = 0, neg = 0;
	unsigned int v;
	char* p, *q;
	char c;
	if (radix == 10 && value < 0) {
		value = -value;
		neg = 1;
	}
	v = value;
	do {
		str[n++] = dig[v%radix];
		v /= radix;
	} while (v);
	if (neg)
		str[n++] = '-';
	str[n] = '\0';
	for (p = str, q = p + n/2; p != q; ++p, --q)
		c = *p, *p = *q, *q = c;
	return str;
}
#endif

SaveState& SaveState::instance() {
    static SaveState singleton;
    return singleton;
}

void SaveState::registerComponent(const std::string& uniqueName, Component& comp) {
    components.insert(std::make_pair(uniqueName, CompData(comp)));
}

namespace Util {
std::string compress(const std::string& input) { //throw (SaveState::Error)
	if (input.empty())
		return input;

	const uLong bufferSize = ::compressBound(input.size());

	std::string output;
	output.resize(bufferSize);

	uLongf actualSize = bufferSize;
	if (::compress2(reinterpret_cast<Bytef*>(&output[0]), &actualSize,
					reinterpret_cast<const Bytef*>(input.c_str()), input.size(), Z_BEST_SPEED) != Z_OK)
		throw SaveState::Error("Compression failed!");

	output.resize(actualSize);

	//save size of uncompressed data
	const size_t uncompressedSize = input.size(); //save size of uncompressed data
	output.resize(output.size() + sizeof(uncompressedSize)); //won't trigger a reallocation
	::memcpy(&output[0] + output.size() - sizeof(uncompressedSize), &uncompressedSize, sizeof(uncompressedSize));

	return std::string(&output[0], output.size()); //strip reserved space
}

std::string decompress(const std::string& input) { //throw (SaveState::Error)
	if (input.empty())
		return input;

	//retrieve size of uncompressed data
	size_t uncompressedSize = 0;
	::memcpy(&uncompressedSize, &input[0] + input.size() - sizeof(uncompressedSize), sizeof(uncompressedSize));

	std::string output;
	output.resize(uncompressedSize);

	uLongf actualSize = uncompressedSize;
	if (::uncompress(reinterpret_cast<Bytef*>(&output[0]), &actualSize,
					 reinterpret_cast<const Bytef*>(input.c_str()), input.size() - sizeof(uncompressedSize)) != Z_OK)
		throw SaveState::Error("Decompression failed!");

	output.resize(actualSize); //should be superfluous!

	return output;
}
}

inline void SaveState::RawBytes::set(const std::string& stream) {
	bytes = stream;
	isCompressed = false;
	dataExists   = true;
}

inline std::string SaveState::RawBytes::get() const { //throw (Error){
	if (isCompressed)
		(Util::decompress(bytes)).swap(bytes);
	isCompressed = false;
	return bytes;
}

inline void SaveState::RawBytes::compress() const { //throw (Error)
	if (!isCompressed)
		(Util::compress(bytes)).swap(bytes);
	isCompressed = true;
}

inline bool SaveState::RawBytes::dataAvailable() const {
	return dataExists;
}

extern "C" {
	int my_minizip(char ** savefile,char ** savefile2);
	int my_miniunz(char ** savefile, const char * savefile2, const char * savedir);
}

void SaveState::save(size_t slot) { //throw (Error)
	if (slot >= SLOT_COUNT)  return;
	SDL_PauseAudio(0);
	bool save_err=false;
	extern unsigned int MEM_TotalPages(void);
	if((MEM_TotalPages()*4096/1024/1024)>200) {
		LOG_MSG("Stopped. 200 MB is the maximum memory size for saving/loading states.");
		return;
	}
	bool create_title=false;
	bool create_memorysize=false;
	//bool create_mute=false;
	extern const char* RunningProgram;
	std::string path;
	bool Get_Custom_SaveDir(std::string& savedir);
	if(Get_Custom_SaveDir(path)) {
		path+=CROSS_FILESPLIT;
	} else {
		// check if dosbox.conf exists in current directory
		extern std::string capturedir;
		//std::string path;
		const size_t last_slash_idx = capturedir.find_last_of("\\/");
		if (std::string::npos != last_slash_idx) {
			path = capturedir.substr(0, last_slash_idx);
		} else {
			path = ".";
		}
		path+=CROSS_FILESPLIT;
		path+="save";
		Cross::CreateDir(path);
		path+=CROSS_FILESPLIT;
	}

	std::string temp, save2;
	std::stringstream slotname;
	slotname << slot+1;
	temp=path;
	std::string save=temp+slotname.str()+".sav";
	remove(save.c_str());
	std::ofstream file (save.c_str());
	file << "";
	file.close();
	try {
		for (CompEntry::iterator i = components.begin(); i != components.end(); ++i) {
			std::ostringstream ss;
			i->second.comp.getBytes(ss);
			i->second.rawBytes[slot].set(ss.str());

			if(!create_title) {
				std::string tempname = temp+"Program_Name";
				std::ofstream programname (tempname.c_str(), std::ofstream::binary);
				programname << RunningProgram;
				create_title=true;
				programname.close();
			}

			if(!create_memorysize) {
                 std::string tempname = temp+"Memory_Size";
				  std::ofstream memorysize (tempname.c_str(), std::ofstream::binary);
				  memorysize << MEM_TotalPages();
				  create_memorysize=true;
				  memorysize.close();
			}
/*
			if (!create_mute) {
				std::string tempname = temp + "Mute";
				std::ofstream mute(tempname.c_str(), std::ofstream::binary);
				if (SDL_GetAudioStatus() == SDL_AUDIO_PAUSED)
					mute << "PAUSED";
				else
					mute << "NOT PAUSED";
				create_mute = true;
				mute.close();
			}
*/
			std::string realtemp;
			realtemp = temp + i->first;
			std::ofstream outfile (realtemp.c_str(), std::ofstream::binary);
			//if(i->first == "Mixer" || i->first == "Midi")
			//	outfile << ss.str(); // fixes crash in Stargunner
			//else
				outfile << (Util::compress(ss.str()));
			//compress all other saved states except position "slot"
			//const std::vector<RawBytes>& rb = i->second.rawBytes;
			//std::for_each(rb.begin(), rb.begin() + slot, std::mem_fun_ref(&RawBytes::compress));
			//std::for_each(rb.begin() + slot + 1, rb.end(), std::mem_fun_ref(&RawBytes::compress));
			outfile.close();
			ss.clear();
			if(outfile.fail()) {
				LOG_MSG("Save failed! - %s", realtemp.c_str());
				save_err=true;
				remove(save.c_str());
				goto delete_all;
			}
		}
	}
	catch (const std::bad_alloc&) {
		LOG_MSG("Save failed! Out of Memory!");
		save_err=true;
		remove(save.c_str());
		goto delete_all;
	}

	for (CompEntry::iterator i = components.begin(); i != components.end(); ++i) {
		save2=temp+i->first;
		my_minizip((char **)save.c_str(), (char **)save2.c_str());
	}
	save2=temp+"Program_Name";
	my_minizip((char **)save.c_str(), (char **)save2.c_str());
	save2=temp+"Memory_Size";
	my_minizip((char **)save.c_str(), (char **)save2.c_str());

delete_all:
	for (CompEntry::iterator i = components.begin(); i != components.end(); ++i) {
		save2=temp+i->first;
		remove(save2.c_str());
	}
	save2=temp+"Program_Name";
	remove(save2.c_str());
	save2=temp+"Memory_Size";
	remove(save2.c_str());
	if (!save_err) LOG_MSG("Saved. (Slot %d)",slot+1);
}

void SaveState::load(size_t slot) const { //throw (Error)
//	if (isEmpty(slot)) return;
	extern unsigned int MEM_TotalPages(void);
	bool load_err=false;
	if((MEM_TotalPages()*4096/1024/1024)>200) {
		LOG_MSG("Stopped. 200 MB is the maximum memory size for saving/loading states.");
		return;
	}
	SDL_PauseAudio(0);
	extern const char* RunningProgram;
	bool read_title=false;
	bool read_memorysize=false;
	//bool read_mute = false;
	std::string path;
	bool Get_Custom_SaveDir(std::string& savedir);
	if(Get_Custom_SaveDir(path)) {
		path+=CROSS_FILESPLIT;
	} else {
		extern std::string capturedir;
		//std::string path;
		const size_t last_slash_idx = capturedir.find_last_of("\\/");
		if (std::string::npos != last_slash_idx) {
			path = capturedir.substr(0, last_slash_idx);
		} else {
			path = ".";
		}
		path += CROSS_FILESPLIT;
		path +="save";
		path += CROSS_FILESPLIT;
	}
	std::string temp;
	temp = path;
	std::stringstream slotname;
	slotname << slot+1;
	std::string save=temp+slotname.str()+".sav";
	std::ifstream check_slot;
	check_slot.open(save.c_str(), std::ifstream::in);
	if(check_slot.fail()) {
		LOG_MSG("No saved slot - %d (%s)",slot+1,save.c_str());
		load_err=true;
		return;
	}

	for (CompEntry::const_iterator i = components.begin(); i != components.end(); ++i) {
		std::filebuf * fb;
		std::ifstream ss;
		std::ifstream check_file;
		fb = ss.rdbuf();

		my_miniunz((char **)save.c_str(),i->first.c_str(),temp.c_str());

		if(!read_title) {
			my_miniunz((char **)save.c_str(),"Program_Name",temp.c_str());
			std::ifstream check_title;
			int length = 8;

			std::string tempname = temp+"Program_Name";
			check_title.open(tempname.c_str(), std::ifstream::in);
			if(check_title.fail()) {
				LOG_MSG("Save state corrupted! Program in inconsistent state! - Program_Name");
				load_err=true;
				goto delete_all;
			}
			check_title.seekg (0, std::ios::end);
			length = check_title.tellg();
			check_title.seekg (0, std::ios::beg);

			char * const buffer = (char*)alloca( (length+1) * sizeof(char)); // char buffer[length];
			check_title.read (buffer, length);
			check_title.close();
			if(strncmp(buffer,RunningProgram,length)) {
				buffer[length]='\0';
				LOG_MSG("Aborted. Check your program name: %s",buffer);
				load_err=true;
				goto delete_all;
			}
			read_title=true;
		}

		if(!read_memorysize) {
			my_miniunz((char **)save.c_str(),"Memory_Size",temp.c_str());
			std::fstream check_memorysize;
			int length = 8;

			std::string tempname = temp+"Memory_Size";
			check_memorysize.open(tempname.c_str(), std::ifstream::in);
			if(check_memorysize.fail()) {
				LOG_MSG("Save state corrupted! Program in inconsistent state! - Memory_Size");
				load_err=true;
				goto delete_all;
			}
			check_memorysize.seekg (0, std::ios::end);
			length = check_memorysize.tellg();
			check_memorysize.seekg (0, std::ios::beg);

			char * const buffer = (char*)alloca( (length+1) * sizeof(char)); // char buffer[length];
			check_memorysize.read (buffer, length);
			check_memorysize.close();
			char str[10];
			itoa(MEM_TotalPages(), str, 10);
			if(strncmp(buffer,str,length)) {
				buffer[length]='\0';
				LOG_MSG("Aborted. Check your memory size.");
				load_err=true;
				goto delete_all;
			}
			read_memorysize=true;
		}
/*
		if (!read_mute) {
			my_miniunz((char **) save.c_str(), "Mute", temp.c_str());
			std::fstream check_mute;
			int length = 8;

			std::string tempname = temp + "Mute";
			check_mute.open(tempname.c_str(), std::ifstream::in);
			if (check_mute.fail()) {
				LOG_MSG("Save state corrupted! Program in inconsistent state! - Memory_Size");
				load_err = true;
				goto delete_all;
			}
			check_mute.seekg(0, std::ios::end);
			length = check_mute.tellg();
			check_mute.seekg(0, std::ios::beg);

			char * const buffer = (char*) alloca((length + 1) * sizeof(char) ); // char buffer[length];
			check_mute.read(buffer, length);
			check_mute.close();
			if (strncmp(buffer, "PAUSED", length)) {
				buffer[length] = '\0';
				if (SDL_GetAudioStatus() != SDL_AUDIO_PAUSED) SDL_PauseAudio(1);
			} else if (strncmp(buffer, "NOT PAUSED", length)) {
				buffer[length] = '\0';
				if (SDL_GetAudioStatus() == SDL_AUDIO_PAUSED) SDL_PauseAudio(0);
			} else {
				LOG_MSG("Save state corrupted! Program in inconsistent state! - Mute");
				load_err = true;
				goto delete_all;
			}
			read_mute = true;
		}
*/
		std::string realtemp;
		realtemp = temp + i->first;
		check_file.open(realtemp.c_str(), std::ifstream::in);
		check_file.close();
		if(check_file.fail()) {
			LOG_MSG("Save state corrupted! Program in inconsistent state! - %s",i->first.c_str());
			load_err=true;
			goto delete_all;
		}

		fb->open(realtemp.c_str(),std::ios::in | std::ios::binary);
		std::string str((std::istreambuf_iterator<char>(ss)), std::istreambuf_iterator<char>());
		std::stringstream mystream;
		//if(i->first == "Mixer" || i->first == "Midi") mystream << str; // fixes crash in Stargunner
		//else
		mystream << (Util::decompress(str));
		i->second.comp.setBytes(mystream);
		if (mystream.rdbuf()->in_avail() != 0 || mystream.eof()) { //basic consistency check
			LOG_MSG("Save state corrupted! Program in inconsistent state! - %s",i->first.c_str());
			load_err=true;
			goto delete_all;
		}
		//compress all other saved states except position "slot"
		//const std::vector<RawBytes>& rb = i->second.rawBytes;
		//std::for_each(rb.begin(), rb.begin() + slot, std::mem_fun_ref(&RawBytes::compress));
		//std::for_each(rb.begin() + slot + 1, rb.end(), std::mem_fun_ref(&RawBytes::compress));
		fb->close();
		mystream.clear();
	}
delete_all:
	std::string save2;
	for (CompEntry::const_iterator i = components.begin(); i != components.end(); ++i) {
		save2=temp+i->first;
		//LOG_MSG("Removing cache... %s",save2.c_str());
		remove(save2.c_str());
	}
	save2=temp+"Program_Name";
	remove(save2.c_str());
	save2=temp+"Memory_Size";
	remove(save2.c_str());
	if (!load_err) LOG_MSG("Loaded. (Slot %d)",slot+1);
}

bool SaveState::isEmpty(size_t slot) const {
	if (slot >= SLOT_COUNT) return true;
	return (components.empty() || !components.begin()->second.rawBytes[slot].dataAvailable());
}
