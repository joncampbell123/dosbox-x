/*
 * Terminal text-mode output backend.
 *
 * Renders IBM-compatible text modes from emulated adapter memory to a POSIX
 * terminal. Graphics modes continue to run in the emulator, but are reported
 * as unsupported because they cannot be displayed by this backend.
 */

#include "output_terminal.h"

#include "control.h"
#include "cp437_uni.h"
#include "dosbox.h"
#include "keyboard.h"
#include "logging.h"
#include "mem.h"
#include "sdlmain.h"
#include "support.h"
#include "vga.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if !defined(WIN32) && !defined(HX_DOS) && !C_EMSCRIPTEN
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

extern int log_dev_con;

namespace {

static constexpr uint16_t BIOSMEM_SEGMENT = 0x40u;
static constexpr uint16_t BIOSMEM_NB_COLS = 0x4au;
static constexpr uint16_t BIOSMEM_CURSOR_POS = 0x50u;
static constexpr uint16_t BIOSMEM_CURSOR_TYPE = 0x60u;
static constexpr uint16_t BIOSMEM_CURRENT_PAGE = 0x62u;
static constexpr uint16_t BIOSMEM_NB_ROWS = 0x84u;
static constexpr uint32_t INVALID_CELL = 0xffffffffu;

struct TerminalState {
    bool initialized = false;
    bool warned_graphics = false;
    bool raw_mode = false;
    bool cursor_visible = true;
    bool rendered_text_frame = false;
    bool changed_log_dev_con = false;
    bool changed_log_suppress_terminal_output = false;
    bool saved_log_suppress_terminal_output = false;
    int saved_log_dev_con = 0;
    uint16_t cols = 0;
    uint16_t rows = 0;
    uint8_t cursor_col = 0xff;
    uint8_t cursor_row = 0xff;
    std::vector<uint32_t> cells = {};
    std::vector<uint32_t> dummy = {};
#if !defined(WIN32) && !defined(HX_DOS) && !C_EMSCRIPTEN
    termios original_termios = {};
    int original_flags = 0;
#endif
};

TerminalState terminal;

static bool host_terminal_supported()
{
#if !defined(WIN32) && !defined(HX_DOS) && !C_EMSCRIPTEN
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#else
    return false;
#endif
}

static void write_term(const char *s)
{
    fputs(s, stdout);
}

static void flush_term()
{
    fflush(stdout);
}

static void restore_logging()
{
    if (terminal.changed_log_dev_con) {
        log_dev_con = terminal.saved_log_dev_con;
        terminal.changed_log_dev_con = false;
    }
    if (terminal.changed_log_suppress_terminal_output) {
        logSuppressTerminalOutput = terminal.saved_log_suppress_terminal_output;
        terminal.changed_log_suppress_terminal_output = false;
    }
}

static void restore_terminal_mode()
{
#if !defined(WIN32) && !defined(HX_DOS) && !C_EMSCRIPTEN
    if (terminal.raw_mode) {
        tcsetattr(STDIN_FILENO, TCSANOW, &terminal.original_termios);
        fcntl(STDIN_FILENO, F_SETFL, terminal.original_flags);
        terminal.raw_mode = false;
    }
#endif
}

static void suppress_dos_console_logging()
{
    if (!terminal.changed_log_dev_con) {
        terminal.saved_log_dev_con = log_dev_con;
        terminal.changed_log_dev_con = true;
    }
    if (!terminal.changed_log_suppress_terminal_output) {
        terminal.saved_log_suppress_terminal_output = logSuppressTerminalOutput;
        terminal.changed_log_suppress_terminal_output = true;
    }

    log_dev_con = 0;
    logSuppressTerminalOutput = true;
}

static Bitu fail_terminal_output(const char *message)
{
    restore_terminal_mode();
    restore_logging();
    E_Exit("%s", message);
    return 0; // not reached
}

static void require_terminal_size(uint16_t cols, uint16_t rows)
{
#if !defined(WIN32) && !defined(HX_DOS) && !C_EMSCRIPTEN
    winsize size = {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0 || size.ws_col == 0 || size.ws_row == 0)
        return;

    if (size.ws_col < cols || size.ws_row < rows) {
        char message[160];
        snprintf(message,
                 sizeof(message),
                 "Terminal output requires at least %ux%u characters; current terminal is %ux%u.",
                 static_cast<unsigned>(cols),
                 static_cast<unsigned>(rows),
                 static_cast<unsigned>(size.ws_col),
                 static_cast<unsigned>(size.ws_row));
        fail_terminal_output(message);
    }
#else
    (void)cols;
    (void)rows;
#endif
}

static void set_color(uint8_t attr)
{
    struct Rgb {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    };

    static constexpr Rgb dos_color[16] = {
            {0x00, 0x00, 0x00},
            {0x00, 0x00, 0xaa},
            {0x00, 0xaa, 0x00},
            {0x00, 0xaa, 0xaa},
            {0xaa, 0x00, 0x00},
            {0xaa, 0x00, 0xaa},
            {0xaa, 0x55, 0x00},
            {0xaa, 0xaa, 0xaa},
            {0x55, 0x55, 0x55},
            {0x55, 0x55, 0xff},
            {0x55, 0xff, 0x55},
            {0x55, 0xff, 0xff},
            {0xff, 0x55, 0x55},
            {0xff, 0x55, 0xff},
            {0xff, 0xff, 0x55},
            {0xff, 0xff, 0xff},
    };
    const uint8_t fg = attr & 0x0f;
    const uint8_t bg = (attr >> 4u) & 0x07;
    const auto &fg_color = dos_color[fg];
    const auto &bg_color = dos_color[bg];
    char seq[64];
    snprintf(seq,
             sizeof(seq),
             "\033[38;2;%u;%u;%u;48;2;%u;%u;%um",
             static_cast<unsigned>(fg_color.red),
             static_cast<unsigned>(fg_color.green),
             static_cast<unsigned>(fg_color.blue),
             static_cast<unsigned>(bg_color.red),
             static_cast<unsigned>(bg_color.green),
             static_cast<unsigned>(bg_color.blue));
    write_term(seq);
}

static void write_cp437(uint8_t chr)
{
    if (chr >= 32 && chr < 127)
        fputc(static_cast<char>(chr), stdout);
    else if (chr == 0 || chr == 0xff)
        fputc(' ', stdout);
    else {
        char encoded[8] = {};
        char *out = encoded;
        /* The shared CP437 table preserves 0x7f as DEL; VGA text displays it as a house glyph. */
        const uint32_t codepoint = (chr == 0x7f) ? 0x2302u : cp437_to_unicode[chr];
        if (utf8_encode(&out, encoded + sizeof(encoded), codepoint) >= 0)
            fwrite(encoded, 1, static_cast<size_t>(out - encoded), stdout);
    }
}

static bool text_mode_active()
{
    return !IS_PC98_ARCH && vga.mem.linear != nullptr &&
           (vga.mode == M_TEXT || vga.mode == M_TANDY_TEXT || vga.mode == M_HERC_TEXT);
}

static uint16_t current_cols()
{
    const auto cols = real_readw(BIOSMEM_SEGMENT, BIOSMEM_NB_COLS);
    return cols ? cols : 80;
}

static uint16_t current_rows()
{
    if (IS_EGAVGA_ARCH)
        return static_cast<uint16_t>(real_readb(BIOSMEM_SEGMENT, BIOSMEM_NB_ROWS) + 1u);
    return 25;
}

static void move_cursor(uint16_t row, uint16_t col)
{
    char seq[32];
    snprintf(seq, sizeof(seq), "\033[%u;%uH",
             static_cast<unsigned>(row + 1u),
             static_cast<unsigned>(col + 1u));
    write_term(seq);
}

static void resize_cache(uint16_t cols, uint16_t rows)
{
    if (cols == terminal.cols && rows == terminal.rows)
        return;

    terminal.cols = cols;
    terminal.rows = rows;
    terminal.cells.assign(static_cast<size_t>(cols) * rows, INVALID_CELL);
    write_term("\033[2J");
}

static uint8_t read_vram_byte(const uint8_t *vram, Bitu offset)
{
    return vram[offset & vga.draw.linear_mask];
}

static uint16_t read_cell(uint16_t row, uint16_t col)
{
    uint8_t chr = ' ';
    uint8_t attr = 0x07u;
    const uint8_t *vram = nullptr;
    Bitu vram_offset = 0;

    /* Read from the adapter-visible text page, matching other video outputs. */
    if (vga.mode == M_TEXT && IS_EGAVGA_ARCH) {
        const Bitu visible_offset = (vga.config.real_start + vga.draw.bytes_skip) +
                                    static_cast<Bitu>(row) * vga.draw.address_add +
                                    (static_cast<Bitu>(col) << vga.config.addr_shift);
        vram = vga.draw.linear_base ? vga.draw.linear_base : vga.mem.linear;
        vram_offset = (visible_offset << 2u) & vga.draw.linear_mask;
        if (vram == nullptr)
            return 0x0720u;

        chr = read_vram_byte(vram, vram_offset + 0u);
        attr = read_vram_byte(vram, vram_offset + 1u);
    } else if (machine == MCH_HERC && hercCard == HERC_InColor) {
        const Bitu visible_offset = (vga.config.real_start + vga.draw.bytes_skip) * 2u +
                                    static_cast<Bitu>(row) * vga.draw.address_add +
                                    static_cast<Bitu>(col) * 2u;
        vram = vga.tandy.draw_base ? vga.tandy.draw_base : vga.mem.linear;
        if (vram == nullptr)
            return 0x0720u;

        vram_offset = (visible_offset * sizeof(uint32_t)) & vga.draw.linear_mask;
        chr = read_vram_byte(vram, vram_offset + 0u);
        attr = read_vram_byte(vram, vram_offset + sizeof(uint32_t));
    } else {
        const Bitu visible_offset = (vga.config.real_start + vga.draw.bytes_skip) * 2u +
                                    static_cast<Bitu>(row) * vga.draw.address_add +
                                    static_cast<Bitu>(col) * 2u;
        vram = vga.tandy.draw_base ? vga.tandy.draw_base : vga.mem.linear;
        vram_offset = visible_offset & vga.draw.linear_mask;
        if (vram == nullptr)
            return 0x0720u;

        chr = read_vram_byte(vram, vram_offset + 0u);
        attr = read_vram_byte(vram, vram_offset + 1u);
    }

    if (machine == MCH_MDA ||
        (machine == MCH_HERC &&
         (hercCard < HERC_InColor || (vga.herc.exception & 0x20)))) {
        /* MDA/Herc attributes encode intensity/reverse/blanking, not RGB color nibbles. */
        if ((attr & 0x77u) == 0x00u)
            attr = 0x00u;
        else if ((attr & 0x77u) == 0x70u)
            attr = (attr & 0x08u) ? 0x78u : 0x70u;
        else
            attr = (attr & 0x08u) ? 0x0fu : 0x07u;
    }

    return static_cast<uint16_t>(chr | (attr << 8u));
}

static void update_cursor(bool force_position)
{
    const uint8_t page = real_readb(BIOSMEM_SEGMENT, BIOSMEM_CURRENT_PAGE);
    const uint8_t col = real_readb(BIOSMEM_SEGMENT, BIOSMEM_CURSOR_POS + page * 2u);
    const uint8_t row = real_readb(BIOSMEM_SEGMENT, BIOSMEM_CURSOR_POS + page * 2u + 1u);
    const bool visible = (real_readw(BIOSMEM_SEGMENT, BIOSMEM_CURSOR_TYPE) & 0x2000u) == 0;

    if (visible != terminal.cursor_visible) {
        write_term(visible ? "\033[?25h" : "\033[?25l");
        terminal.cursor_visible = visible;
    }

    if (visible && row < terminal.rows && col < terminal.cols &&
        (force_position || row != terminal.cursor_row || col != terminal.cursor_col)) {
        move_cursor(row, col);
        terminal.cursor_row = row;
        terminal.cursor_col = col;
    }
}

static void redraw_text()
{
    const uint16_t cols = std::min<uint16_t>(current_cols(), 160);
    const uint16_t rows = std::min<uint16_t>(current_rows(), 100);
    require_terminal_size(cols, rows);
    resize_cache(cols, rows);

    int last_attr = -1;
    bool wrote_cells = false;
    for (uint16_t row = 0; row < rows; ++row) {
        for (uint16_t col = 0; col < cols;) {
            const auto idx = static_cast<size_t>(row) * cols + col;
            const uint16_t cell = read_cell(row, col);
            if (terminal.cells[idx] == cell) {
                ++col;
                continue;
            }

            move_cursor(row, col);

            const uint8_t attr = static_cast<uint8_t>(cell >> 8u);
            if (attr != last_attr) {
                set_color(attr);
                last_attr = attr;
            }

            while (col < cols) {
                const auto run_idx = static_cast<size_t>(row) * cols + col;
                const uint16_t run_cell = read_cell(row, col);
                if (terminal.cells[run_idx] == run_cell ||
                    static_cast<int>(static_cast<uint8_t>(run_cell >> 8u)) != last_attr)
                    break;

                terminal.cells[run_idx] = run_cell;
                write_cp437(static_cast<uint8_t>(run_cell & 0xffu));
                wrote_cells = true;
                ++col;
            }
        }
    }

    if (wrote_cells)
        write_term("\033[0m");
    update_cursor(wrote_cells);
    flush_term();
}

static void press_key(KBD_KEYS key)
{
    KEYBOARD_AddKey(key, true);
    KEYBOARD_AddKey(key, false);
}

static KBD_KEYS ascii_to_key(uint8_t ch, bool &shift)
{
    static constexpr KBD_KEYS letter_keys[26] = {
            KBD_a, KBD_b, KBD_c, KBD_d, KBD_e, KBD_f, KBD_g, KBD_h, KBD_i,
            KBD_j, KBD_k, KBD_l, KBD_m, KBD_n, KBD_o, KBD_p, KBD_q, KBD_r,
            KBD_s, KBD_t, KBD_u, KBD_v, KBD_w, KBD_x, KBD_y, KBD_z,
    };

    shift = false;
    if (ch >= 'a' && ch <= 'z')
        return letter_keys[ch - 'a'];
    if (ch >= 'A' && ch <= 'Z') {
        shift = true;
        return letter_keys[ch - 'A'];
    }
    if (ch >= '1' && ch <= '9')
        return static_cast<KBD_KEYS>(KBD_1 + (ch - '1'));
    if (ch == '0')
        return KBD_0;

    switch (ch) {
    case ' ': return KBD_space;
    case '\r':
    case '\n': return KBD_enter;
    case '\t': return KBD_tab;
    case 0x7f:
    case '\b': return KBD_backspace;
    case '`': return KBD_grave;
    case '-': return KBD_minus;
    case '=': return KBD_equals;
    case '[': return KBD_leftbracket;
    case ']': return KBD_rightbracket;
    case '\\': return KBD_backslash;
    case ';': return KBD_semicolon;
    case '\'': return KBD_quote;
    case ',': return KBD_comma;
    case '.': return KBD_period;
    case '/': return KBD_slash;
    case '~': shift = true; return KBD_grave;
    case '!': shift = true; return KBD_1;
    case '@': shift = true; return KBD_2;
    case '#': shift = true; return KBD_3;
    case '$': shift = true; return KBD_4;
    case '%': shift = true; return KBD_5;
    case '^': shift = true; return KBD_6;
    case '&': shift = true; return KBD_7;
    case '*': shift = true; return KBD_8;
    case '(': shift = true; return KBD_9;
    case ')': shift = true; return KBD_0;
    case '_': shift = true; return KBD_minus;
    case '+': shift = true; return KBD_equals;
    case '{': shift = true; return KBD_leftbracket;
    case '}': shift = true; return KBD_rightbracket;
    case '|': shift = true; return KBD_backslash;
    case ':': shift = true; return KBD_semicolon;
    case '"': shift = true; return KBD_quote;
    case '<': shift = true; return KBD_comma;
    case '>': shift = true; return KBD_period;
    case '?': shift = true; return KBD_slash;
    default: return KBD_NONE;
    }
}

static void press_ascii(uint8_t ch)
{
    bool shift = false;
    const KBD_KEYS key = ascii_to_key(ch, shift);
    if (key == KBD_NONE)
        return;

    if (shift)
        KEYBOARD_AddKey(KBD_leftshift, true);
    press_key(key);
    if (shift)
        KEYBOARD_AddKey(KBD_leftshift, false);
}

static void press_alt_ascii(uint8_t ch)
{
    bool shift = false;
    const KBD_KEYS key = ascii_to_key(ch, shift);
    if (key == KBD_NONE)
        return;

    KEYBOARD_AddKey(KBD_leftalt, true);
    if (shift)
        KEYBOARD_AddKey(KBD_leftshift, true);
    press_key(key);
    if (shift)
        KEYBOARD_AddKey(KBD_leftshift, false);
    KEYBOARD_AddKey(KBD_leftalt, false);
}

static void handle_escape_sequence(const std::string &seq)
{
    if (seq.size() < 3 || seq[0] != '\033')
    {
        if (seq.size() == 2)
            press_alt_ascii(static_cast<uint8_t>(seq[1]));
        return;
    }

    if (seq[1] == 'O') {
        switch (seq[2]) {
        case 'F': press_key(KBD_end); return;
        case 'H': press_key(KBD_home); return;
        case 'P': press_key(KBD_f1); return;
        case 'Q': press_key(KBD_f2); return;
        case 'R': press_key(KBD_f3); return;
        case 'S': press_key(KBD_f4); return;
        default: return;
        }
    }

    if (seq[1] != '[')
        return;

    switch (seq.back()) {
    case 'A': press_key(KBD_up); return;
    case 'B': press_key(KBD_down); return;
    case 'C': press_key(KBD_right); return;
    case 'D': press_key(KBD_left); return;
    case 'F': press_key(KBD_end); return;
    case 'H': press_key(KBD_home); return;
    default: break;
    }

    if (seq.back() != '~')
        return;

    size_t pos = 2;
    int code = 0;
    while (pos < seq.size() && seq[pos] >= '0' && seq[pos] <= '9') {
        code = code * 10 + (seq[pos] - '0');
        ++pos;
    }

    switch (code) {
    case 1: press_key(KBD_home); break;
    case 2: press_key(KBD_insert); break;
    case 3: press_key(KBD_delete); break;
    case 4: press_key(KBD_end); break;
    case 5: press_key(KBD_pageup); break;
    case 6: press_key(KBD_pagedown); break;
    case 11: press_key(KBD_f1); break;
    case 12: press_key(KBD_f2); break;
    case 13: press_key(KBD_f3); break;
    case 14: press_key(KBD_f4); break;
    case 15: press_key(KBD_f5); break;
    case 17: press_key(KBD_f6); break;
    case 18: press_key(KBD_f7); break;
    case 19: press_key(KBD_f8); break;
    case 20: press_key(KBD_f9); break;
    case 21: press_key(KBD_f10); break;
    case 23: press_key(KBD_f11); break;
    case 24: press_key(KBD_f12); break;
    default: break;
    }
}

} // namespace

void OUTPUT_TERMINAL_Select()
{
    sdl.desktop.want_type = SCREEN_TERMINAL;

    /* Terminal output owns stdout while active, so late logs would corrupt the DOS screen. */
    suppress_dos_console_logging();
}

Bitu OUTPUT_TERMINAL_GetBestMode(Bitu flags)
{
    (void)flags;
    return GFX_CAN_32 | GFX_RGBONLY;
}

Bitu OUTPUT_TERMINAL_SetSize()
{
    if (!host_terminal_supported()) {
        return fail_terminal_output("Terminal output requires an interactive POSIX terminal.");
    }
    suppress_dos_console_logging();
    require_terminal_size(80, 25);

    terminal.dummy.assign(std::max<Bitu>(1, sdl.draw.width) *
                          std::max<Bitu>(1, sdl.draw.height), 0);

    if (!terminal.initialized) {
#if !defined(WIN32) && !defined(HX_DOS) && !C_EMSCRIPTEN
        if (tcgetattr(STDIN_FILENO, &terminal.original_termios) != 0) {
            return fail_terminal_output("Terminal output could not read terminal attributes.");
        }
        termios raw = terminal.original_termios;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | IEXTEN | ISIG));
        raw.c_iflag &= static_cast<tcflag_t>(~(IXON | IXOFF));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        terminal.original_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (terminal.original_flags < 0)
            return fail_terminal_output("Terminal output could not read terminal file status flags.");
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
            return fail_terminal_output("Terminal output could not enable raw terminal mode.");
        terminal.raw_mode = true;
        if (fcntl(STDIN_FILENO, F_SETFL, terminal.original_flags | O_NONBLOCK) != 0)
            return fail_terminal_output("Terminal output could not enable nonblocking terminal input.");
#endif
        write_term("\033[?25l\033[2J\033[H");
        terminal.cursor_visible = false;
        terminal.initialized = true;
    }

    return OUTPUT_TERMINAL_GetBestMode(sdl.draw.flags);
}

bool OUTPUT_TERMINAL_StartUpdate(uint8_t* &pixels, Bitu &pitch)
{
    /* The render pipeline still writes a frame; terminal output reads text VRAM in EndUpdate(). */
    const auto scratch_cells = static_cast<size_t>(std::max<Bitu>(1, sdl.draw.width)) *
                               static_cast<size_t>(std::max<Bitu>(1, sdl.draw.height));
    if (terminal.dummy.size() < scratch_cells)
        terminal.dummy.assign(scratch_cells, 0);

    pixels = reinterpret_cast<uint8_t*>(terminal.dummy.data());
    pitch = std::max<Bitu>(1, sdl.draw.width) * sizeof(uint32_t);
    sdl.updating = true;
    return true;
}

void OUTPUT_TERMINAL_EndUpdate(const uint16_t *changedLines)
{
    (void)changedLines;

    if (text_mode_active()) {
        suppress_dos_console_logging();
        terminal.rendered_text_frame = true;
        terminal.warned_graphics = false;
        redraw_text();
        return;
    }

    if (!terminal.initialized || !terminal.rendered_text_frame)
        return;

    if (!terminal.warned_graphics) {
        /* Only emit the graphics warning after terminal text rendering has been established. */
        suppress_dos_console_logging();
        write_term("\033[2J\033[H\033[0m");
        write_term("WARNING: This program is using unsupported graphics output.\n"
                   "It is still running, but graphics modes cannot be displayed in the terminal.\n"
                   "Press Ctrl-C to quit DOSBox-X.\n");
        flush_term();
        terminal.cells.clear();
        terminal.warned_graphics = true;
    }
}

void OUTPUT_TERMINAL_Shutdown()
{
    if (!terminal.initialized) {
        restore_logging();
        return;
    }

    write_term("\033[0m\033[?25h\n");
    flush_term();

    restore_terminal_mode();

    restore_logging();
    terminal = TerminalState();
}

void OUTPUT_TERMINAL_InputEvent()
{
#if !defined(WIN32) && !defined(HX_DOS) && !C_EMSCRIPTEN
    static constexpr KBD_KEYS ctrl_keys[26] = {
            KBD_a, KBD_b, KBD_c, KBD_d, KBD_e, KBD_f, KBD_g, KBD_h, KBD_i,
            KBD_j, KBD_k, KBD_l, KBD_m, KBD_n, KBD_o, KBD_p, KBD_q, KBD_r,
            KBD_s, KBD_t, KBD_u, KBD_v, KBD_w, KBD_x, KBD_y, KBD_z,
    };

    char ch = 0;
    while (read(STDIN_FILENO, &ch, 1) == 1) {
        const auto uch = static_cast<uint8_t>(ch);
        if (terminal.warned_graphics && uch == 0x03)
            throw 1;

        if (uch == 0x1b) {
            std::string seq;
            seq.push_back('\033');
            char next = 0;
            for (int i = 0; i < 15 && read(STDIN_FILENO, &next, 1) == 1; ++i) {
                seq.push_back(next);
                if ((next >= 'A' && next <= 'Z') || next == '~')
                    break;
            }
            if (seq.size() == 1)
                press_key(KBD_esc);
            else
                handle_escape_sequence(seq);
        } else if (uch == '\r' || uch == '\n' || uch == '\t' ||
                   uch == '\b' || uch == 0x7f) {
            press_ascii(uch);
        } else if (uch >= 1 && uch <= 26) {
            KEYBOARD_AddKey(KBD_leftctrl, true);
            press_key(ctrl_keys[uch - 1]);
            KEYBOARD_AddKey(KBD_leftctrl, false);
        } else {
            press_ascii(uch);
        }
    }
#endif
}
