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

#include <cstdio>
#include "mt32emu.h"
#include "sha1/sha1.h"

namespace MT32Emu {

static void SHA1DigestToString(char *strDigest, const unsigned int intDigest[]) {
	sprintf(strDigest, "%08x%08x%08x%08x%08x", intDigest[0], intDigest[1], intDigest[2], intDigest[3], intDigest[4]);
}

File::File() : sha1DigestCalculated(false), fileSize(0), data(NULL) {
	sha1DigestCalculated = false;
}

const char *File::getSHA1() {
	if (sha1DigestCalculated) {
		return sha1Digest;
	}
	sha1Digest[0] = 0;
	sha1DigestCalculated = true;

	if (getData() == NULL) {
		return sha1Digest;
	}

	SHA1 sha1;
	unsigned int fileDigest[5];

	sha1.Input(data, fileSize);
	if (sha1.Result(fileDigest)) {
		SHA1DigestToString(sha1Digest, fileDigest);
	}
	return sha1Digest;
}

}
