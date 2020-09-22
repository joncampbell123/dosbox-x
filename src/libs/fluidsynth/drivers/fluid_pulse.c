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

/* fluid_pulse.c
 *
 * Audio driver for PulseAudio.
 *
 */

#include "fluid_synth.h"
#include "fluid_adriver.h"
#include "fluid_settings.h"

#include "config.h"

#include <pulse/simple.h>
#include <pulse/error.h>

/** fluid_pulse_audio_driver_t
 *
 * This structure should not be accessed directly. Use audio port
 * functions instead.
 */
typedef struct {
  fluid_audio_driver_t driver;
  pa_simple *pa_handle;
  fluid_audio_func_t callback;
  void* data;
  int buffer_size;
  fluid_thread_t *thread;
  int cont;
} fluid_pulse_audio_driver_t;


fluid_audio_driver_t* new_fluid_pulse_audio_driver(fluid_settings_t* settings,
						   fluid_synth_t* synth);
fluid_audio_driver_t* new_fluid_pulse_audio_driver2(fluid_settings_t* settings,
						    fluid_audio_func_t func, void* data);
int delete_fluid_pulse_audio_driver(fluid_audio_driver_t* p);
void fluid_pulse_audio_driver_settings(fluid_settings_t* settings);
static void fluid_pulse_audio_run(void* d);
static void fluid_pulse_audio_run2(void* d);


void fluid_pulse_audio_driver_settings(fluid_settings_t* settings)
{
  fluid_settings_register_str(settings, "audio.pulseaudio.server", "default", 0, NULL, NULL);
  fluid_settings_register_str(settings, "audio.pulseaudio.device", "default", 0, NULL, NULL);
  fluid_settings_register_str(settings, "audio.pulseaudio.media-role", "music", 0, NULL, NULL);
  fluid_settings_register_int(settings, "audio.pulseaudio.adjust-latency", 1, 0, 1,
                              FLUID_HINT_TOGGLED, NULL, NULL);
}


fluid_audio_driver_t*
new_fluid_pulse_audio_driver(fluid_settings_t* settings,
			    fluid_synth_t* synth)
{
  return new_fluid_pulse_audio_driver2(settings, NULL, synth);
}

fluid_audio_driver_t*
new_fluid_pulse_audio_driver2(fluid_settings_t* settings,
			     fluid_audio_func_t func, void* data)
{
  fluid_pulse_audio_driver_t* dev;
  pa_sample_spec samplespec;
  pa_buffer_attr bufattr;
  double sample_rate;
  int period_size, period_bytes, adjust_latency;
  char *server = NULL;
  char *device = NULL;
  char *media_role = NULL;
  int realtime_prio = 0;
  int err;

  dev = FLUID_NEW(fluid_pulse_audio_driver_t);
  if (dev == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  FLUID_MEMSET(dev, 0, sizeof(fluid_pulse_audio_driver_t));

//  fluid_settings_getint(settings, "audio.periods", &periods);
  fluid_settings_getint(settings, "audio.period-size", &period_size);
  fluid_settings_getnum(settings, "synth.sample-rate", &sample_rate);
  fluid_settings_dupstr(settings, "audio.pulseaudio.server", &server);  /* ++ alloc server string */
  fluid_settings_dupstr(settings, "audio.pulseaudio.device", &device);  /* ++ alloc device string */
  fluid_settings_dupstr(settings, "audio.pulseaudio.media-role", &media_role);  /* ++ alloc media-role string */
  fluid_settings_getint(settings, "audio.realtime-prio", &realtime_prio);
  fluid_settings_getint(settings, "audio.pulseaudio.adjust-latency", &adjust_latency);

  #define PULSE_ENV "PULSE_PROP_media.role"

  if (media_role != NULL) {
    if (strcmp(media_role, "") != 0) {
      #ifdef HAVE_SETENV
      setenv(PULSE_ENV, media_role, TRUE);
      #else
      char* buf = FLUID_MALLOC(strlen(media_role) + 1 + sizeof(PULSE_ENV));
      if (buf == NULL) {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
      }
      memcpy(buf, PULSE_ENV, sizeof(PULSE_ENV)-1);
      buf[sizeof(PULSE_ENV)-1] = '=';
      memcpy(buf+sizeof(PULSE_ENV), media_role, strlen(media_role)+1);
      putenv(buf);
      #endif
    }
    FLUID_FREE (media_role);      /* -- free media_role string */
  }

  if (server && strcmp (server, "default") == 0)
  {
    FLUID_FREE (server);        /* -- free server string */
    server = NULL;
  }

  if (device && strcmp (device, "default") == 0)
  {
    FLUID_FREE (device);        /* -- free device string */
    device = NULL;
  }

  dev->data = data;
  dev->callback = func;
  dev->cont = 1;
  dev->buffer_size = period_size;

  samplespec.format = PA_SAMPLE_FLOAT32NE;
  samplespec.channels = 2;
  samplespec.rate = sample_rate;

  period_bytes = period_size * sizeof (float) * 2;
  bufattr.maxlength = adjust_latency ? -1 : period_bytes;
  bufattr.tlength = period_bytes;
  bufattr.minreq = -1;
  bufattr.prebuf = -1;    /* Just initialize to same value as tlength */
  bufattr.fragsize = -1;  /* Not used */

  dev->pa_handle = pa_simple_new (server, "FluidSynth", PA_STREAM_PLAYBACK,
				  device, "FluidSynth output", &samplespec,
				  NULL, /* pa_channel_map */
				  &bufattr,
				  &err);

  if (!dev->pa_handle)
  {
    FLUID_LOG(FLUID_ERR, "Failed to create PulseAudio connection");
    goto error_recovery;
  }

  FLUID_LOG(FLUID_INFO, "Using PulseAudio driver");

  /* Create the audio thread */
  dev->thread = new_fluid_thread ("pulse-audio", func ? fluid_pulse_audio_run2 : fluid_pulse_audio_run,
                                  dev, realtime_prio, FALSE);
  if (!dev->thread)
    goto error_recovery;

  if (server) FLUID_FREE (server);      /* -- free server string */
  if (device) FLUID_FREE (device);      /* -- free device string */

  return (fluid_audio_driver_t*) dev;

 error_recovery:
  if (server) FLUID_FREE (server);      /* -- free server string */
  if (device) FLUID_FREE (device);      /* -- free device string */
  delete_fluid_pulse_audio_driver((fluid_audio_driver_t*) dev);
  return NULL;
}

int delete_fluid_pulse_audio_driver(fluid_audio_driver_t* p)
{
  fluid_pulse_audio_driver_t* dev = (fluid_pulse_audio_driver_t*) p;

  if (dev == NULL) {
    return FLUID_OK;
  }

  dev->cont = 0;

  if (dev->thread)
    fluid_thread_join (dev->thread);

  if (dev->pa_handle)
    pa_simple_free(dev->pa_handle);

  FLUID_FREE(dev);

  return FLUID_OK;
}

/* Thread without audio callback, more efficient */
static void
fluid_pulse_audio_run(void* d)
{
  fluid_pulse_audio_driver_t* dev = (fluid_pulse_audio_driver_t*) d;
  float *buf;
  int buffer_size;
  int err;

  buffer_size = dev->buffer_size;

  /* FIXME - Probably shouldn't alloc in run() */
  buf = FLUID_ARRAY(float, buffer_size * 2);

  if (buf == NULL)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory.");
    return;
  }

  while (dev->cont)
  {
    fluid_synth_write_float(dev->data, buffer_size, buf, 0, 2, buf, 1, 2);

    if (pa_simple_write (dev->pa_handle, buf,
			 buffer_size * sizeof (float) * 2, &err) < 0)
    {
      FLUID_LOG(FLUID_ERR, "Error writing to PulseAudio connection.");
      break;
    }
  }	/* while (dev->cont) */

  FLUID_FREE(buf);
}

static void
fluid_pulse_audio_run2(void* d)
{
  fluid_pulse_audio_driver_t* dev = (fluid_pulse_audio_driver_t*) d;
  fluid_synth_t *synth = (fluid_synth_t *)(dev->data);
  float *left, *right, *buf;
  float* handle[2];
  int buffer_size;
  int err;
  int i;

  buffer_size = dev->buffer_size;

  /* FIXME - Probably shouldn't alloc in run() */
  left = FLUID_ARRAY(float, buffer_size);
  right = FLUID_ARRAY(float, buffer_size);
  buf = FLUID_ARRAY(float, buffer_size * 2);

  if (left == NULL || right == NULL || buf == NULL)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory.");
    return;
  }

  handle[0] = left;
  handle[1] = right;

  while (dev->cont)
  {
    (*dev->callback)(synth, buffer_size, 0, NULL, 2, handle);

    /* Interleave the floating point data */
    for (i = 0; i < buffer_size; i++)
    {
      buf[i * 2] = left[i];
      buf[i * 2 + 1] = right[i];
    }

    if (pa_simple_write (dev->pa_handle, buf,
			 buffer_size * sizeof (float) * 2, &err) < 0)
    {
      FLUID_LOG(FLUID_ERR, "Error writing to PulseAudio connection.");
      break;
    }
  }	/* while (dev->cont) */

  FLUID_FREE(left);
  FLUID_FREE(right);
  FLUID_FREE(buf);
}
