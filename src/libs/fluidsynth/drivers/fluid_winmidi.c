/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */


/* fluid_winmidi.c
 *
 * Driver for Windows MIDI
 *
 * NOTE: Unfortunately midiInAddBuffer(), for SYSEX data, should not be called
 * from within the MIDI input callback, despite many examples contrary to that
 * on the Internet.  Some MIDI devices will deadlock.  Therefore we add MIDIHDR
 * pointers to a queue and re-add them in a separate thread, using a mutex and
 * conditional to wake up the thread.  Lame-o API! :(
 */

#include "fluidsynth_priv.h"

#if WINMIDI_SUPPORT

#include "fluid_midi.h"
#include "fluid_mdriver.h"
#include "fluid_settings.h"

#define MIDI_SYSEX_MAX_SIZE     512
#define MIDI_SYSEX_BUF_COUNT    16

typedef struct {
  fluid_midi_driver_t driver;
  HMIDIIN hmidiin;
  int closing;                  /* Set to TRUE when closing driver, to prevent endless SYSEX lockup loop */

  fluid_thread_t *sysExAddThread;       /* Thread for SYSEX re-add thread */
  fluid_cond_mutex_t *mutex;    /* Lock for condition */
  fluid_cond_t *cond;           /* Condition to signal MIDI event thread of available events */

  /* MIDI HDR for SYSEX buffer */
  MIDIHDR sysExHdrs[MIDI_SYSEX_BUF_COUNT];

  /* TRUE for each MIDIHDR buffer which should be re-added to MIDI device */
  int sysExHdrAdd[MIDI_SYSEX_BUF_COUNT];

  /* Sysex data buffer */
  unsigned char sysExBuf[MIDI_SYSEX_BUF_COUNT * MIDI_SYSEX_MAX_SIZE];

  int sysExOffset;              /* Current offset in sysex buffer (for message continuation) */
} fluid_winmidi_driver_t;

static char fluid_winmidi_error_buffer[256];

#define msg_type(_m)  ((unsigned char)(_m & 0xf0))
#define msg_chan(_m)  ((unsigned char)(_m & 0x0f))
#define msg_p1(_m)    ((_m >> 8) & 0x7f)
#define msg_p2(_m)    ((_m >> 16) & 0x7f)

fluid_midi_driver_t* new_fluid_winmidi_driver(fluid_settings_t* settings,
					    handle_midi_event_func_t handler, void* data);

int delete_fluid_winmidi_driver(fluid_midi_driver_t* p);

void CALLBACK fluid_winmidi_callback(HMIDIIN hmi, UINT wMsg, DWORD_PTR dwInstance,
				    DWORD_PTR msg, DWORD_PTR extra);
static void fluid_winmidi_add_sysex_thread (void *data);
static char* fluid_winmidi_input_error(int no);
int fluid_winmidi_driver_status(fluid_midi_driver_t* p);


void fluid_winmidi_midi_driver_settings(fluid_settings_t* settings)
{
  MMRESULT res;
  MIDIINCAPS in_caps;
  UINT i, num;	
  fluid_settings_register_str(settings, "midi.winmidi.device", "default", 0, NULL, NULL);
  num = midiInGetNumDevs();
  if (num > 0) {
    fluid_settings_add_option(settings, "midi.winmidi.device", "default");
    for (i = 0; i < num; i++) {
      res = midiInGetDevCaps(i, &in_caps, sizeof(MIDIINCAPS));
      if (res == MMSYSERR_NOERROR) {
        fluid_settings_add_option(settings, "midi.winmidi.device", in_caps.szPname);
      }
    }
  }
}

/*
 * new_fluid_winmidi_driver
 */
fluid_midi_driver_t*
new_fluid_winmidi_driver(fluid_settings_t* settings,
			handle_midi_event_func_t handler, void* data)
{
  fluid_winmidi_driver_t* dev;
  MIDIHDR *hdr;
  MMRESULT res;
  UINT i, err, num, midi_num = 0;
  MIDIINCAPS in_caps;
  char* devname = NULL;

  /* not much use doing anything */
  if (handler == NULL) {
    FLUID_LOG(FLUID_ERR, "Invalid argument");
    return NULL;
  }

  dev = FLUID_MALLOC(sizeof(fluid_winmidi_driver_t));
  if (dev == NULL) {
    return NULL;
  }

  memset (dev, 0, sizeof (fluid_winmidi_driver_t));

  dev->hmidiin = NULL;
  dev->driver.handler = handler;
  dev->driver.data = data;
  dev->closing = FALSE;

  /* get the device name. if none is specified, use the default device. */
  if(!fluid_settings_dupstr(settings, "midi.winmidi.device", &devname) || !devname) {
    devname = FLUID_STRDUP ("default");

    if (!devname)
    {
      FLUID_LOG(FLUID_ERR, "Out of memory");
      goto error_recovery;
    }
  }
  
  /* check if there any midi devices installed */
  num = midiInGetNumDevs();
  if (num == 0) {
    FLUID_LOG(FLUID_ERR, "no MIDI in devices found");
    goto error_recovery;
  }

  /* find the device */
  if (strcasecmp("default", devname) != 0) {
    for (i = 0; i < num; i++) {
      res = midiInGetDevCaps(i, &in_caps, sizeof(MIDIINCAPS));
      if (res == MMSYSERR_NOERROR) {
        FLUID_LOG(FLUID_DBG, "Testing midi device: %s\n", in_caps.szPname);
        if (strcasecmp(devname, in_caps.szPname) == 0) {
          FLUID_LOG(FLUID_DBG, "Selected midi device number: %d\n", i);
          midi_num = i;
          break;
        }
      }
    }
    if (midi_num != i) {
      FLUID_LOG(FLUID_ERR, "Device <%s> does not exists", devname);
      goto error_recovery;
    }
  }

  /* try opening the device */
  err = midiInOpen(&dev->hmidiin, midi_num,
		   (DWORD_PTR) fluid_winmidi_callback,
		   (DWORD_PTR) dev, CALLBACK_FUNCTION);
  if (err != MMSYSERR_NOERROR) {
    FLUID_LOG(FLUID_ERR, "Couldn't open MIDI input: %s (error %d)",
	     fluid_winmidi_input_error(err), err);
    goto error_recovery;
  }

  /* Prepare and add SYSEX buffers */
  for (i = 0; i < MIDI_SYSEX_BUF_COUNT; i++)
  {
    dev->sysExHdrAdd[i] = FALSE;
    hdr = &dev->sysExHdrs[i];

    hdr->lpData = &dev->sysExBuf[i * MIDI_SYSEX_MAX_SIZE];
    hdr->dwBufferLength = MIDI_SYSEX_MAX_SIZE;

    /* Prepare a buffer for SYSEX data and add it */
    err = midiInPrepareHeader (dev->hmidiin, hdr, sizeof (MIDIHDR));

    if (err == MMSYSERR_NOERROR)
    {
      err = midiInAddBuffer (dev->hmidiin, hdr, sizeof (MIDIHDR));

      if (err != MMSYSERR_NOERROR)
      {
        FLUID_LOG (FLUID_WARN, "Failed to prepare MIDI SYSEX buffer: %s (error %d)",
                   fluid_winmidi_input_error (err), err);
        midiInUnprepareHeader (dev->hmidiin, hdr, sizeof (MIDIHDR));
      }
    }
    else FLUID_LOG (FLUID_WARN, "Failed to prepare MIDI SYSEX buffer: %s (error %d)",
                    fluid_winmidi_input_error (err), err);
  }

  /* Start the MIDI input interface */
  if (midiInStart(dev->hmidiin) != MMSYSERR_NOERROR) {
    FLUID_LOG(FLUID_ERR, "Failed to start the MIDI input. MIDI input not available.");
    goto error_recovery;
  }

  /* Create mutex and condition */
  dev->mutex = new_fluid_cond_mutex ();
  dev->cond = new_fluid_cond ();

  if (!dev->mutex || !dev->cond)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    goto error_recovery;
  }

  /* Create thread which processes re-adding SYSEX buffers */
  dev->sysExAddThread = new_fluid_thread ("winmidi-sysex", fluid_winmidi_add_sysex_thread,
                                          dev, 0, FALSE);
  if (!dev->sysExAddThread)
  {
    FLUID_LOG(FLUID_ERR, "Failed to create SYSEX buffer processing thread");
    goto error_recovery;
  }

  if (devname) FLUID_FREE (devname);    /* -- free device name */

  return (fluid_midi_driver_t*) dev;

 error_recovery:
  if (devname) FLUID_FREE (devname);    /* -- free device name */
  delete_fluid_winmidi_driver((fluid_midi_driver_t*) dev);
  return NULL;
}

/*
 * delete_fluid_winmidi_driver
 */
int
delete_fluid_winmidi_driver(fluid_midi_driver_t* p)
{
  fluid_winmidi_driver_t* dev = (fluid_winmidi_driver_t*) p;
  if (dev->hmidiin != NULL) {
    fluid_atomic_int_set (&dev->closing, TRUE);

    if (dev->sysExAddThread)
    {
      fluid_cond_mutex_lock (dev->mutex);         /* ++ lock */
      fluid_cond_signal (dev->cond);
      fluid_cond_mutex_unlock (dev->mutex);       /* -- unlock */

      fluid_thread_join (dev->sysExAddThread);
    }

    midiInStop(dev->hmidiin);
    midiInReset(dev->hmidiin);
    midiInClose(dev->hmidiin);
  }

  if (dev->mutex) delete_fluid_cond_mutex (dev->mutex);
  if (dev->cond) delete_fluid_cond (dev->cond);

  FLUID_FREE(dev);
  return 0;
}

void CALLBACK
fluid_winmidi_callback(HMIDIIN hmi, UINT wMsg, DWORD_PTR dwInstance,
                       DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
  fluid_winmidi_driver_t* dev = (fluid_winmidi_driver_t *) dwInstance;
  fluid_midi_event_t event;
  LPMIDIHDR pMidiHdr;
  unsigned char *data;
  int index;
  unsigned int msg_param = (unsigned int) dwParam1;

  switch (wMsg) {
  case MIM_OPEN:
    break;

  case MIM_CLOSE:
    break;

  case MIM_DATA:
    event.type = msg_type(msg_param);
    event.channel = msg_chan(msg_param);

    if (event.type != PITCH_BEND) {
      event.param1 = msg_p1(msg_param);
      event.param2 = msg_p2(msg_param);
    } else {  /* Pitch bend is a 14 bit value */
      event.param1 = (msg_p2 (msg_param) << 7) | msg_p1 (msg_param);
      event.param2 = 0;
    }

    (*dev->driver.handler)(dev->driver.data, &event);
    break;

  case MIM_LONGDATA:    /* SYSEX data */
    if (dev->closing) break;    /* Prevent MIM_LONGDATA endless loop, don't re-add buffer if closing */

    pMidiHdr = (LPMIDIHDR)dwParam1;
    data = (unsigned char *)(pMidiHdr->lpData);

    /* We only process complete SYSEX messages (discard those that are too small or too large) */
    if (pMidiHdr->dwBytesRecorded > 2 && data[0] == 0xF0
        && data[pMidiHdr->dwBytesRecorded - 1] == 0xF7)
    {
      fluid_midi_event_set_sysex (&event, pMidiHdr->lpData + 1,
                                  pMidiHdr->dwBytesRecorded - 2, FALSE);
      (*dev->driver.handler)(dev->driver.data, &event);
    }

    index = (pMidiHdr - dev->sysExHdrs) / sizeof (MIDIHDR);
    fluid_atomic_int_set (&dev->sysExHdrAdd[index], TRUE);

    fluid_cond_mutex_lock (dev->mutex);         /* ++ lock */
    fluid_cond_signal (dev->cond);
    fluid_cond_mutex_unlock (dev->mutex);       /* -- unlock */
    break;

  case MIM_ERROR:
    break;

  case MIM_LONGERROR:
    break;

  case MIM_MOREDATA:
    break;
  }
}

/* Thread for re-adding SYSEX buffers */
static void
fluid_winmidi_add_sysex_thread (void *data)
{
  fluid_winmidi_driver_t *dev = data;
  int i;

  while (!fluid_atomic_int_get (&dev->closing))
  {
    fluid_cond_mutex_lock (dev->mutex);         /* ++ lock */
    fluid_cond_wait (dev->cond, dev->mutex);
    fluid_cond_mutex_unlock (dev->mutex);       /* -- unlock */

    for (i = 0; i < MIDI_SYSEX_BUF_COUNT; i++)
    {
      if (fluid_atomic_int_get (&dev->sysExHdrAdd[i]))
      {
        fluid_atomic_int_set (&dev->sysExHdrAdd[i], FALSE);
        midiInAddBuffer (dev->hmidiin, &dev->sysExHdrs[i], sizeof (MIDIHDR));
      }
    }
  }
}

int
fluid_winmidi_driver_status(fluid_midi_driver_t* p)
{
  return 0;
}

static char*
fluid_winmidi_input_error(int no)
{
  midiInGetErrorText(no, fluid_winmidi_error_buffer, 256);
  return fluid_winmidi_error_buffer;
}

#endif /* WINMIDI_SUPPORT */
