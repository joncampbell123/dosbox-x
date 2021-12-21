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

/**
 * Sends a repsonse with the current debugger state
 */
static int RespondState() {
    if (IsDebuggerActive()) {
        // debugger active means program is stopped
        return DEBUG_ServerWriteResponse("s:stopped");
    }
    else if (IsDebuggerRunwatch()) {
        return DEBUG_ServerWriteResponse("s:watching");
    }
    else if (IsDebuggerRunNormal()) {
        // normal means the program is running
        return DEBUG_ServerWriteResponse("s:running");
    }
    else {
        return DEBUG_ServerWriteResponse("s:inactive");
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
        else if (strcmp(cmd, "show os") == 0) {
            DEBUG_ServerWriteResponse("~\"The current OS ABI is \\\"PC98\\\"\"\n(gdb)\r\n");
        }
        else if (strcmp(cmd, "show architecture") == 0) {
            DEBUG_ServerWriteResponse("~\"(currently i386)\"\n(gdb)\r\n");
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
                return DEBUG_ServerWriteResponse("^error,msg=\"dosbox-not-stopped\",code=\"1\"\r\n(gdb)\r\n");
        }
        return DEBUG_ServerWriteResponse("^done\r\n(gdb)\r\n");
    }
    else if (strcmp(request, "-environment-pwd") == 0) {
        return DEBUG_ServerWriteResponse("^done,cwd=\"/foo/bar\"");
    }
    else if (strcmp(request, "-exec-continue") == 0) {
        if (IsDebuggerActive()) {
            DEBUG_SetRunmode(DEBUG_RUNMODE_NORMAL);
        }
        return DEBUG_ServerWriteResponse("*running,thread-id=\"1\"\r\n");
    }
    else if (strstr(request, "-exec-interrupt")) {
        if (!IsDebuggerActive()) {
            DEBUG_SetRunmode(DEBUG_RUNMODE_DEBUG);
        }
        return DEBUG_ServerWriteResponse("*stopped,reason=\"exec\",thread-id=\"1\"\r\n(gdb)\r\n");

    }
    else if (strcmp(request, "-environment-path") == 0) {
        return DEBUG_ServerWriteResponse("^done,path=\"/foo/bar\"");
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
        return DEBUG_ServerWriteResponse("^done,threads=[{id=\"1\",target-id=\"1\",name=\"Emulator\"}],current-thread-id=\"1\"\r\n");
    }
    else if (strcmp(request, "-add-inferior") == 0) {
        return DEBUG_ServerWriteResponse("^done,inferior=\"i1\"\n(gdb)\r\n");
    }
    else if (strcmp(request, "-list-thread-groups") == 0) {
        return DEBUG_ServerWriteResponse("^done,groups=[{id=\"i1\",type=\"process\",pid=\"1\",num_children=\"1\",executable=\"DOSBox-X " VERSION "\",description=\"DOSBox-X " VERSION "\",threads=[{id=\"1\",target-id=\"Emulation\",state=\"running\"}]}]\r\n(gdb)\r\n");
    }
    else if (strcmp(request, "-list-thread-groups --available") == 0) {
        return DEBUG_ServerWriteResponse("^done,groups=[{id=\"1\",type=\"process\",pid=\"1\",num_children=\"1\",executable=\"DOSBox-X " VERSION "\",description=\"DOSBox-X " VERSION "\"}]\r\n(gdb)\r\n");
    }
    else if (strcmp(request, "-list-thread-groups i1") == 0) {
        return DEBUG_ServerWriteResponse("^done,threads=[{id=\"1\",target-id=\"Emulation\",state=\"running\"}]\n(gdb)\r\n");
    }
    else if (strcmp(request, "-exec-arguments") == 0) {
        // Enable the debugger
        DEBUG_EnableDebugger();
        return RespondState();
    }
    else if (strcmp(request, "bp") == 0) {
        // List breakpoints
        int nr = 0;
        std::list<Breakpoint*>::iterator i;
        char breakpoints[1024];
        strcpy(breakpoints, "bp:");
        for (i = Breakpoint::allBreakpoints.begin(); i != Breakpoint::allBreakpoints.end(); ++i) {
            Breakpoint* bp = *i;
            char* breakpoint = FormatBreakpoint(nr, bp);
            if (nr > 0) {
                safe_strcat(breakpoints, ";");
            }
            safe_strcat(breakpoints, breakpoint);
            nr++;
        }
        return DEBUG_ServerWriteResponse(breakpoints);
    }
    else if (strcmp(request, "r") == 0) {
        // Return registers and status
        char regs[512];
        char x87buf[8][12] = {};
        snprintf(regs, sizeof(regs) - 1, 
            "r:eax=%08x;ebx=%08x;ecx=%08x;edx=%08x;esi=%08x;edi=%08x;"  
            "eip=%08x;ebp=%08x;esp=%08x;eflags=%08x;" 
            "cs=%04x;ds=%04x;es=%04x;fs=%04x;gs=%04x;" 
            "st0=%s;st1=%s;st2=%s;st3=%s;st4=%s;st5=%s;st6=%s;st7=%s;" 
            "mode=%02x;big=%02x;cpl=%02x;paging=%02x;cr0=%08x;" 
            "cycles=%08x", 
            reg_eax, reg_ebx, reg_ecx, reg_edx, reg_esi, reg_edi,
            reg_eip, reg_ebp, reg_esp, reg_flags,
            SegValue(cs), SegValue(ds), SegValue(es), SegValue(fs), SegValue(gs),
            F80ToString(STV(0), x87buf[0]),
            F80ToString(STV(1), x87buf[1]),
            F80ToString(STV(2), x87buf[2]),
            F80ToString(STV(3), x87buf[3]),
            F80ToString(STV(4), x87buf[4]),
            F80ToString(STV(5), x87buf[5]),
            F80ToString(STV(6), x87buf[6]),
            F80ToString(STV(7), x87buf[7]),
            cpu.pmode, cpu.code.big, cpu.cpl, paging.enabled, cpu.cr0,
            cycle_count
        );
        return DEBUG_ServerWriteResponse(regs);
    }
    else if (strncmp(request, "cmd:", 4) == 0 && strlen(request) > 4) {
        // Run an abitrary command in the debugger
        if (IsDebuggerActive()) {
            ParseCommand(request + 4);
            return RespondState();
        }
        else {
            // Cannot run commands in debugger when the program is not stopped
            return DEBUG_ServerWriteResponse("e:not-stopped");
        }
    }
    else {
        // Request is unknown
        return DEBUG_ServerWriteResponse("^error,msg=\"Unknown request\",code=\"0\"\r\n(gdb)\r\n");
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
