#include "dosbox.h"

#if C_DEBUG && C_DEBUG_SERVER

#include "debug.h"
#include "logging.h"
#include "support.h"
#include "regs.h"
#include "fpu.h"
#include "paging.h"

#define MAX_REQUEST_SIZE 512

// From debug.c
extern int debugrunmode;
extern bool tohide;

// This structure controls various aspects of the serverr
struct DebugServer DEBUG_server;

/**
 * Enables the debug server
 *
 * This is called when the dosbox debugger gets enabled and will
 * spawn a new thread to handle incoming connections from the remote
 * debugger.
 */
void DEBUG_EnableServer() {
    // Initialize server control
    DEBUG_server.shutdown = false;

    // Start server thread
    DEBUG_ServerStartThread();
}

/**
 * Disables the debug server
 */
void DEBUG_ShutdownServer() {
    // Shutdown server thread
    DEBUG_ServerShutdownThread();
}

#define DEBUG_RUNMODE_DEBUG 0
#define DEBUG_RUNMODE_NORMAL 1
#define DEBUG_RUNMODE_WATCH 2
/**
 * Sets the debuggers active runmode
 */
static void DEBUG_SetRunmode(int mode) {
    debugrunmode = mode;
    if (IsDebuggerActive() || IsDebuggerRunwatch() || IsDebuggerRunNormal()) {
        tohide = false;
        DEBUG_Enable_Handler(true);
        tohide = true;
    }
}

static char* FormatBreakpoint(int nr, Breakpoint* bp) {
    char formatted[128];
    int offset = bp->offset;
    if (bp->type == Breakpoint::BP_INTERRUPT) {
        if (bp->GetValue() == Breakpoint::BPINT_ALL) {
            offset = 0 << 24 | bp->intNr << 16;
        }
        else if (bp->GetOther() == Breakpoint::BPINT_ALL) {
            offset = 1 << 24 | bp->intNr << 16 | bp->GetValue() << 8;
        }
        else {
            offset = 2 << 24 | bp->intNr << 16 | bp->GetValue() << 8 | bp->GetOther();
        }
    }
    else if (bp->type == Breakpoint::BP_MEMORY) {
        
        //DEBUG_ShowMsg("%02X. BPMEM %04X:%04X (%02X)\n", nr, bp->segment, bp->offset, bp->GetValue());
    }
    else if (bp->type == Breakpoint::BP_MEMORY_PROT) {
        
        //DEBUG_ShowMsg("%02X. BPPM %04X:%08X (%02X)\n", nr, bp->segment, bp->offset, bp->GetValue());
    }
    else if (bp->type == Breakpoint::BP_MEMORY_LINEAR) {
        //DEBUG_ShowMsg("%02X. BPLM %08X (%02X)\n", nr, bp->offset, bp->GetValue());
    }
    snprintf(formatted, sizeof(formatted) - 1,
        "%02x,%02x,%02x,%02x,%04x,%08x",
        nr,
        bp->type,
        bp->active,
        bp->once,
        bp->segment,
        bp->offset
    );
    return formatted;
}

#define MI2_CMD_INTERPRETER_EXEC "-interpreter-exec console "
#define MI2_FRAME_INFO "frame={level=\"0\",addr=\"0xdeadbeef\",func=\"dosbox\",file=\"C:\\foo.exe\"}"
#define MI2_THREAD_INFO "{id=\"1\",target-id=\"i1\",name=\"Emulator\"," MI2_FRAME_INFO "}"

static void DEBUG_WriteRegister(int number, char* value) {
    char formatted[128];
    snprintf(formatted, sizeof(formatted) - 2, "{number=\"%i\",value=\"%s\"}", number, value);
    DEBUG_ServerWriteResponse(formatted);
}
static void DEBUG_WriteRegister32(int number, uint32_t value) {
    char formatted[128];
    snprintf(formatted, sizeof(formatted) - 2, "0x%08x", value);
    DEBUG_WriteRegister(number, formatted);
}
static void DEBUG_WriteRegister16(int number, uint16_t value) {
    char formatted[128];
    snprintf(formatted, sizeof(formatted) - 2, "0x%04x", value);
    DEBUG_WriteRegister(number, formatted);
}

/**
 * Handles a single request from the remote debugger and sends a response
 *
 * The request is already trimmed from all whitespaces
 */
int DEBUG_ServerHandleRequest(char* request) {


    if (strncmp(request, MI2_CMD_INTERPRETER_EXEC, strlen(MI2_CMD_INTERPRETER_EXEC)) == 0) {
        
        // Strip leading and trailing quotes (TODO: do proper unescaping..)
        char* cmd = request + strlen(MI2_CMD_INTERPRETER_EXEC) + 1;
        cmd[strlen(cmd) - 1] = '\0';
        // Run command

        if (strcmp(cmd, "show version") == 0) {
            DEBUG_ServerWriteResponse("~\"DOSBox-X " VERSION " gdb-debugserver MI2\"\n");
        }
        else if (strcmp(cmd, "show endian") == 0) {
            DEBUG_ServerWriteResponse("~\"little endian\"\n(gdb)\r\n");
        }
        else if (strcmp(cmd, "info proc mappings") == 0) {
            DEBUG_ServerWriteResponse("~\"0x0 0x1000000 0x1000000 0x0 mem\"\n");
        }
        else if (strcmp(cmd, "show os") == 0) {
            DEBUG_ServerWriteResponse("~\"The current OS ABI is \\\"Linux\\\"\"\r\n");
        }
        else if (strcmp(cmd, "show architecture") == 0) {
            DEBUG_ServerWriteResponse("~\"(currently i386)\"\r\n");
        }
        else if (strcmp(cmd, "maintenance info sections ALLOBJ nosections") == 0) {
            // ignore
        }
        
        else if (strncmp(cmd, "set ", 4) == 0) {
            // ignore for now
        }
        else {
            if (IsDebuggerActive()) {
                // Run an abitrary command in the debugger
                ParseCommand(cmd);
            }
            else
                // Cannot run commands in debugger when the program is not stopped
                return DEBUG_ServerWriteResponse("^error,msg=\"dosbox-not-stopped\",code=\"1\"\r\n");
        }
        return DEBUG_ServerWriteResponse("^done\r\n");
    }
    else if (strcmp(request, "-environment-pwd") == 0) {
        return DEBUG_ServerWriteResponse("^done,cwd=\"/foo/bar\"");
    }
    else if (strcmp(request, "-exec-continue") == 0) {
        if (IsDebuggerActive()) {
            DEBUG_SetRunmode(DEBUG_RUNMODE_NORMAL);
        }
        DEBUG_ServerWriteResponse("*running,thread-id=\"1\"\r\n");
        return DEBUG_ServerWriteResponse("^running\r\n");
    }
    else if (strstr(request, "-data-read-memory-bytes")) {
        uint32_t offset, length;
        char buf[256];
        sscanf(request, "-data-read-memory-bytes 0x%x %i", &offset, &length);
        if (length > 4096) {
            sprintf(buf, "wtf %i", length);
            DEBUG_ServerWriteResponse(buf);
            return 1;
        }
        snprintf(buf, sizeof(buf) - 2, "^done,memory=[{begin=\"0x%x\",offset=\"0x%x\",end=\"0x%x\",contents=\"", offset, 0, offset+length);
        DEBUG_ServerWriteResponse(buf);
        for (int i = 0; i < length; i++) {
            uint8_t byte = mem_readb(offset + i);
            sprintf(buf, "%02x", byte);
            DEBUG_ServerWriteResponse(buf);
        }
        DEBUG_ServerWriteResponse("\"}]\r\n");

    }
    else if (strstr(request, "-exec-interrupt")) {
        if (!IsDebuggerActive()) {
            DEBUG_SetRunmode(DEBUG_RUNMODE_DEBUG);
        }
        DEBUG_ServerWriteResponse("*stopped,reason=\"exec\",thread-id=\"1\"\r\n");
        return DEBUG_ServerWriteResponse("^done\r\n");
    }
    else if (strcmp(request, "-environment-path") == 0) {
        return DEBUG_ServerWriteResponse("^done,path=\"/foo/bar\"");
    }
    else if (strstr(request, "-target-attach")) {
        DEBUG_ServerWriteResponse("=thread-created,id=\"1\",group-id=\"i1\"\r\n");
        DEBUG_ServerWriteResponse("*stopped,reason=\"exec\",thread-id=\"1\"\r\n");
        return DEBUG_ServerWriteResponse("^done\r\n");
    }
    else if (strcmp(request, "-break-list") == 0) {
        return DEBUG_ServerWriteResponse(
            "^done,BreakpointTable={nr_rows=\"0\",nr_cols=\"2\","
            "hdr=[{width=\"3\",alignment=\"-1\",col_name=\"number\",colhdr=\"Num\"},"
            "{width=\"14\",alignment=\"-1\",col_name=\"type\",colhdr=\"Type\"},"
            "{width=\"3\",alignment=\"-1\",col_name=\"enabled\",colhdr=\"Enb\" },"
            "{width=\"10\",alignment=\"-1\",col_name=\"addr\",colhdr=\"Address\"}],"
            "body=[]}\r\n(gdb)\r\n");
    }
    else if (strncmp(request, "-thread-select", strlen("-thread-select")) == 0) {
        return DEBUG_ServerWriteResponse("^done,new-thread-id=\"1\"\r\n");
    }
    else if (strncmp(request, "-thread-info", strlen("-thread-info")) == 0) {
        return DEBUG_ServerWriteResponse("^done,threads=[" MI2_THREAD_INFO "],current-thread-id=\"1\"\r\n");
    }
    else if (strcmp(request, "-add-inferior") == 0) {
        return DEBUG_ServerWriteResponse("^done,inferior=\"i1\"\n(gdb)\r\n");
    }
    else if (strncmp(request, "-stack-list-frames", strlen("-stack-list-frames")) == 0) {
        return DEBUG_ServerWriteResponse("^done,stack=[" MI2_FRAME_INFO "]\n(gdb)\r\n");
    }
    else if (strncmp(request, "-data-list-register-names", strlen("-data-list-register-names")) == 0) {
        return DEBUG_ServerWriteResponse("^done,register-names=[\"eax\",\"ebx\",\"ecx\",\"edx\",\"esi\",\"edi\",\"ebp\",\"esp\",\"eip\",\"cs\",\"ds\",\"es\",\"fs\",\"gs\",\"eflags\",\"cr0\",\"st0\",\"st1\",\"st2\",\"st3\",\"st4\",\"st5\",\"st6\",\"st7\"]\r\n");
    }
    else if (strncmp(request, "-data-evaluate-expression --thread 1 \"{sizeof($eax)", strlen("-data-evaluate-expression --thread 1 \"{sizeof($eax)")) == 0) {
        return DEBUG_ServerWriteResponse("^done,value=\"{4,4,4,4,4,4,4,4,4,2,2,2,2,2,4,4,4,4,4,4,4,4,4,4}\"\r\n");
    }
    else if (strncmp(request, "-data-list-register-values", strlen("-data-list-register-values")) == 0) {
        char x87buf[8][12] = {};
        DEBUG_ServerWriteResponse("^done,register-values=[");
        DEBUG_WriteRegister32(0, reg_eax);
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister32(1, reg_ebx);
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister32(2, reg_ecx);
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister32(3, reg_edx);
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister32(4, reg_esi);
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister32(5, reg_edi);
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister32(6, reg_ebp);
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister32(7, reg_esp);
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister32(8, reg_eip);
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister16(9, SegValue(cs));
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister16(10, SegValue(ds));
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister16(11, SegValue(es));
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister16(12, SegValue(fs));
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister16(13, SegValue(gs));
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister32(14, reg_flags);
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister32(15, cpu.cr0);
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister(16, F80ToString(STV(0), x87buf[0]));
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister(17, F80ToString(STV(1), x87buf[1]));
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister(18, F80ToString(STV(2), x87buf[2]));
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister(19, F80ToString(STV(3), x87buf[3]));
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister(20, F80ToString(STV(4), x87buf[4]));
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister(21, F80ToString(STV(5), x87buf[5]));
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister(22, F80ToString(STV(6), x87buf[6]));
        DEBUG_ServerWriteResponse(",");
        DEBUG_WriteRegister(23, F80ToString(STV(7), x87buf[7]));
        return DEBUG_ServerWriteResponse("]\r\n");
    }
    else if (strcmp(request, "-list-thread-groups") == 0) {
        return DEBUG_ServerWriteResponse("^done,groups=[{id=\"i1\",type=\"process\",pid=\"1\",num_children=\"1\",executable=\"DOSBox-X " VERSION "\",description=\"DOSBox-X " VERSION "\",threads=[" MI2_THREAD_INFO "]}]\r\n");
    }
    else if (strcmp(request, "-list-thread-groups --available") == 0) {
        return DEBUG_ServerWriteResponse("^done,groups=[{id=\"1\",type=\"process\",pid=\"1\",executable=\"DOSBox-X " VERSION "\",description=\"DOSBox-X " VERSION "\"}]\r\n");
    }
    else if (strcmp(request, "-list-thread-groups i1") == 0) {
        return DEBUG_ServerWriteResponse("^done,threads=[" MI2_THREAD_INFO "]\r\n");
    }
    else if (strcmp(request, "-exec-arguments") == 0) {
        // Enable the debugger
        DEBUG_EnableDebugger();
        return DEBUG_ServerWriteResponse("^done\r\n(gdb)\r\n");
    }
    else {
        // Request is unknown
        return DEBUG_ServerWriteResponse("^error,msg=\"Unknown request\",code=\"0\"\r\n");
    }
}

/**
 * Accepts remote connections and handles their requests
 *
 * This method accepts one remote connection at a time and
 * then loops over received requestsuntil the connection
 * dies or dosbox is shutdown
 */
void DEBUG_Server(void* arguments) {
    int bytesRead = 0;
    char request[MAX_REQUEST_SIZE];

    while (!DEBUG_server.shutdown) {
        // Wait for a single connection
        DEBUG_ServerAcceptConnection(NULL, DEBUG_SERVER_PORT);

        // Receive until the remote debugger disconnects
        while (0 < (bytesRead = DEBUG_ServerReadRequest(request, sizeof(request)))) {
            if (DEBUG_server.shutdown) {
                return;
            }
            DEBUG_ServerHandleRequest(trim(request));
        }
    }
}

#endif // C_DEBUG && C_DEBUG_SERVER
