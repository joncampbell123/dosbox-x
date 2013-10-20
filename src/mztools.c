
#include "zlib.h"

extern int ZEXPORT unzRepair(file, fileOut, fileOutTmp, nRecovered, bytesRecovered)
const char* file;
const char* fileOut;
const char* fileOutTmp;
uLong* nRecovered;
uLong* bytesRecovered;
{
  return Z_STREAM_ERROR;
}
