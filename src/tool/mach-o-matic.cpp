
/* Tool for Mac OS X builds to patch .dylib files so they always load relative
   to the main executable. This was once done using XCode tool install_name_tool
   but that tool succumbed to a debilitating neurotic break due to excess worrying
   over whether or not a __LINKEDIT segment is completely occupied by link edit
   data. So to continue enabling Mac OS releases, we now use this tool which does
   the one thing we need it to do: Change .dylib references in MachO binaries
   from absolute paths to @executable_path@ relative references so they can be
   bundled in the DOSBox-X app bundle without requiring the user to install any
   additional software. */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include <mach-o/loader.h> /* Apple headers */

#include <string>

using namespace std;

string get_macho_lcstr(union lc_str str,const uint8_t *base,const uint8_t *fence) {
    string r;

    if (str.offset <= (uint32_t)(fence-base)) {
        base += str.offset;
        assert(base <= fence);

        while (base < fence && *base != 0)
            r += *base++;
    }

    return r;
}

int main(int argc,char **argv) {
    std::string fpath;
    struct stat st;
    size_t src_mmap_sz;
    const uint8_t *src_mmap;
    const uint8_t *src_mmap_fence;
    const uint8_t *src_scan;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    int src_bits;
    int src_fd;

    if (argc < 2) {
        fprintf(stderr,"mach-o-matic <Mach-O executable or dylib>\n");
        return 1;
    }

    fpath = argv[1];
    if (lstat(fpath.c_str(),&st) != 0) {
        fprintf(stderr,"Cannot stat %s\n",fpath.c_str());
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr,"%s is not a file\n",fpath.c_str());
        return 1;
    }

    if ((src_fd=open(fpath.c_str(),O_RDONLY)) < 0) {
        fprintf(stderr,"Cannot open %s\n",fpath.c_str());
        return 1;
    }

    if (st.st_size < 1024) {
        fprintf(stderr,"%s is too small\n",fpath.c_str());
        return 1;
    }
    if (st.st_size > (512*1024*1024)) { // If your dylibs are that large you have a bloat problem
        fprintf(stderr,"%s is too large\n",fpath.c_str());
        return 1;
    }
    src_mmap_sz = (size_t)((st.st_size + 0xFFFll) & ~0xFFFll); /* round up */

    if ((src_mmap=(const uint8_t*)mmap(NULL,src_mmap_sz,PROT_READ,MAP_SHARED,src_fd,0)) == MAP_FAILED) {
        fprintf(stderr,"Failed to mmap %s\n",fpath.c_str());
        return 1;
    }
    src_mmap_fence = src_mmap + st.st_size;
    src_scan = src_mmap;

    if (*((uint32_t*)src_scan) == MH_MAGIC) {
        src_bits = 32;
        fprintf(stderr,"%s: 32-bit Mach-O patching not yet supported\n",fpath.c_str());
        return 1;
    }
    else if (*((uint32_t*)src_scan) == MH_MAGIC_64) {
        const mach_header_64 *hdr = (const mach_header_64*)src_scan;
        src_scan += sizeof(mach_header_64);
        assert(src_scan <= src_mmap_fence);

        src_bits = 64;
        cputype = hdr->cputype;
        cpusubtype = hdr->cpusubtype;
        filetype = hdr->filetype;
        ncmds = hdr->ncmds;
        sizeofcmds = hdr->sizeofcmds;
        flags = hdr->flags;
    }
    else {
        fprintf(stderr,"%s: Unknown signature\n",fpath.c_str());
        return 1;
    }

#if 1
    fprintf(stderr,"Mach-O header:\n");
    fprintf(stderr,"    cputype:        0x%08lx\n",(unsigned long)cputype);
    fprintf(stderr,"    cpusubtype:     0x%08lx\n",(unsigned long)cpusubtype);
    fprintf(stderr,"    filetype:       0x%08lx\n",(unsigned long)filetype);
    fprintf(stderr,"    ncmds:          %lu\n",(unsigned long)ncmds);
    fprintf(stderr,"    sizeofcmds:     %lu\n",(unsigned long)sizeofcmds);
    fprintf(stderr,"    flags:          0x%08lx\n",(unsigned long)flags);
#endif

    if (!(filetype == MH_EXECUTE || filetype == MH_DYLIB)) {
        fprintf(stderr,"This tool is intended for use with executables or dylib files\n");
        return 1;
    }

    /* read load commands */
    const uint8_t *load_cmd_fence = src_scan + sizeofcmds;
    while ((src_scan+8) <= load_cmd_fence) {
        const struct load_command *cmd = (const struct load_command*)src_scan;
        if (cmd->cmdsize < 8) {
            fprintf(stderr,"Load command with too-small size\n");
            return 1;
        }
        if (cmd->cmdsize > sizeofcmds) {
            fprintf(stderr,"Load command with excessive size\n");
            return 1;
        }
        src_scan += cmd->cmdsize;
        if (src_scan > load_cmd_fence) {
            fprintf(stderr,"Load command exceeds sizeofcmds\n");
            return 1;
        }

        if ((src_bits == 32 && (cmd->cmdsize & 3) != 0) ||
            (src_bits == 64 && (cmd->cmdsize & 7) != 0)) {
            fprintf(stderr,"Load command size does not meet Apple alignment requirements\n");
            return 1;
        }

#if 1
        fprintf(stderr,"Load command:\n");
        fprintf(stderr,"    cmd:         0x%08lx\n",(unsigned long)cmd->cmd);
        fprintf(stderr,"    cmdsize:     %lu\n",(unsigned long)cmd->cmdsize);
#endif

        if (cmd->cmd == LC_ID_DYLIB || cmd->cmd == LC_LOAD_DYLIB || cmd->cmd == LC_LOAD_WEAK_DYLIB || cmd->cmd == LC_REEXPORT_DYLIB) {
            const struct dylib_command *dycmd = (const struct dylib_command*)cmd;
            if (cmd->cmdsize < sizeof(*dycmd)) continue;

#if 1
            switch (cmd->cmd) {
                case LC_ID_DYLIB:           fprintf(stderr,"    LC_ID_DYLIB:\n"); break;
                case LC_LOAD_DYLIB:         fprintf(stderr,"    LC_LOAD_DYLIB:\n"); break;
                case LC_LOAD_WEAK_DYLIB:    fprintf(stderr,"    LC_LOAD_WEAK_DYLIB:\n"); break;
                case LC_REEXPORT_DYLIB:     fprintf(stderr,"    LC_REEXPORT_DYLIB:\n"); break;
                default:                    abort(); break;
            }
#endif

            string name = get_macho_lcstr(dycmd->dylib.name,(const uint8_t*)cmd,(const uint8_t*)src_scan);

#if 1
            fprintf(stderr,"        name:                   %s\n",name.c_str());
            fprintf(stderr,"        timestamp:              %lu\n",(unsigned long)dycmd->dylib.timestamp);
            fprintf(stderr,"        current_version:        0x%08lx\n",(unsigned long)dycmd->dylib.current_version);
            fprintf(stderr,"        compatibility_version:  0x%08lx\n",(unsigned long)dycmd->dylib.compatibility_version);
#endif
        }
    }

    munmap((void*)src_mmap,src_mmap_sz);
    close(src_fd);
    return 0;
}

