/* Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011, 2012, 2013 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
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

#include "mt32emu.h"
#include "FileStream.h"

namespace MT32Emu {

using std::ifstream;
using std::ios_base;

FileStream::FileStream() {
	ifsp = new ifstream();
}

FileStream::~FileStream() {
	if (ifsp != NULL) {
		delete ifsp; // destructor closes the file itself
	}
	if (data) {
		delete[] data;
	}
}

size_t FileStream::getSize() {
	if (fileSize != 0) {
		return fileSize;
	}
	if (ifsp == NULL) {
		return 0;
	}
	if (!ifsp->is_open()) {
		return 0;
	}
	ifsp->seekg(0, ios_base::end);
	fileSize = ifsp->tellg();
	return fileSize;
}

const unsigned char* FileStream::getData() {
	if (data != NULL) {
		return data;
	}
	if (ifsp == NULL) {
		return NULL;
	}
	if (!ifsp->is_open()) {
		return NULL;
	}
	if (getSize() == 0) {
		return NULL;
	}
	data = new unsigned char[fileSize];
	if (data == NULL) {
		return NULL;
	}
	ifsp->seekg(0);
	ifsp->read((char *)data, fileSize);
	if ((size_t)ifsp->tellg() != fileSize) {
		delete[] data;
		data = NULL;
		return NULL;
	}
	return data;
}

bool FileStream::open(const char *filename) {
	if (ifsp) {
		ifsp->open(filename, ios_base::in | ios_base::binary);
	}
	return (ifsp->good());
}

void FileStream::close() {
	ifsp->close();
}

}
