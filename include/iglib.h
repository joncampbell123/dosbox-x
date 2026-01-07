
/* this header is copied to/from the doslib.h copy */

#ifndef __DOSLIB_HW_IDE_DOSBOXIGLIB_H
#define __DOSLIB_HW_IDE_DOSBOXIGLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DOSBOX_INCLUDE
  /* In order to be usable as part of a VxD, code must be combined with data.
   * In Watcom C we can do this by declaring all global variables __based on _CODE. */
# ifdef TARGET_VXD
#  define DOSBOXID_VAR __based( __segname("_CODE") )
# else
#  define DOSBOXID_VAR
# endif

extern uint16_t DOSBOXID_VAR dosbox_id_baseio;

# define DOSBOX_IDPORT(x)                                (dosbox_id_baseio+(x))
#endif

#define DOSBOX_ID_INDEX                                 (0U) /* R/W */
#define DOSBOX_ID_DATA                                  (1U) /* R/W */
#define DOSBOX_ID_STATUS                                (2U) /* R   */
#define DOSBOX_ID_COMMAND                               (2U) /*   W */

/* bits 7-6: register select byte index
 * bits 5-4: register byte index
 * bit    1: error
 * bit    0: busy */
/* DOSBOX_ID_STATUS */
#define DOSBOX_ID_STATUS_BUSY                           (0x01U)
#define DOSBOX_ID_STATUS_ERROR                          (0x02U)
#define DOSBOX_ID_STATUS_REGBYTE_SHIFT                  (4U)
#define DOSBOX_ID_STATUS_REGBYTE_MASK                   (0x03U << DOSBOX_ID_STATUS_REGBYTE_SHIFT)
#define DOSBOX_ID_STATUS_REGSEL_SHIFT                   (6U)
#define DOSBOX_ID_STATUS_REGSEL_MASK                    (0x03U << DOSBOX_ID_STATUS_REGSEL_SHIFT)

/* DOSBOX_ID_COMMAND */
#define DOSBOX_ID_CMD_RESET_LATCH                       (0x00U)
#define DOSBOX_ID_CMD_FLUSH_WRITE                       (0x01U)
#define DOSBOX_ID_CMD_PUSH_STATE                        (0x20U)
#define DOSBOX_ID_CMD_POP_STATE                         (0x21U)
#define DOSBOX_ID_CMD_DISCARD_STATE                     (0x22U)
#define DOSBOX_ID_CMD_DISCARD_ALL_STATE                 (0x23U)
#define DOSBOX_ID_CMD_CLEAR_ERROR                       (0xFEU)
#define DOSBOX_ID_CMD_RESET_INTERFACE                   (0xFFU)

/* DOSBOX_ID_DATA after reset */
#define DOSBOX_ID_RESET_DATA_CODE                       (0x0D05B0C5UL)
#define DOSBOX_ID_RESET_INDEX_CODE                      (0xAA55BB66UL)

/* DOSBOX_ID_INDEX */
#define DOSBOX_ID_REG_IDENTIFY                          (0x00000000UL)
#define DOSBOX_ID_REG_TEST                              (0x00000001UL)
#define DOSBOX_ID_REG_VERSION_STRING                    (0x00000002UL)
#define DOSBOX_ID_REG_VERSION_NUMBER                    (0x00000003UL)
#define DOSBOX_ID_REG_READ_EMTIME                       (0x00000004UL)
#define DOSBOX_ID_REG_DOSBOX_VERSION_MAJOR              (0x00000005UL)
#define DOSBOX_ID_REG_DOSBOX_VERSION_MINOR              (0x00000006UL)
#define DOSBOX_ID_REG_DOSBOX_VERSION_SUB                (0x00000007UL)
#define DOSBOX_ID_REG_DOSBOX_PLATFORM_TYPE              (0x00000008UL)  /* a code describing the platform, see src/ints/bios.cpp */
#define DOSBOX_ID_REG_DOSBOX_MACHINE_TYPE               (0x00000009UL)  /* machine type code (MCH_* constant), see include/dosbox.h */

#define DOSBOX_ID_REG_DEBUG_OUT                         (0x0000DEB0UL)
#define DOSBOX_ID_REG_DEBUG_CLEAR                       (0x0000DEB1UL)

#define DOSBOX_ID_REG_USER_MOUSE_STATUS                 (0x00434D54UL)  /* status (cursor capture, etc) */
# define DOSBOX_ID_REG_USER_MOUSE_STATUS_CAPTURE        (0x00000001UL)

#define DOSBOX_ID_REG_USER_MOUSE_CURSOR                 (0x00434D55UL)  /* screen coordinates, pixels */
#define DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED      (0x00434D56UL)  /* screen coordinates, normalized to 0...65535 for Windows 3.x */

#define DOSBOX_ID_REG_RELEASE_MOUSE_CAPTURE             (0x0052434DUL)  /* release mouse capture (W) / mouse capture status (R) */

#define DOSBOX_ID_REG_GET_VGA_MEMBASE                   (0x00684580UL)  /* get VGA video memory base in extended memory */
#define DOSBOX_ID_REG_GET_VGA_MEMSIZE                   (0x00684581UL)  /* get VGA video memory size */
#define DOSBOX_ID_REG_VGAIG_CTL                         (0x00684590UL)  /* integrated device control */
# define DOSBOX_ID_REG_VGAIG_CTL_OVERRIDE               (1UL << 0UL)    /* override the standard VGA with IG video mode */
# define DOSBOX_ID_REG_VGAIG_CTL_VGAREG_LOCKOUT         (1UL << 1UL)    /* lock out access to most VGA registers except 3DAh and DAC */
# define DOSBOX_ID_REG_VGAIG_CTL_3DA_LOCKOUT            (1UL << 2UL)    /* lock out access to 3BAh/3DAh */
# define DOSBOX_ID_REG_VGAIG_CTL_DAC_LOCKOUT            (1UL << 3UL)    /* lock out access to DAC registers */
# define DOSBOX_ID_REG_VGAIG_CTL_OVERRIDE_REFRESH       (1UL << 4UL)    /* override refresh rate */
# define DOSBOX_ID_REG_VGAIG_CTL_ACPAL_BYPASS           (1UL << 5UL)    /* for 16-color planar modes, bypass the attribute controller palette mapping */
# define DOSBOX_ID_REG_VGAIG_CTL_VBEMODESET_DISABLE     (1uL << 6UL)    /* disable VESA BIOS modesetting, intended for the Windows driver */
#define DOSBOX_ID_REG_VGAIG_DISPLAYSIZE                 (0x00684591UL)  /* integrated device width/height (16:16 = HEIGHT:WIDTH) */
#define DOSBOX_ID_REG_VGAIG_HVTOTALADD                  (0x00684592UL)  /* integrated device add to width/height to get htotal/vtotal = (16:16 V:H) */
#define DOSBOX_ID_REG_VGAIG_FMT_BYTESPERSCANLINE        (0x00684593UL)  /* integrated device fmt/bytes per scanline (16:16 FMT:BYTESPERSCANLINE) */
/* fmt codes must match enum VGA_DOSBoxIG_VidFormat << 16 */
# define DOSBOX_ID_REG_VGAIG_FMT_MASK                   (0xFFUL << 16UL)/* mask value to extract format */
# define DOSBOX_ID_REG_VGAIG_FMT_NONE                   (0x00UL << 16UL)/* none (blank screen) */
# define DOSBOX_ID_REG_VGAIG_FMT_1BPP                   (0x01UL << 16UL)/* 1bpp */
# define DOSBOX_ID_REG_VGAIG_FMT_4BPP                   (0x02UL << 16UL)/* 4bpp */
# define DOSBOX_ID_REG_VGAIG_FMT_8BPP                   (0x03UL << 16UL)/* 8bpp */
# define DOSBOX_ID_REG_VGAIG_FMT_15BPP                  (0x04UL << 16UL)/* 15bpp 1:5:5:5 */
# define DOSBOX_ID_REG_VGAIG_FMT_16BPP                  (0x05UL << 16UL)/* 16bpp 5:6:5 */
# define DOSBOX_ID_REG_VGAIG_FMT_24BPP8                 (0x06UL << 16UL)/* 24bpp 8:8:8 */
# define DOSBOX_ID_REG_VGAIG_FMT_32BPP8                 (0x07UL << 16UL)/* 32bpp 8:8:8:8 */
# define DOSBOX_ID_REG_VGAIG_FMT_32BPP10                (0x08UL << 16UL)/* 32bpp 2:10:10:10 */
# define DOSBOX_ID_REG_VGAIG_FMT_1BPP4PLANE             (0x09UL << 16UL)/* 1bpp planar */
#define DOSBOX_ID_REG_VGAIG_REFRESHRATE                 (0x00684594UL)  /* integrated device refresh rate as a 16.16 fixed point number */
#define DOSBOX_ID_REG_VGAIG_DISPLAYOFFSET               (0x00684595UL)  /* display video memory offset */
#define DOSBOX_ID_REG_VGAIG_HVPELSCALE                  (0x00684596UL)  /* h/v scale and h/v pel (8:8:8:8 = vscale:hscale:vpel:hpel) */
#define DOSBOX_ID_REG_VGAIG_BANKWINDOW                  (0x00684597UL)  /* bank switching window offset (4KB granularity) */
#define DOSBOX_ID_REG_VGAIG_ASPECTRATIO                 (0x00684598UL)  /* display aspect ratio (16:16 = h:w), set to zero for square pixels */
#define DOSBOX_ID_REG_VGAIG_CAPS                        (0x00684599UL)  /* read-only capabilities, 0 if machine type is not DOSBox IG */
# define DOSBOX_ID_REG_VGA1G_CAPS_ENABLED               (1UL << 0UL)    /* DOSBox IG is enabled (machine=svga_dosbox) */
#define DOSBOX_ID_REG_GET_VGA_SIZE                      (0x006845C0UL)  /* size of the VGA screen */
#define DOSBOX_ID_REG_GET_WINDOW_SIZE                   (0x006845FFUL)  /* size of the emulator window (to give USER_MOUSE_CURSOR meaning) */

#define DOSBOX_ID_REG_8042_KB_INJECT                    (0x00804200UL)
# define DOSBOX_ID_8042_KB_INJECT_KB                    (0x00UL << 8UL)
# define DOSBOX_ID_8042_KB_INJECT_AUX                   (0x01UL << 8UL)
# define DOSBOX_ID_8042_KB_INJECT_MOUSEBTN              (0x08UL << 8UL)
# define DOSBOX_ID_8042_KB_INJECT_MOUSEMX               (0x09UL << 8UL)
# define DOSBOX_ID_8042_KB_INJECT_MOUSEMY               (0x0AUL << 8UL)
# define DOSBOX_ID_8042_KB_INJECT_MOUSESCRW             (0x0BUL << 8UL)

#define DOSBOX_ID_REG_8042_KB_STATUS                    (0x00804201UL)
/* WARNING: bitfields may change over time! */
# define DOSBOX_ID_8042_KB_STATUS_LED_STATE_SHIFT       (0UL)
# define DOSBOX_ID_8042_KB_STATUS_LED_STATE_MASK        (0xFFUL << DOSBOX_ID_8042_KB_STATUS_LED_STATE_SHIFT)
# define DOSBOX_ID_8042_KB_STATUS_SCANSET_SHIFT         (8UL)
# define DOSBOX_ID_8042_KB_STATUS_SCANSET_MASK          (0x3UL << DOSBOX_ID_8042_KB_STATUS_SCANSET_SHIFT)
# define DOSBOX_ID_8042_KB_STATUS_RESET                 (0x1UL << 10UL)
# define DOSBOX_ID_8042_KB_STATUS_ACTIVE                (0x1UL << 11UL)
# define DOSBOX_ID_8042_KB_STATUS_SCANNING              (0x1UL << 12UL)
# define DOSBOX_ID_8042_KB_STATUS_AUXACTIVE             (0x1UL << 13UL)
# define DOSBOX_ID_8042_KB_STATUS_SCHEDULED             (0x1UL << 14UL)
# define DOSBOX_ID_8042_KB_STATUS_P60CHANGED            (0x1UL << 15UL)
# define DOSBOX_ID_8042_KB_STATUS_AUXCHANGED            (0x1UL << 16UL)
# define DOSBOX_ID_8042_KB_STATUS_CB_XLAT               (0x1UL << 17UL)
# define DOSBOX_ID_8042_KB_STATUS_PS2_MOUSE_LBTN        (0x1UL << 18UL)
# define DOSBOX_ID_8042_KB_STATUS_PS2_MOUSE_MBTN        (0x1UL << 19UL)
# define DOSBOX_ID_8042_KB_STATUS_PS2_MOUSE_RBTN        (0x1UL << 20UL)
# define DOSBOX_ID_8042_KB_STATUS_MOUSE_REPORTING       (0x1UL << 21UL)
# define DOSBOX_ID_8042_KB_STATUS_MOUSE_STREAM_MODE     (0x1UL << 22UL)
# define DOSBOX_ID_8042_KB_STATUS_MOUSE_LBTN            (0x1UL << 23UL)
# define DOSBOX_ID_8042_KB_STATUS_MOUSE_RBTN            (0x1UL << 24UL)
# define DOSBOX_ID_8042_KB_STATUS_MOUSE_MBTN            (0x1UL << 25UL)

#define DOSBOX_ID_REG_INJECT_NMI                        (0x00808602UL)

#define DOSBOX_ID_REG_CPU_CYCLES                        (0x55504300UL)  /* fixed cycle count, which may vary if max or auto */
#define DOSBOX_ID_REG_CPU_MAX_PERCENT                   (0x55504301UL)  /* for max cycles, percentage from 0 to 100 */
#define DOSBOX_ID_REG_CPU_CYCLES_INFO                   (0x55504302UL)  /* info about cycles, including fixed, auto, max */
# define DOSBOX_ID_REG_CPU_CYCLES_INFO_MODE_MASK        (0xFUL << 0UL)
# define DOSBOX_ID_REG_CPU_CYCLES_INFO_FIXED            (1UL << 0UL)
# define DOSBOX_ID_REG_CPU_CYCLES_INFO_MAX              (2UL << 0UL)
# define DOSBOX_ID_REG_CPU_CYCLES_INFO_AUTO             (3UL << 0UL)

#define DOSBOX_ID_REG_8237_INJECT_WRITE                 (0x00823700UL)
#define DOSBOX_ID_REG_8237_INJECT_READ                  (0x00823780UL)

#define DOSBOX_ID_REG_8259_INJECT_IRQ                   (0x00825900UL)
#define DOSBOX_ID_REG_8259_PIC_INFO                     (0x00825901UL)

#define DOSBOX_ID_REG_SCREENSHOT_TRIGGER                (0x00C54010UL)
/* DOSBOX_ID_REG_SCREENSHOT_TRIGGER bitfield for writing */
# define DOSBOX_ID_SCREENSHOT_IMAGE                     (1UL << 0UL)    /* trigger a screenshot. wait for vertical retrace, then read the register to check it happened */
# define DOSBOX_ID_SCREENSHOT_VIDEO                     (1UL << 1UL)    /* toggle on/off video capture */
# define DOSBOX_ID_SCREENSHOT_WAVE                      (1UL << 2UL)    /* toggle on/off WAVE capture */
/* DOSBOX_ID_REG_SCREENSHOT_TRIGGER readback */
# define DOSBOX_ID_SCREENSHOT_STATUS_IMAGE_IN_PROGRESS  (1UL << 0UL)    /* if set, DOSBox is prepared to write a screenshot on vertical retrace. will clear itself when it happens */
# define DOSBOX_ID_SCREENSHOT_STATUS_VIDEO_IN_PROGRESS  (1UL << 1UL)    /* if set, DOSBox is capturing video. */
# define DOSBOX_ID_SCREENSHOT_STATUS_WAVE_IN_PROGRESS   (1UL << 2UL)    /* if set, DOSBox is capturing WAVE audio */
# define DOSBOX_ID_SCREENSHOT_STATUS_NOT_ENABLED        (1UL << 30UL)   /* if set, DOSBox has not enabled this register. */
# define DOSBOX_ID_SCREENSHOT_STATUS_NOT_AVAILABLE      (1UL << 31UL)   /* if set, DOSBox was compiled without screenshot/video support (C_SSHOT not defined) */

#define DOSBOX_ID_REG_DOS_KERNEL_STATUS                 (0x4B6F4400UL)
#define DOSBOX_ID_REG_DOS_KERNEL_CODEPAGE               (0x4B6F4401UL)
#define DOSBOX_ID_REG_DOS_KERNEL_COUNTRY                (0x4B6F4402UL)
#define DOSBOX_ID_REG_DOS_KERNEL_VERSION_MAJOR          (0x4B6F4403UL)
#define DOSBOX_ID_REG_DOS_KERNEL_VERSION_MINOR          (0x4B6F4404UL)
#define DOSBOX_ID_REG_DOS_KERNEL_ERROR_CODE             (0x4B6F4405UL)
#define DOSBOX_ID_REG_DOS_KERNEL_BOOT_DRIVE             (0x4B6F4406UL)
#define DOSBOX_ID_REG_DOS_KERNEL_CURRENT_DRIVE          (0x4B6F4407UL)
#define DOSBOX_ID_REG_DOS_KERNEL_LFN_STATUS             (0x4B6F4408UL)

#define DOSBOX_ID_REG_MIXER_QUERY                       (0x5158494DUL)

#define DOSBOX_ID_REG_SET_WATCHDOG                      (0x57415444UL)

/* return value of DOSBOX_ID_REG_IDENTIFY */
#define DOSBOX_ID_IDENTIFICATION                        (0xD05B0740UL)

#ifndef DOSBOX_INCLUDE
static inline void dosbox_id_reset_latch() {
	outp(DOSBOX_IDPORT(DOSBOX_ID_COMMAND),DOSBOX_ID_CMD_RESET_LATCH);
}

static inline void dosbox_id_reset_interface() {
	outp(DOSBOX_IDPORT(DOSBOX_ID_COMMAND),DOSBOX_ID_CMD_RESET_INTERFACE);
}

static inline void dosbox_id_flush_write() {
	outp(DOSBOX_IDPORT(DOSBOX_ID_COMMAND),DOSBOX_ID_CMD_FLUSH_WRITE);
}

static inline uint8_t dosbox_id_read_data_nrl_u8() {
	return inp(DOSBOX_IDPORT(DOSBOX_ID_DATA));
}

static inline void dosbox_id_write_data_nrl_u8(const unsigned char c) {
	outp(DOSBOX_IDPORT(DOSBOX_ID_DATA),c);
}

static inline void dosbox_id_push_state() {
	outp(DOSBOX_IDPORT(DOSBOX_ID_COMMAND),DOSBOX_ID_CMD_PUSH_STATE);
}

static inline void dosbox_id_pop_state() {
	outp(DOSBOX_IDPORT(DOSBOX_ID_COMMAND),DOSBOX_ID_CMD_POP_STATE);
}

static inline void dosbox_id_discard_state() {
	outp(DOSBOX_IDPORT(DOSBOX_ID_COMMAND),DOSBOX_ID_CMD_DISCARD_STATE);
}

static inline void dosbox_id_discard_all_state() {
	outp(DOSBOX_IDPORT(DOSBOX_ID_COMMAND),DOSBOX_ID_CMD_DISCARD_ALL_STATE);
}

uint32_t dosbox_id_read_regsel();
void dosbox_id_write_regsel(const uint32_t reg);
uint32_t dosbox_id_read_data_nrl();
uint32_t dosbox_id_read_data();
int dosbox_id_reset();
uint32_t dosbox_id_read_identification();
int probe_dosbox_id();
int probe_dosbox_id_version_string(char *buf,size_t len);
void dosbox_id_write_data_nrl(const uint32_t val);
void dosbox_id_write_data(const uint32_t val);
void dosbox_id_debug_message(const char *str);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __DOSLIB_HW_IDE_DOSBOXIGLIB_H */

