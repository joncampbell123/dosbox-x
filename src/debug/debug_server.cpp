#include "dosbox.h"

#if C_DEBUG && C_DEBUG_SERVER

#include "debug.h"
#include "logging.h"
#include "support.h"

#define MAX_REQUEST_SIZE 512
#define PORT "2999"

// From debug.c
extern int debugrunmode;
extern bool tohide;

// This structure controls various aspects of the serverr
struct DebugServerControl DEBUG_server;

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

/**
 * Handles a single request from the remote debugger and sends a response
 *
 * The request is already trimmed from all whitespaces
 */
int DEBUG_ServerHandleRequest(char* request) {


    if (strcmp(request, "v") == 0) {
        // Get dosbox version        
        return DEBUG_ServerWriteResponse("v:" VERSION);
    }
    else if (strcmp(request, "s") == 0) {
        // Get current debugger state
        return RespondState();
    }
    else if (strcmp(request, "run") == 0) {
        // Run program (normal execution)
        DEBUG_SetRunmode(DEBUG_RUNMODE_NORMAL);
        return RespondState();
    }
    else if (strcmp(request, "runwatch") == 0) {
        // Run program (watch modefor debugger)
        DEBUG_SetRunmode(DEBUG_RUNMODE_WATCH);
        return RespondState();
    }
    else if (strcmp(request, "stop") == 0) {
        // Run debugger / stop program
        DEBUG_SetRunmode(DEBUG_RUNMODE_DEBUG);
        return RespondState();
    }
    else if (strcmp(request, "enable") == 0) {
        // Enable the debugger
        DEBUG_EnableDebugger();
        return RespondState();
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
        return DEBUG_ServerWriteResponse("e:unknown");
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
        DEBUG_ServerAcceptConnection(NULL, PORT);

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
