#include "save_state.h"
#include "cross.h"
#include "logging.h"
#include <malloc.h>
#include <cstring>
#include <fstream>

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <stdlib.h>
#include "SDL.h"

SaveState& SaveState::instance() {
    static SaveState singleton;
    return singleton;
}

void SaveState::registerComponent(const std::string& uniqueName, Component& comp) {
}

namespace Util {
std::string compress(const std::string& input) { //throw (SaveState::Error)
	return "";
}

std::string decompress(const std::string& input) { //throw (SaveState::Error)
	return "";
}
}

inline void SaveState::RawBytes::set(const std::string& stream) {
}

inline std::string SaveState::RawBytes::get() const { //throw (Error){
	return "";
}

inline void SaveState::RawBytes::compress() const { //throw (Error)
}

inline bool SaveState::RawBytes::dataAvailable() const {
	return false;
}

void SaveState::save(size_t slot) { //throw (Error)
}

void SaveState::load(size_t slot) const { //throw (Error)
}

bool SaveState::isEmpty(size_t slot) const {
	return true;
}
