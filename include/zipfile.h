
#ifndef ZIPFILE_H
#define ZIPFILE_H

#include <stdint.h>

extern "C" {
#include "zipcrc.h"
}

#include <map>

#pragma pack(push,1)
struct ZIPLocalFileHeader { /* ZIP File Format Specification v6.3.4 sec 4.3.7 Local file header */
    uint32_t        local_file_header_signature;                /* +0x00 0x04034b50 */
    uint16_t        version_needed_to_extract;                  /* +0x04 */
    uint16_t        general_purpose_bit_flag;                   /* +0x06 */
    uint16_t        compression_method;                         /* +0x08 */
    uint16_t        last_mod_file_time;                         /* +0x0A */
    uint16_t        last_mod_file_date;                         /* +0x0C */
    uint32_t        crc_32;                                     /* +0x0E */
    uint32_t        compressed_size;                            /* +0x12 */
    uint32_t        uncompressed_size;                          /* +0x16 */
    uint16_t        file_name_length;                           /* +0x1A */
    uint16_t        extra_field_length;                         /* +0x1C */
};                                                              /* =0x1E */
#pragma pack(pop)

#pragma pack(push,1)
# define PKZIP_CENTRAL_DIRECTORY_HEADER_SIG (0x02014B50UL)

struct pkzip_central_directory_header_main { /* PKZIP APPNOTE 2.0: General Format of a ZIP file section C */
    uint32_t                sig;                            /* 4 bytes  +0x00 0x02014B50 = 'PK\x01\x02' */
    uint16_t                version_made_by;                /* 2 bytes  +0x04 version made by */
    uint16_t                version_needed_to_extract;      /* 2 bytes  +0x06 version needed to extract */
    uint16_t                general_purpose_bit_flag;       /* 2 bytes  +0x08 general purpose bit flag */
    uint16_t                compression_method;             /* 2 bytes  +0x0A compression method */
    uint16_t                last_mod_file_time;             /* 2 bytes  +0x0C */
    uint16_t                last_mod_file_date;             /* 2 bytes  +0x0E */
    uint32_t                crc32;                          /* 4 bytes  +0x10 */
    uint32_t                compressed_size;                /* 4 bytes  +0x14 */
    uint32_t                uncompressed_size;              /* 4 bytes  +0x18 */
    uint16_t                filename_length;                /* 2 bytes  +0x1C */
    uint16_t                extra_field_length;             /* 2 bytes  +0x1E */
    uint16_t                file_comment_length;            /* 2 bytes  +0x20 */
    uint16_t                disk_number_start;              /* 2 bytes  +0x22 */
    uint16_t                internal_file_attributes;       /* 2 bytes  +0x24 */
    uint32_t                external_file_attributes;       /* 4 bytes  +0x26 */
    uint32_t                relative_offset_of_local_header;/* 4 bytes  +0x2A */
};                                                          /*          =0x2E */
/* filename and extra field follow, then file data */
#pragma pack(pop)

#pragma pack(push,1)
# define PKZIP_CENTRAL_DIRECTORY_END_SIG    (0x06054B50UL)

struct pkzip_central_directory_header_end { /* PKZIP APPNOTE 2.0: General Format of a ZIP file section C */
    uint32_t                sig;                            /* 4 bytes  +0x00 0x06054B50 = 'PK\x05\x06' */
    uint16_t                number_of_this_disk;            /* 2 bytes  +0x04 */
    uint16_t                number_of_disk_with_start_of_central_directory;
                                                            /* 2 bytes  +0x06 */
    uint16_t                total_number_of_entries_of_central_dir_on_this_disk;
                                                            /* 2 bytes  +0x08 */
    uint16_t                total_number_of_entries_of_central_dir;
                                                            /* 2 bytes  +0x0A */
    uint32_t                size_of_central_directory;      /* 4 bytes  +0x0C */
    uint32_t                offset_of_central_directory_from_start_disk;
                                                            /* 4 bytes  +0x10 */
    uint16_t                zipfile_comment_length;         /* 2 bytes  +0x14 */
};                                                          /*          =0x16 */
/* filename and extra field follow, then file data */
#pragma pack(pop)

class ZIPFile;

class ZIPFileEntry {
public:
    bool        can_write = false;
    off_t       file_length = 0;
    off_t       file_offset = 0;
    off_t       file_header_offset = 0;
    off_t       position = 0;
    std::string name;
    ZIPFile*    file = NULL;
    zipcrc_t    write_crc = 0;
public:
    bool rewind(void);
    int read(void *buffer,size_t count);
    int write(const void *buffer,size_t count);
private: /* encourage code that uses this C++ class to stream-read so that
            this code can add deflate compression support later without pain and hacks */
    off_t seek_file(off_t pos);
};

class ZIPFile {
public:
    int         file_fd = -1;
    std::string filename;
    std::map<std::string,ZIPFileEntry> entries;
    off_t       write_pos = 0;
    bool        can_write = false;
    bool        wrote_trailer = false;
    std::string current_entry;      /* currently writing entry */
public:
    ZIPFile();
    ~ZIPFile();
public:
    void close(void);
    ZIPFileEntry *get_entry(const char *name);
    ZIPFileEntry *new_entry(const char *name);
    off_t end_of_file(void);
    void close_current(void);
    int open(const char *path,int mode);
    off_t seek_file(off_t pos);
    int read(void *buffer,size_t count);
    int write(const void *buffer,size_t count);
    void writeZIPFooter(void);
};

class zip_nv_pair_map : public std::map<std::string, std::string> {
public:
    zip_nv_pair_map();
    zip_nv_pair_map(ZIPFileEntry &ent);
public:
    std::string &get(const char *name);
    bool get_bool(const char *name);
    long get_long(const char *name);
    unsigned long get_ulong(const char *name);
    void process_line(char *line/*will modify, assume caller has put NUL at the end*/);
    void read_nv_pairs(ZIPFileEntry &ent);
};

void zip_nv_write(ZIPFileEntry &ent,const char *name,bool val);
void zip_nv_write(ZIPFileEntry &ent,const char *name,long val);
void zip_nv_write_hex(ZIPFileEntry &ent,const char *name,unsigned long val);
#endif //ZIPFILE_H

