#!/usr/bin/env python3
"""
extlpt_peer.py - minimal reference peer for the DOSBox-X `extlpt` parallel-port
backend. Copy this file, subclass `Device`, done.

DOSBox-X's `parallel1=extlpt` turns every guest access to the LPT registers
(DATA / STATUS / CONTROL) into a synchronous 4-byte request / 4-byte response
RPC to an external process. This file is that process, boiled down to the
protocol and nothing else - no hardware, no framework. The wire rules never
change, so whatever device you need to fake (a programmer, a transfer cable,
a lab instrument) is just a `Device` subclass.

---------------------------------------------------------------------------
Wire protocol
---------------------------------------------------------------------------
Reliable ordered stream (TCP or a local pipe). No length prefix: every frame
is exactly 4 bytes.

    byte0  opcode
    byte1  register    0 = DATA (base+0), 1 = STATUS (base+1), 2 = CONTROL (base+2)
    byte2  value
    byte3  sequence    echoed back verbatim; a framing tripwire only

    opcode   0x01 WRITE   0x02 READ   0x03 RESPONSE   0x04 ACK
             0x05 HELLO   0xFF ERROR

Exchange:
    DOSBox-X -> OP_HELLO  ver caps 0     (very first frame, once per connection)
    peer     -> OP_HELLO  ver caps 0     ver must match or the connection is refused

    DOSBox-X -> OP_WRITE  reg val seq    guest wrote `val` to `reg`
    peer     -> OP_ACK    reg P   seq    P = 1 if you now have a response queued
                                         (lets DOSBox-X decide whether a following
                                         read belongs to you or its printer engine)

    DOSBox-X -> OP_READ   reg 0   seq    guest is reading `reg`
    peer     -> OP_RESPONSE reg val seq  `val` is handed to the guest

    peer     -> OP_ERROR  reg code seq   fails that transaction (diagnostic only)

DOSBox-X blocks the guest CPU on each exchange, bounded by ~2 s; answer
promptly. Keep PROTOCOL_VERSION in lockstep with extlpt.cpp.

---------------------------------------------------------------------------
Quick start
---------------------------------------------------------------------------
    python extlpt_peer.py --selftest                # no DOSBox-X: exercise the Device
    python extlpt_peer.py --tcp 5700 -v             # listen, decode every frame
    python extlpt_peer.py --pipe \\\\.\\pipe\\dosbox_lpt -v
    python extlpt_peer.py --tcp 5700 --device sniffer -v   # log what a DOS program does

On startup it prints the matching `dosbox-x.conf` line for the other side.
It keeps serving reconnects until Ctrl+C (use --once for a single connection).

Topology  (who creates the endpoint)
    DOSBox-X config                                    run this as
    parallel1=extlpt transport:pipe pipe:<p>           --pipe <p>            (listen)
    parallel1=extlpt transport:pipe pipe:<p> server    --pipe <p> --connect
    parallel1=extlpt transport:tcp port:<n>            --tcp [host:]<n>      (listen)
    parallel1=extlpt transport:tcp port:<n> server     --tcp host:<n> --connect
"""
import argparse
import sys
import time

# --- protocol constants (keep in sync with extlpt.cpp) --------------------
OP_WRITE, OP_READ, OP_RESPONSE, OP_ACK, OP_HELLO, OP_ERROR = 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF
REG_DATA, REG_STATUS, REG_CONTROL = 0x00, 0x01, 0x02
REG_NAME = {REG_DATA: "DATA", REG_STATUS: "STATUS", REG_CONTROL: "CONTROL"}

PROTOCOL_VERSION = 1
SUPPORTED_CAPS = 0                       # no optional extensions defined yet

ERR_BAD_LENGTH, ERR_BAD_WRITE_REG, ERR_BAD_READ_REG, ERR_UNKNOWN_OPCODE = 0, 1, 2, 3
_ERR_NAME = {0: "BAD_LENGTH", 1: "BAD_WRITE_REG", 2: "BAD_READ_REG", 3: "UNKNOWN_OPCODE"}


def decode(frame: bytes) -> str:
    """Human-readable gloss for one 4-byte frame - printed next to the raw hex."""
    op, reg, val, seq = frame
    rn = REG_NAME.get(reg, "reg%d" % reg)
    if op == OP_HELLO:
        return "HELLO v%d caps=%#04x" % (reg, val)
    if op == OP_WRITE:
        return "WRITE %s = %#04x  (seq %d)" % (rn, val, seq)
    if op == OP_READ:
        return "READ %s  (seq %d)" % (rn, seq)
    if op == OP_RESPONSE:
        return "RESPONSE %s = %#04x  (seq %d)" % (rn, val, seq)
    if op == OP_ACK:
        return "ACK %s pending=%d  (seq %d)" % (rn, val, seq)
    if op == OP_ERROR:
        return "ERROR %s %s  (seq %d)" % (rn, _ERR_NAME.get(val, str(val)), seq)
    return "OP?%#04x %02x %02x %02x" % (op, reg, val, seq)


# --- the part you replace ------------------------------------------------
class Device:
    """Model whatever sits on the far end of the cable. Override the three
    hooks; the framing/handshake around them is handled for you.

    All values are 0..255. `read`/`write` see the register index (REG_*).
    A factory (see DEVICES) builds one per connection and is handed `log`.
    """

    def __init__(self, log):
        self.log = log

    def write(self, reg: int, value: int) -> None:
        """Guest wrote `value` to `reg` (DATA or CONTROL; STATUS is read-only
        and never reaches here)."""

    def read(self, reg: int) -> int:
        """Return the byte the guest should see when it reads `reg`."""
        return 0xFF

    def response_pending(self) -> bool:
        """True when you have data queued for the guest to read back. Reported
        to DOSBox-X in every OP_ACK so it can route a following read to you
        rather than to its virtual printer. Return False if you don't share
        the port with the printer engine - it does no harm."""
        return False


class ExampleLatch(Device):
    """Trivial demo: remember the last DATA byte, hand it back on a DATA read,
    report 'ready, not busy, no error' for STATUS. Enough to see traffic."""

    def __init__(self, log):
        super().__init__(log)
        self._data = 0xFF
        self._control = 0x00

    def write(self, reg, value):
        if reg == REG_DATA:
            self._data = value
        elif reg == REG_CONTROL:
            self._control = value

    def read(self, reg):
        if reg == REG_DATA:
            return self._data
        if reg == REG_CONTROL:
            return self._control
        if reg == REG_STATUS:
            # bit7 BUSY is active-low: 0x80 => not busy. bit3 error, bit5 paper.
            return 0x80 | 0x08 | 0x20
        return 0xFF

    def response_pending(self):
        return True


class Sniffer(Device):
    """Own nothing, change nothing: log every register access and return 0xFF
    (idle bus). Point a real DOS program at this to learn its port protocol
    before you write a Device that answers for real."""

    def write(self, reg, value):
        self.log("    guest WRITE %-7s = %#04x" % (REG_NAME.get(reg, reg), value))

    def read(self, reg):
        self.log("    guest READ  %-7s -> 0xFF" % REG_NAME.get(reg, reg))
        return 0xFF


# name -> factory(log) -> Device.  Add your own and pick it with --device.
DEVICES = {
    "latch": ExampleLatch,
    "sniffer": Sniffer,
}


# --- framing / handshake / dispatch (protocol-generic, don't touch) -------
def handle_frame(dev: Device, frame: bytes) -> bytes:
    op, reg, val, seq = frame
    if op == OP_WRITE:
        if reg not in (REG_DATA, REG_CONTROL):
            return bytes([OP_ERROR, reg, ERR_BAD_WRITE_REG, seq])
        dev.write(reg, val)
        return bytes([OP_ACK, reg, 1 if dev.response_pending() else 0, seq])
    if op == OP_READ:
        if reg not in (REG_DATA, REG_STATUS, REG_CONTROL):
            return bytes([OP_ERROR, reg, ERR_BAD_READ_REG, seq])
        return bytes([OP_RESPONSE, reg, dev.read(reg) & 0xFF, seq])
    return bytes([OP_ERROR, reg, ERR_UNKNOWN_OPCODE, seq])


def serve(recv_exact, send_all, make_device, log, verbose):
    """recv_exact(n) -> n bytes or None on EOF; send_all(bytes) -> None.
    One connection: handshake, then request/response until the peer goes away."""
    hello = recv_exact(4)
    if not hello or hello[0] != OP_HELLO:
        log("handshake failed: expected OP_HELLO, got %s"
            % (hello.hex() if hello else "EOF"))
        return
    reply = bytes([OP_HELLO, PROTOCOL_VERSION, SUPPORTED_CAPS, 0])
    if verbose:
        log("REQ  %s   %s" % (hello.hex(), decode(hello)))
        log("RESP %s   %s" % (reply.hex(), decode(reply)))
    send_all(reply)
    if hello[1] != PROTOCOL_VERSION:
        log("protocol mismatch: DOSBox-X v%d, peer v%d - refusing"
            % (hello[1], PROTOCOL_VERSION))
        return
    log("handshake OK (protocol v%d)" % PROTOCOL_VERSION)

    dev = make_device(log)
    while True:
        frame = recv_exact(4)
        if not frame:
            log("DOSBox-X disconnected")
            return
        t0 = time.perf_counter()
        if verbose:
            log("REQ  %s   %s" % (frame.hex(), decode(frame)))
        reply = handle_frame(dev, frame)
        if verbose:
            dt = (time.perf_counter() - t0) * 1000.0
            log("RESP %s   %s   [%.2f ms]" % (reply.hex(), decode(reply), dt))
        send_all(reply)


def selftest(make_device, log):
    """No socket, no DOSBox-X: drive the chosen Device through a scripted
    handshake + register sequence and print the decoded trace. Fast sanity
    check while writing a Device subclass."""
    log("selftest: %s, no DOSBox-X\n" % make_device.__name__)
    hi = bytes([OP_HELLO, PROTOCOL_VERSION, 0, 0])
    ho = bytes([OP_HELLO, PROTOCOL_VERSION, SUPPORTED_CAPS, 0])
    log("REQ  %s   %s" % (hi.hex(), decode(hi)))
    log("RESP %s   %s" % (ho.hex(), decode(ho)))

    dev = make_device(log)
    script = [(OP_WRITE, REG_DATA, 0x55), (OP_WRITE, REG_CONTROL, 0x0D),
              (OP_READ, REG_STATUS, 0), (OP_READ, REG_DATA, 0)]
    for seq, (op, reg, val) in enumerate(script):
        req = bytes([op, reg, val, seq])
        rsp = handle_frame(dev, req)
        log("REQ  %s   %s" % (req.hex(), decode(req)))
        log("RESP %s   %s" % (rsp.hex(), decode(rsp)))
    log("\nselftest OK")


# --- transports --------------------------------------------------------
def _sock_io(conn):
    def recv_exact(n):
        buf = b""
        while len(buf) < n:
            chunk = conn.recv(n - len(buf))
            if not chunk:
                return None
            buf += chunk
        return buf
    return recv_exact, conn.sendall


def _parse_hostport(s, default_host="127.0.0.1"):
    host, sep, port = s.rpartition(":")
    return (host or default_host, int(port)) if sep else (default_host, int(s))


def _serve_one(r, w, make_device, log, verbose):
    try:
        serve(r, w, make_device, log, verbose)
    except OSError as e:
        log("connection error: %s" % e)


def run_tcp(addr, connect, make_device, log, verbose, once):
    import socket
    host, port = _parse_hostport(addr)

    if connect:
        try:
            c = socket.create_connection((host, port))
        except OSError as e:
            sys.exit("cannot connect to %s:%d - is DOSBox-X running and started "
                     "with the extlpt 'server' option?  (%s)" % (host, port, e))
        c.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        log("connected to %s:%d" % (host, port))
        _serve_one(*_sock_io(c), make_device, log, verbose)
        c.close()
        return

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        srv.bind((host, port))
    except OSError as e:
        sys.exit("cannot bind %s:%d - port busy? another peer already running?  (%s)"
                 % (host, port, e))
    srv.listen(1)
    log("listening on %s:%d  (Ctrl+C to stop)" % (host, port))
    try:
        while True:
            conn, peer = srv.accept()
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            log("connected: %s:%d" % peer[:2])
            _serve_one(*_sock_io(conn), make_device, log, verbose)
            conn.close()
            if once:
                break
    finally:
        srv.close()


def run_pipe(path, connect, make_device, log, verbose, once):
    if sys.platform != "win32":
        _run_unix(path, connect, make_device, log, verbose, once)
    else:
        _run_winpipe(path, connect, make_device, log, verbose, once)


def _run_unix(path, connect, make_device, log, verbose, once):
    # POSIX: extlpt uses an AF_UNIX stream socket; `path` is the socket file.
    import os
    import socket

    if connect:
        c = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            c.connect(path)
        except OSError as e:
            sys.exit("cannot connect to %s - is DOSBox-X running with the extlpt "
                     "'server' option?  (%s)" % (path, e))
        log("connected to %s" % path)
        _serve_one(*_sock_io(c), make_device, log, verbose)
        c.close()
        return

    if os.path.exists(path):
        os.unlink(path)
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(path)
    srv.listen(1)
    log("listening on %s  (Ctrl+C to stop)" % path)
    try:
        while True:
            conn, _ = srv.accept()
            log("connected")
            _serve_one(*_sock_io(conn), make_device, log, verbose)
            conn.close()
            if once:
                break
    finally:
        srv.close()
        if os.path.exists(path):
            os.unlink(path)


def _run_winpipe(path, connect, make_device, log, verbose, once):
    try:
        import win32file
        import win32pipe
        import pywintypes
    except ImportError:
        sys.exit("Windows named pipe needs pywin32:  pip install pywin32")

    def pipe_io(h):
        def recv_exact(n):
            try:
                _hr, data = win32file.ReadFile(h, n)
            except pywintypes.error:
                return None
            return bytes(data) if len(data) == n else None
        return recv_exact, lambda b: win32file.WriteFile(h, b)

    if connect:
        try:
            h = win32file.CreateFile(
                path, win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0, None, win32file.OPEN_EXISTING, 0, None)
        except pywintypes.error as e:
            sys.exit("cannot open %s - is DOSBox-X running with the extlpt "
                     "'server' option?  (%s)" % (path, e.strerror))
        log("connected to %s" % path)
        try:
            serve(*pipe_io(h), make_device=make_device, log=log, verbose=verbose)
        except pywintypes.error as e:
            log("connection error: %s" % e.strerror)
        win32file.CloseHandle(h)
        return

    log("listening on %s  (Ctrl+C to stop)" % path)
    while True:
        h = win32pipe.CreateNamedPipe(
            path, win32pipe.PIPE_ACCESS_DUPLEX,
            win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_READMODE_BYTE | win32pipe.PIPE_WAIT,
            1, 4096, 4096, 0, None)
        try:
            win32pipe.ConnectNamedPipe(h, None)
            log("connected")
            try:
                serve(*pipe_io(h), make_device=make_device, log=log, verbose=verbose)
            except pywintypes.error as e:
                log("connection error: %s" % e.strerror)
        finally:
            win32file.CloseHandle(h)
        if once:
            break


# --- entry point --------------------------------------------------------
def _config_hint(args):
    if args.tcp:
        host, port = _parse_hostport(args.tcp)
        line = "parallel1=extlpt transport:tcp port:%d" % port
        if host not in ("127.0.0.1", "localhost", "0.0.0.0"):
            line += " host:%s" % host
    else:
        line = "parallel1=extlpt transport:pipe pipe:%s" % args.pipe
    if args.connect:
        line += " server"
    return line


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--pipe", metavar="PATH",
                   help=r"named pipe (\\.\pipe\name) / AF_UNIX socket path")
    g.add_argument("--tcp", metavar="[HOST:]PORT", help="TCP endpoint")
    g.add_argument("--selftest", action="store_true",
                   help="drive the Device with a scripted sequence, no DOSBox-X")
    ap.add_argument("--connect", action="store_true",
                    help="connect to an endpoint DOSBox-X created (DOSBox-X started "
                         "with the extlpt 'server' option); default: create it and wait")
    ap.add_argument("--device", choices=sorted(DEVICES), default="latch",
                    help="which Device to run (default: %(default)s)")
    ap.add_argument("--once", action="store_true",
                    help="serve one connection then exit (default: keep serving)")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="log every raw 4-byte frame, decoded, with handler latency")
    args = ap.parse_args(argv)
    if not (args.pipe or args.tcp or args.selftest):
        ap.error("need one of --pipe, --tcp, or --selftest")

    def log(msg):
        print(msg, file=sys.stderr, flush=True)

    make_device = DEVICES[args.device]

    if args.selftest:
        selftest(make_device, log)
        return

    log("device: %s" % args.device)
    log("DOSBox-X config:  %s" % _config_hint(args))
    try:
        if args.pipe:
            run_pipe(args.pipe, args.connect, make_device, log, args.verbose, args.once)
        else:
            run_tcp(args.tcp, args.connect, make_device, log, args.verbose, args.once)
    except KeyboardInterrupt:
        log("\nstopped")


if __name__ == "__main__":
    main()
