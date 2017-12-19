
#include "dosbox.h"
#include "setup.h"
#include "video.h"
#include "pic.h"
#include "vga.h"
#include "inout.h"
#include "programs.h"
#include "support.h"
#include "setup.h"
#include "timer.h"
#include "mem.h"
#include "util_units.h"
#include "control.h"
#include "pc98_cg.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"
#include "mixer.h"

bool                        gdc_5mhz_mode = false;
bool                        GDC_vsync_interrupt = false;
uint8_t                     GDC_display_plane = false;

