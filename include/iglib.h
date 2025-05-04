
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
#define DOSBOX_ID_REG_VERSION_STRING                    (0x00000002UL)
#define DOSBOX_ID_REG_READ_EMTIME                       (0x00000004UL)

#define DOSBOX_ID_REG_DEBUG_OUT                         (0x0000DEB0UL)
#define DOSBOX_ID_REG_DEBUG_CLEAR                       (0x0000DEB1UL)

#define DOSBOX_ID_REG_USER_MOUSE_STATUS                 (0x00434D54UL)  /* status (cursor capture, etc) */
# define DOSBOX_ID_REG_USER_MOUSE_STATUS_CAPTURE        (0x00000001UL)

#define DOSBOX_ID_REG_USER_MOUSE_CURSOR                 (0x00434D55UL)  /* screen coordinates, pixels */
#define DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED      (0x00434D56UL)  /* screen coordinates, normalized to 0...65535 for Windows 3.x */

#define DOSBOX_ID_REG_RELEASE_MOUSE_CAPTURE             (0x0052434DUL)

#define DOSBOX_ID_CMD_GET_VGA_SIZE                      (0x006845C0UL)

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

#define DOSBOX_ID_REG_MIXER_QUERY                       (0x5158494DUL)

#define DOSBOX_ID_CMD_SET_WATCHDOG                      (0x57415444UL)

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

