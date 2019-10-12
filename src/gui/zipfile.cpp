
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <algorithm> // std::transform
#ifdef WIN32
# include <signal.h>
# include <sys/stat.h>
# include <process.h>
#endif

#include "dosbox.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"
#include "bios.h"
#include "support.h"
#include "debug.h"
#include "ide.h"
#include "bitop.h"
#include "ptrop.h"
#include "mapper.h"
#include "zipfile.h"

#include "mapper.h"
#include "vga.h"
#include "keyboard.h"
#include "cpu.h"
#include "fpu.h"
#include "cross.h"
#include "keymap.h"

static std::string          zip_nv_pair_empty;

static char                 zip_nv_tmp[1024];

bool ZIPFileEntry::rewind(void) {
    if (can_write) return false;
    return (seek_file(0) == 0);
}

off_t ZIPFileEntry::seek_file(off_t pos) {
    if (file == NULL || file_offset == (off_t)0 || can_write) return (off_t)(-1LL);
    if (pos < (off_t)0) pos = (off_t)0;
    if (pos > file_length) pos = file_length;
    pos = file->seek_file(pos + file_offset) - file_offset;
    if (pos < 0 || pos > file_length) return (off_t)(-1LL);
    position = pos;
    return pos;
}

int ZIPFileEntry::read(void *buffer,size_t count) {
    if (file == NULL || file_offset == (off_t)0) return -1;
    if (position >= file_length) return 0;

    size_t mread = (size_t)file_length - (size_t)position;
    if (mread > count) mread = count;

    if (mread > 0) {
        if (seek_file(position) != position) return -1;
        int r = file->read(buffer,mread);
        if (r > 0) position += (off_t)r;
        return r;
    }

    return (int)mread;
}

int ZIPFileEntry::write(const void *buffer,size_t count) {
    if (file == NULL || file_offset == (off_t)0 || !can_write) return -1;

    /* write stream only, no seeking.
     * this code assumes the file pointer will not change anywhere else,
     * and always to the end */
    if (count > size_t(0)) {
        int r = file->write(buffer,count);
        if (r > 0) {
            position += (off_t)r;
            write_crc = zipcrc_update(write_crc, buffer, size_t(r));
            file_length = position;
        }
        return r;
    }

    return (int)count;
}

ZIPFile::ZIPFile() {
}

ZIPFile::~ZIPFile() {
    close();
}

void ZIPFile::close(void) {
    if (file_fd >= 0) {
        ::close(file_fd);
        file_fd = -1;
    }

    entries.clear();
}

ZIPFileEntry *ZIPFile::get_entry(const char *name) {
    if (file_fd < 0) return NULL;

    /* no reading while writing except what is being written */
    if (!current_entry.empty() && current_entry != name) return NULL;

    /* no empty names */
    if (*name == 0) return NULL;

    auto i = entries.find(name);
    if (i == entries.end()) return NULL;

    return &(i->second);
}

ZIPFileEntry *ZIPFile::new_entry(const char *name) {
    if (file_fd < 0 || !can_write || wrote_trailer) return NULL;

    /* cannot make new entries that exist already */
    auto i = entries.find(name);
    if (i != entries.end()) return NULL;

    /* no empty names */
    if (*name == 0) return NULL;

    /* close current entry, if open */
    close_current();

    /* begin new entry at end */
    current_entry = name;
    write_pos = end_of_file();

    ZIPFileEntry *ent = &entries[name];
    ent->name = name;
    ent->can_write = true;
    ent->file_header_offset = write_pos;
    write_pos += (off_t)(sizeof(ZIPLocalFileHeader) + ent->name.length());
    ent->write_crc = zipcrc_init();
    ent->file_offset = write_pos;
    ent->file = this;

    if (seek_file(ent->file_header_offset) != ent->file_header_offset) {
        close_current();
        return NULL;
    }

    ZIPLocalFileHeader hdr;
    memset(&hdr,0,sizeof(hdr));
    hdr.local_file_header_signature = htole32(0x04034b50);  /* PK\x03\x04 */
    hdr.version_needed_to_extract = htole16(20);            /* PKZIP 2.0 */
    hdr.general_purpose_bit_flag = htole16(0 << 1);
    hdr.compression_method = 0;                             /* store (no compression) */
    hdr.file_name_length = htole16((uint16_t)ent->name.length());
    if (write(&hdr,sizeof(hdr)) != sizeof(hdr)) {
        close_current();
        return NULL;
    }
    assert(ent->name.length() != 0);
    if ((size_t)write(ent->name.c_str(),ent->name.length()) != ent->name.length()) {
        close_current();
        return NULL;
    }
    if (seek_file(ent->file_offset) != ent->file_offset) {
        close_current();
        return NULL;
    }

    return ent;
}

off_t ZIPFile::end_of_file(void) {
    return lseek(file_fd,0,SEEK_END);
}

void ZIPFile::close_current(void) {
    if (!can_write) return;

    if (!current_entry.empty()) {
        ZIPFileEntry *ent = get_entry(current_entry.c_str());
        ZIPLocalFileHeader hdr;

        if (ent != NULL && ent->can_write) {
            ent->can_write = false;

            if (seek_file(ent->file_header_offset) == ent->file_header_offset && read(&hdr,sizeof(hdr)) == sizeof(hdr)) {
                hdr.compressed_size = hdr.uncompressed_size = htole32(((uint32_t)ent->file_length));
                hdr.crc_32 = htole32(zipcrc_finalize(ent->write_crc));

                if (seek_file(ent->file_header_offset) == ent->file_header_offset && write(&hdr,sizeof(hdr)) == sizeof(hdr)) {
                    /* good */
                }
            }
        }
    }

    current_entry.clear();
}

int ZIPFile::open(const char *path,int mode) {
    close();

    if (path == NULL) return -1;

    if ((mode & 3) == O_WRONLY) {
        LOG_MSG("WARNING: ZIPFile attempt to open with O_WRONLY, which will not work");
        return -1;
    }

#if defined(O_BINARY)
    mode |= O_BINARY;
#endif

    file_fd = ::open(path,mode,0644);
    if (file_fd < 0) return -1;
    if (lseek(file_fd,0,SEEK_SET) != 0) {
        close();
        return -1;
    }

    entries.clear();
    current_entry.clear();
    wrote_trailer = false;
    write_pos = 0;

    /* WARNING: This assumes O_RDONLY, O_WRONLY, O_RDWR are defined as in Linux (0, 1, 2) in the low two bits */
    if ((mode & 3) == O_RDWR)
        can_write = true;
    else
        can_write = false;

    /* if we're supposed to READ the ZIP file, then start scanning now */
    if ((mode & 3) == O_RDONLY) {
        unsigned char tmp[512];
        struct pkzip_central_directory_header_main chdr;
        struct pkzip_central_directory_header_end ehdr;

        off_t fsz = end_of_file();

        /* check for 'PK' at the start of the file.
         * This code only expects to handle the ZIP files it generated, not ZIP files in general. */
        if (fsz < 64 || seek_file(0) != 0 || read(tmp,4) != 4 || memcmp(tmp,"PK\x03\x04",4) != 0) {
            LOG_MSG("Not a PKZIP file");
            close();
            return -1;
        }

        /* then look for the central directory at the end.
         * this code DOES NOT SUPPORT the ZIP comment field, nor will this code generate one. */
        if (seek_file(fsz - (off_t)sizeof(ehdr)) != (fsz - (off_t)sizeof(ehdr)) || (size_t)read(&ehdr,sizeof(ehdr)) != sizeof(ehdr) || ehdr.sig != PKZIP_CENTRAL_DIRECTORY_END_SIG || ehdr.size_of_central_directory > 0x100000u/*absurd size*/ || ehdr.offset_of_central_directory_from_start_disk == 0 || (off_t)ehdr.offset_of_central_directory_from_start_disk >= fsz) {
            LOG_MSG("Cannot locate Central Directory");
            close();
            return -1;
        }
        if (seek_file((off_t)ehdr.offset_of_central_directory_from_start_disk) != (off_t)ehdr.offset_of_central_directory_from_start_disk) {
            LOG_MSG("Cannot locate Central Directory #2");
            close();
            return -1;
        }

        /* read the central directory */
        {
            long remain = (long)ehdr.size_of_central_directory;

            while (remain >= (long)sizeof(struct pkzip_central_directory_header_main)) {
                if (read(&chdr,sizeof(chdr)) != sizeof(chdr)) break;
                remain -= (long)sizeof(chdr);

                if (chdr.sig != PKZIP_CENTRAL_DIRECTORY_HEADER_SIG) break;
                if (chdr.filename_length >= sizeof(tmp)) break;

                tmp[chdr.filename_length] = 0;
                if (chdr.filename_length != 0) {
                    if (read(tmp,chdr.filename_length) != chdr.filename_length) break;
                    remain -= chdr.filename_length;
                }

                if (tmp[0] == 0) continue;

                ZIPFileEntry *ent = &entries[(char*)tmp];
                ent->can_write = false;
                ent->file_length = (off_t)htole32(chdr.uncompressed_size);
                ent->file_header_offset = (off_t)htole32(chdr.relative_offset_of_local_header);
                ent->file_offset = ent->file_header_offset + (off_t)sizeof(struct ZIPLocalFileHeader) + (off_t)htole16(chdr.filename_length) + (off_t)htole16(chdr.extra_field_length);
                ent->position = 0;
                ent->name = (char*)tmp;
                ent->file = this;
            }
        }
    }

    return 0;
}

off_t ZIPFile::seek_file(off_t pos) {
    if (file_fd < 0) return (off_t)(-1LL);
    return ::lseek(file_fd,pos,SEEK_SET);
}

int ZIPFile::read(void *buffer,size_t count) {
    if (file_fd < 0) return -1;
    return ::read(file_fd,buffer,(unsigned int)count);
}

int ZIPFile::write(const void *buffer,size_t count) {
    if (file_fd < 0) return -1;
    return ::write(file_fd,buffer,(unsigned int)count);
}

void ZIPFile::writeZIPFooter(void) {
    struct pkzip_central_directory_header_main chdr;
    struct pkzip_central_directory_header_end ehdr;
    uint32_t cdircount = 0;
    uint32_t cdirbytes = 0;
    off_t cdirofs = 0;

    if (file_fd < 0 || wrote_trailer || !can_write) return;

    close_current();
    cdirofs = end_of_file();

    for (auto i=entries.begin();i!=entries.end();i++) {
        const ZIPFileEntry &ent = i->second;

        memset(&chdr,0,sizeof(chdr));
        chdr.sig = htole32(PKZIP_CENTRAL_DIRECTORY_HEADER_SIG);
        chdr.version_made_by = htole16((0 << 8) + 20);      /* PKZIP 2.0 */
        chdr.version_needed_to_extract = htole16(20);       /* PKZIP 2.0 or higher */
        chdr.general_purpose_bit_flag = htole16(0 << 1);    /* just lie and say that "normal" deflate was used */
        chdr.compression_method = 0;                        /* stored (no compression) */
        chdr.last_mod_file_time = 0;
        chdr.last_mod_file_date = 0;
        chdr.compressed_size = htole32(((uint32_t)ent.file_length));
        chdr.uncompressed_size = htole32(((uint32_t)ent.file_length));
        chdr.filename_length = htole16((uint16_t)ent.name.length());
        chdr.disk_number_start = htole16(1u);
        chdr.internal_file_attributes = 0;
        chdr.external_file_attributes = 0;
        chdr.relative_offset_of_local_header = (uint32_t)htole32(ent.file_header_offset);
        chdr.crc32 = htole32(zipcrc_finalize(ent.write_crc));

        if (write(&chdr,sizeof(chdr)) != sizeof(chdr)) break;
        cdirbytes += sizeof(chdr);
        cdircount++;

        assert(ent.name.length() != 0);
        if ((size_t)write(ent.name.c_str(),ent.name.length()) != ent.name.length()) break;
        cdirbytes += (uint32_t)ent.name.length();
    }

    memset(&ehdr,0,sizeof(ehdr));
    ehdr.sig = htole32(PKZIP_CENTRAL_DIRECTORY_END_SIG);
    ehdr.number_of_disk_with_start_of_central_directory = htole16(0);
    ehdr.number_of_this_disk = htole16(0);
    ehdr.total_number_of_entries_of_central_dir_on_this_disk = htole16(cdircount);
    ehdr.total_number_of_entries_of_central_dir = htole16(cdircount);
    ehdr.size_of_central_directory = htole32(cdirbytes);
    ehdr.offset_of_central_directory_from_start_disk = (uint32_t)htole32(cdirofs);
    write(&ehdr,sizeof(ehdr));

    wrote_trailer = true;
    current_entry.clear();
}

zip_nv_pair_map::zip_nv_pair_map() {
}

zip_nv_pair_map::zip_nv_pair_map(ZIPFileEntry &ent) {
    read_nv_pairs(ent);
}

std::string &zip_nv_pair_map::get(const char *name) {
    auto i = find(name);
    if (i != end()) return i->second;
    return zip_nv_pair_empty;
}

bool zip_nv_pair_map::get_bool(const char *name) {
    std::string &val = get(name);
    return (strtol(val.c_str(),NULL,0) > 0);
}

long zip_nv_pair_map::get_long(const char *name) {
    std::string &val = get(name);
    return strtol(val.c_str(),NULL,0);
}

unsigned long zip_nv_pair_map::get_ulong(const char *name) {
    std::string &val = get(name);
    return strtoul(val.c_str(),NULL,0);
}

void zip_nv_pair_map::process_line(char *line/*will modify, assume caller has put NUL at the end*/) {
    char *equ = strchr(line,'=');
    if (equ == NULL) return;
    *equ++ = 0; /* overwite '=' with NUL, split name vs value */

    /* no null names */
    if (*line == 0) return;

    (*this)[line] = equ;
}

void zip_nv_pair_map::read_nv_pairs(ZIPFileEntry &ent) {
    char tmp[1024];
    char line[1024],*w,*wf=line+sizeof(line)-1;
    char c;
    int l;

    clear();
    ent.rewind();

    w = line;
    while ((l=ent.read(tmp,sizeof(tmp))) > 0) {
        char* r = tmp;
        char* f = tmp + l;

        while (r < f) {
            c = *r++;

            if (c == '\n') {
                assert(w <= wf);
                *w = 0;
                process_line(line);
                w = line;
            }
            else if (c == '\r') {
                /* ignore */
            }
            else if (w < wf) {
                *w++ = c;
            }
        }
    }

    if (w != line) {
        assert(w <= wf);
        *w = 0;
        process_line(line);
        w = line;
    }
}

void zip_nv_write(ZIPFileEntry &ent,const char *name,bool val) {
    size_t l;

    if ((l = ((size_t)snprintf(zip_nv_tmp,sizeof(zip_nv_tmp),"%s=%d\n",name,val?1:0))) >= (sizeof(zip_nv_tmp)-1u))
        E_Exit("zip_nv_write buffer overrun (result too long)");

    ent.write(zip_nv_tmp,l);
}

void zip_nv_write(ZIPFileEntry &ent,const char *name,long val) {
    size_t l;

    if ((l = ((size_t)snprintf(zip_nv_tmp,sizeof(zip_nv_tmp),"%s=%ld\n",name,val))) >= (sizeof(zip_nv_tmp)-1u))
        E_Exit("zip_nv_write buffer overrun (result too long)");

    ent.write(zip_nv_tmp,l);
}

void zip_nv_write_hex(ZIPFileEntry &ent,const char *name,unsigned long val) {
    size_t l;

    if ((l = ((size_t)snprintf(zip_nv_tmp,sizeof(zip_nv_tmp),"%s=0x%lx\n",name,val))) >= (sizeof(zip_nv_tmp)-1u))
        E_Exit("zip_nv_write buffer overrun (result too long)");

    ent.write(zip_nv_tmp,l);
}

