/* DOSBox-X keymap handling */
#ifndef __DOSBOX_X_KEYMAP
#define __DOSBOX_X_KEYMAP

/* these enumerations are meant to represent the host OS keyboard map,
 * as well as the keymap used by the mapper interface */
enum {
    // #0
    DKM_US=0,           // US keyboard layout
    DKM_DEU,            // German keyboard layout (one concerned user, in issue tracker)
    DKM_JPN_PC98,       // Japanese PC98 keyboard layout (for PC-98 emulation)
    DKM_JPN,            // Japanese keyboard layout (one concerned user, in issue tracker, with suggestion for mapping Ro)

    DKM_MAX
};

const char *DKM_to_string(const unsigned int dkm);
const char *DKM_to_descriptive_string(const unsigned int dkm);

#endif //__DOSBOX_X_KEYMAP

