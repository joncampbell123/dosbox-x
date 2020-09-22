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

/* fluid_portaudio.c
 *
 * Drivers for the PortAudio API : www.portaudio.com
 * Implementation files for PortAudio on each platform have to be added
 *
 * Stephane Letz  (letz@grame.fr)  Grame
 * 12/20/01 Adapdation for new audio drivers
 *
 * Josh Green <jgreen@users.sourceforge.net>
 * 2009-01-28 Overhauled for PortAudio 19 API and current FluidSynth API (was broken)
 */

#include "fluid_synth.h"
#include "fluid_sys.h"
#include "fluid_settings.h"
#include "fluid_adriver.h"

#if PORTAUDIO_SUPPORT

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <portaudio.h>


/** fluid_portaudio_driver_t
 *
 * This structure should not be accessed directly. Use audio port
 * functions instead.
 */
typedef struct
{
  fluid_audio_driver_t driver;
  fluid_synth_t *synth;
  fluid_audio_callback_t read;
  PaStream *stream;
} fluid_portaudio_driver_t;

static int
fluid_portaudio_run (const void *input, void *output, unsigned long frameCount,
                     const PaStreamCallbackTimeInfo* timeInfo,
                     PaStreamCallbackFlags statusFlags, void *userData);
int delete_fluid_portaudio_driver (fluid_audio_driver_t *p);

#define PORTAUDIO_DEFAULT_DEVICE "PortAudio Default"

void
fluid_portaudio_driver_settings (fluid_settings_t *settings)
{
  const PaDeviceInfo *deviceInfo;
  int numDevices;
  PaError err;
  int i;

  fluid_settings_register_str (settings, "audio.portaudio.device", PORTAUDIO_DEFAULT_DEVICE, 0, NULL, NULL);
  fluid_settings_add_option (settings, "audio.portaudio.device", PORTAUDIO_DEFAULT_DEVICE);

  err = Pa_Initialize();

  if (err != paNoError)
  {
    FLUID_LOG (FLUID_ERR, "Error initializing PortAudio driver: %s",
               Pa_GetErrorText (err));
    return;
  }

  numDevices = Pa_GetDeviceCount();

  if (numDevices < 0)
  {
    FLUID_LOG (FLUID_ERR, "PortAudio returned unexpected device count %d", numDevices);
    return;
  }

  for (i = 0; i < numDevices; i++)
  {
    deviceInfo = Pa_GetDeviceInfo (i);
    if ( deviceInfo->maxOutputChannels >= 2 )
      fluid_settings_add_option (settings, "audio.portaudio.device",
                                 deviceInfo->name);
  }

  /* done with PortAudio for now, may get reopened later */
  err = Pa_Terminate();

  if (err != paNoError)
    printf ("PortAudio termination error: %s\n", Pa_GetErrorText (err) );
}

fluid_audio_driver_t *
new_fluid_portaudio_driver (fluid_settings_t *settings, fluid_synth_t *synth)
{
  fluid_portaudio_driver_t *dev = NULL;
  PaStreamParameters outputParams;
  char *device = NULL;
  double sample_rate;
  int period_size;
  PaError err;

  dev = FLUID_NEW (fluid_portaudio_driver_t);

  if (dev == NULL)
  {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    return NULL;
  }

  err = Pa_Initialize ();

  if (err != paNoError)
  {
    FLUID_LOG (FLUID_ERR, "Error initializing PortAudio driver: %s",
               Pa_GetErrorText (err));
    FLUID_FREE (dev);
    return NULL;
  }

  FLUID_MEMSET (dev, 0, sizeof (fluid_portaudio_driver_t));

  dev->synth = synth;

  fluid_settings_getint (settings, "audio.period-size", &period_size);
  fluid_settings_getnum (settings, "synth.sample-rate", &sample_rate);
  fluid_settings_dupstr(settings, "audio.portaudio.device", &device);   /* ++ alloc device name */

  memset (&outputParams, 0, sizeof (outputParams));
  outputParams.channelCount = 2;
  outputParams.suggestedLatency = (PaTime)period_size / sample_rate;

  /* Locate the device if specified */
  if (strcmp (device, PORTAUDIO_DEFAULT_DEVICE) != 0)
  {
    const PaDeviceInfo *deviceInfo;
    int numDevices;
    int i;

    numDevices = Pa_GetDeviceCount ();

    if (numDevices < 0)
    {
      FLUID_LOG (FLUID_ERR, "PortAudio returned unexpected device count %d", numDevices);
      goto error_recovery;
    }

    for (i = 0; i < numDevices; i++)
    {
      deviceInfo = Pa_GetDeviceInfo (i);

      if (strcmp (device, deviceInfo->name) == 0)
      {
        outputParams.device = i;
        break;
      }
    }

    if (i == numDevices)
    {
      FLUID_LOG (FLUID_ERR, "PortAudio device '%s' was not found", device);
      goto error_recovery;
    }
  }
  else outputParams.device = Pa_GetDefaultOutputDevice();

  if (fluid_settings_str_equal (settings, "audio.sample-format", "16bits"))
  {
    outputParams.sampleFormat = paInt16;
    dev->read = fluid_synth_write_s16;
  }
  else if (fluid_settings_str_equal (settings, "audio.sample-format", "float"))
  {
    outputParams.sampleFormat = paFloat32;
    dev->read = fluid_synth_write_float;
  }
  else
  {
    FLUID_LOG (FLUID_ERR, "Unknown sample format");
    goto error_recovery;
  }

  /* PortAudio section */

  /* Open an audio I/O stream. */
  err = Pa_OpenStream (&dev->stream,
                       NULL,              /* Input parameters */
                       &outputParams,
                       sample_rate,
                       period_size,
                       paNoFlag,
                       fluid_portaudio_run,
                       dev);

  if (err != paNoError)
  {
    FLUID_LOG (FLUID_ERR, "Error opening PortAudio stream: %s",
               Pa_GetErrorText (err));
    goto error_recovery;
  }

  err = Pa_StartStream (dev->stream);

  if (err != paNoError)
  {
    FLUID_LOG (FLUID_ERR, "Error starting PortAudio stream: %s",
               Pa_GetErrorText (err));
    goto error_recovery;
  }

  if (device) FLUID_FREE (device);      /* -- free device name */
  
  return (fluid_audio_driver_t *)dev;

error_recovery:
  if (device) FLUID_FREE (device);      /* -- free device name */
  delete_fluid_portaudio_driver ((fluid_audio_driver_t *)dev);
  return NULL;
}

/* PortAudio callback
 * fluid_portaudio_run
 */
static int
fluid_portaudio_run (const void *input, void *output, unsigned long frameCount,
                     const PaStreamCallbackTimeInfo* timeInfo,
                     PaStreamCallbackFlags statusFlags, void *userData)
{
  fluid_portaudio_driver_t *dev = (fluid_portaudio_driver_t *)userData;
  /* it's as simple as that: */
  dev->read (dev->synth, frameCount, output, 0, 2, output, 1, 2);
  return 0;
}

/*
 * delete_fluid_portaudio_driver
 */
int
delete_fluid_portaudio_driver(fluid_audio_driver_t *p)
{
  fluid_portaudio_driver_t* dev;
  PaError err;

  dev = (fluid_portaudio_driver_t*)p;
  if (dev == NULL) return FLUID_OK;

  /* PortAudio section */
  if (dev->stream) Pa_CloseStream (dev->stream);

  err = Pa_Terminate();

  if (err != paNoError)
    printf ("PortAudio termination error: %s\n", Pa_GetErrorText (err) );

  FLUID_FREE (dev);
  return FLUID_OK;
}

#endif /*#if PORTAUDIO_SUPPORT */
