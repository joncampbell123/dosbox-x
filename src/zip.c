
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "zlib.h"
#include "zip.h"

int LoadCentralDirectoryRecord(void* pziinit) {
  return 0;
}

extern zipFile ZEXPORT zipOpen3 (const void *pathname, int append, zipcharpc* globalcomment, zlib_filefunc64_32_def* pzlib_filefunc64_32_def)
{
    return NULL;
}

extern zipFile ZEXPORT zipOpen2 (const char *pathname, int append, zipcharpc* globalcomment, zlib_filefunc_def* pzlib_filefunc32_def)
{
    return NULL;
}

extern zipFile ZEXPORT zipOpen2_64 (const void *pathname, int append, zipcharpc* globalcomment, zlib_filefunc64_def* pzlib_filefunc_def)
{
    return NULL;
}

extern zipFile ZEXPORT zipOpen (const char* pathname, int append)
{
    return NULL;
}

extern zipFile ZEXPORT zipOpen64 (const void* pathname, int append)
{
    return NULL;
}

int Write_LocalFileHeader(void* zi, const char* filename, uInt size_extrafield_local, const void* extrafield_local)
{
    return -1;
}

extern int ZEXPORT zipOpenNewFileInZip4_64 (zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                         const void* extrafield_local, uInt size_extrafield_local,
                                         const void* extrafield_global, uInt size_extrafield_global,
                                         const char* comment, int method, int level, int raw,
                                         int windowBits,int memLevel, int strategy,
                                         const char* password, uLong crcForCrypting,
                                         uLong versionMadeBy, uLong flagBase, int zip64)
{
    return -1;
}

extern int ZEXPORT zipOpenNewFileInZip4 (zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                         const void* extrafield_local, uInt size_extrafield_local,
                                         const void* extrafield_global, uInt size_extrafield_global,
                                         const char* comment, int method, int level, int raw,
                                         int windowBits,int memLevel, int strategy,
                                         const char* password, uLong crcForCrypting,
                                         uLong versionMadeBy, uLong flagBase)
{
    return -1;
}

extern int ZEXPORT zipOpenNewFileInZip3 (zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                         const void* extrafield_local, uInt size_extrafield_local,
                                         const void* extrafield_global, uInt size_extrafield_global,
                                         const char* comment, int method, int level, int raw,
                                         int windowBits,int memLevel, int strategy,
                                         const char* password, uLong crcForCrypting)
{
    return -1;
}

extern int ZEXPORT zipOpenNewFileInZip3_64(zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                         const void* extrafield_local, uInt size_extrafield_local,
                                         const void* extrafield_global, uInt size_extrafield_global,
                                         const char* comment, int method, int level, int raw,
                                         int windowBits,int memLevel, int strategy,
                                         const char* password, uLong crcForCrypting, int zip64)
{
    return -1;
}

extern int ZEXPORT zipOpenNewFileInZip2(zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                        const void* extrafield_local, uInt size_extrafield_local,
                                        const void* extrafield_global, uInt size_extrafield_global,
                                        const char* comment, int method, int level, int raw)
{
    return -1;
}

extern int ZEXPORT zipOpenNewFileInZip2_64(zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                        const void* extrafield_local, uInt size_extrafield_local,
                                        const void* extrafield_global, uInt size_extrafield_global,
                                        const char* comment, int method, int level, int raw, int zip64)
{
    return -1;
}

extern int ZEXPORT zipOpenNewFileInZip64 (zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                        const void* extrafield_local, uInt size_extrafield_local,
                                        const void*extrafield_global, uInt size_extrafield_global,
                                        const char* comment, int method, int level, int zip64)
{
    return -1;
}

extern int ZEXPORT zipOpenNewFileInZip (zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                        const void* extrafield_local, uInt size_extrafield_local,
                                        const void*extrafield_global, uInt size_extrafield_global,
                                        const char* comment, int method, int level)
{
    return -1;
}

extern int ZEXPORT zipWriteInFileInZip (zipFile file,const void* buf,unsigned int len)
{
    return -1;
}

extern int ZEXPORT zipCloseFileInZipRaw (zipFile file, uLong uncompressed_size, uLong crc32)
{
    return -1;
}

extern int ZEXPORT zipCloseFileInZipRaw64 (zipFile file, ZPOS64_T uncompressed_size, uLong crc32)
{
    return -1;
}

extern int ZEXPORT zipCloseFileInZip (zipFile file)
{
    return -1;
}

int Write_Zip64EndOfCentralDirectoryLocator(void* zi, ZPOS64_T zip64eocd_pos_inzip)
{
    return -1;
}

int Write_Zip64EndOfCentralDirectoryRecord(void* zi, uLong size_centraldir, ZPOS64_T centraldir_pos_inzip)
{
    return -1;
}
int Write_EndOfCentralDirectoryRecord(void* zi, uLong size_centraldir, ZPOS64_T centraldir_pos_inzip)
{
    return -1;
}

int Write_GlobalComment(void* zi, const char* global_comment)
{
    return -1;
}

extern int ZEXPORT zipClose (zipFile file, const char* global_comment)
{
    return -1;
}

extern int ZEXPORT zipRemoveExtraInfoBlock (char* pData, int* dataLen, short sHeader)
{
  return -1;
}

