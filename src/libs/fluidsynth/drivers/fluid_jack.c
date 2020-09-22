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


/* fluid_jack.c
 *
 * Driver for the JACK
 *
 * This code is derived from the simple_client example in the JACK
 * source distribution. Many thanks to Paul Davis.
 *
 */

#include "fluidsynth_priv.h"
#include "fluid_sys.h"
#include "fluid_synth.h"
#include "fluid_adriver.h"
#include "fluid_mdriver.h"
#include "fluid_settings.h"

#include <jack/jack.h>
#include <jack/midiport.h>

#include "config.h"
#include "fluid_lash.h"


typedef struct _fluid_jack_audio_driver_t fluid_jack_audio_driver_t;
typedef struct _fluid_jack_midi_driver_t fluid_jack_midi_driver_t;


/* Clients are shared for drivers using the same server. */
typedef struct
{
  jack_client_t *client;
  char *server;                 /* Jack server name used */
  fluid_jack_audio_driver_t *audio_driver;
  fluid_jack_midi_driver_t *midi_driver;
} fluid_jack_client_t;

/* Jack audio driver instance */
struct _fluid_jack_audio_driver_t
{
  fluid_audio_driver_t driver;
  fluid_jack_client_t *client_ref;
  int audio_channels;
  jack_port_t **output_ports;
  int num_output_ports;
  float **output_bufs;
  fluid_audio_func_t callback;
  void* data;
};

/* Jack MIDI driver instance */
struct _fluid_jack_midi_driver_t
{
  fluid_midi_driver_t driver;
  fluid_jack_client_t *client_ref;
  jack_port_t *midi_port;
  fluid_midi_parser_t *parser;
};

static fluid_jack_client_t *new_fluid_jack_client (fluid_settings_t *settings,
                                                   int isaudio, void *driver);
static int fluid_jack_client_register_ports (void *driver, int isaudio,
                                             jack_client_t *client,
                                             fluid_settings_t *settings);
fluid_audio_driver_t*
new_fluid_jack_audio_driver2(fluid_settings_t* settings, fluid_audio_func_t func, void* data);
int delete_fluid_jack_audio_driver(fluid_audio_driver_t* p);
void fluid_jack_driver_shutdown(void *arg);
int fluid_jack_driver_srate(jack_nframes_t nframes, void *arg);
int fluid_jack_driver_bufsize(jack_nframes_t nframes, void *arg);
int fluid_jack_driver_process(jack_nframes_t nframes, void *arg);
int delete_fluid_jack_midi_driver(fluid_midi_driver_t *p);


static fluid_mutex_t last_client_mutex = FLUID_MUTEX_INIT;     /* Probably not necessary, but just in case drivers are created by multiple threads */
static fluid_jack_client_t *last_client = NULL;       /* Last unpaired client. For audio/MIDI driver pairing. */


void
fluid_jack_audio_driver_settings(fluid_settings_t* settings)
{
  fluid_settings_register_str(settings, "audio.jack.id", "fluidsynth", 0, NULL, NULL);
  fluid_settings_register_int(settings, "audio.jack.multi", 0, 0, 1, FLUID_HINT_TOGGLED, NULL, NULL);
  fluid_settings_register_int(settings, "audio.jack.autoconnect", 0, 0, 1, FLUID_HINT_TOGGLED, NULL, NULL);
  fluid_settings_register_str(settings, "audio.jack.server", "", 0, NULL, NULL);
}

/*
 * Create Jack client as necessary, share clients of the same server.
 * @param settings Settings object
 * @param isaudio TRUE if audio driver, FALSE if MIDI
 * @param driver fluid_jack_audio_driver_t or fluid_jack_midi_driver_t
 * @param data The user data instance associated with the driver (fluid_synth_t for example)
 * @return New or paired Audio/MIDI Jack client
 */
static fluid_jack_client_t *
new_fluid_jack_client (fluid_settings_t *settings, int isaudio, void *driver)
{
  fluid_jack_client_t *client_ref = NULL;
  char *server = NULL;
  char* client_name;
  char name[64];

  fluid_settings_dupstr (settings, isaudio ? "audio.jack.server"        /* ++ alloc server name */
                         : "midi.jack.server", &server);

  fluid_mutex_lock (last_client_mutex);     /* ++ lock last_client */

  /* If the last client uses the same server and is not the same type (audio or MIDI),
   * then re-use the client. */
  if (last_client && ((!last_client->server && !server)
                      || FLUID_STRCMP (last_client->server, server) == 0)
      && ((!isaudio && !last_client->midi_driver) || (isaudio && !last_client->audio_driver)))
  {
    client_ref = last_client;
    last_client = NULL;         /* No more pairing for this client */

    /* Register ports */
    if (fluid_jack_client_register_ports (driver, isaudio, client_ref->client, settings) != FLUID_OK)
      goto error_recovery;

    if (isaudio) fluid_atomic_pointer_set (&client_ref->audio_driver, driver);
    else fluid_atomic_pointer_set (&client_ref->midi_driver, driver);

    fluid_mutex_unlock (last_client_mutex);       /* -- unlock last_client */

    if (server) FLUID_FREE (server);

    return client_ref;
  }

  /* No existing client for this Jack server */
  client_ref = FLUID_NEW (fluid_jack_client_t);

  if (!client_ref)
  {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    goto error_recovery;
  }

  FLUID_MEMSET (client_ref, 0, sizeof (fluid_jack_client_t));

  fluid_settings_dupstr (settings, isaudio ? "audio.jack.id"   /* ++ alloc client name */
                         : "midi.jack.id", &client_name);

  if (client_name != NULL && client_name[0] != 0)
    snprintf(name, 64, "%s", client_name);
  else strcpy (name, "fluidsynth");

  name[63] = '\0';

  if (client_name) FLUID_FREE (client_name);    /* -- free client name */

  /* Open a connection to the Jack server and use the server name if specified */
  if (server && server[0] != '\0')
    client_ref->client = jack_client_open (name, JackServerName, NULL, server);
  else client_ref->client = jack_client_open (name, JackNullOption, NULL);

  if (!client_ref->client)
  {
    FLUID_LOG (FLUID_ERR, "Failed to connect to Jack server.");
    goto error_recovery;
  }

  jack_set_process_callback (client_ref->client, fluid_jack_driver_process, client_ref);
  jack_set_buffer_size_callback (client_ref->client, fluid_jack_driver_bufsize, client_ref);
  jack_set_sample_rate_callback (client_ref->client, fluid_jack_driver_srate, client_ref);
  jack_on_shutdown (client_ref->client, fluid_jack_driver_shutdown, client_ref);

  /* Register ports */
  if (fluid_jack_client_register_ports (driver, isaudio, client_ref->client, settings) != FLUID_OK)
    goto error_recovery;

  /* tell the JACK server that we are ready to roll */
  if (jack_activate (client_ref->client))
  {
    FLUID_LOG (FLUID_ERR, "Failed to activate Jack client");
    goto error_recovery;
  }

  /* tell the lash server our client name */
#ifdef LASH_ENABLED
  {
    int enable_lash = 0;
    fluid_settings_getint (settings, "lash.enable", &enable_lash);
    if (enable_lash)
      fluid_lash_jack_client_name (fluid_lash_client, name);
  }
#endif /* LASH_ENABLED */

  client_ref->server = server;        /* !! takes over allocation */
  server = NULL;      /* Set to NULL so it doesn't get freed below */

  last_client = client_ref;

  if (isaudio) fluid_atomic_pointer_set (&client_ref->audio_driver, driver);
  else fluid_atomic_pointer_set (&client_ref->midi_driver, driver);

  fluid_mutex_unlock (last_client_mutex);       /* -- unlock last_client */

  if (server) FLUID_FREE (server);

  return client_ref;

error_recovery:

  fluid_mutex_unlock (last_client_mutex);       /* -- unlock clients list */
  if (server) FLUID_FREE (server);  /* -- free server name */

  if (client_ref)
  {
    if (client_ref->client)
      jack_client_close (client_ref->client);

    FLUID_FREE (client_ref);
  }

  return NULL;
}

static int
fluid_jack_client_register_ports (void *driver, int isaudio, jack_client_t *client,
                                  fluid_settings_t *settings)
{
  fluid_jack_audio_driver_t *dev;
  char name[64];
  int multi;
  int i;
  int jack_srate;
  double sample_rate;

  if (!isaudio)
  {
    fluid_jack_midi_driver_t *dev = driver;

    dev->midi_port = jack_port_register (client, "midi", JACK_DEFAULT_MIDI_TYPE,
				         JackPortIsInput | JackPortIsTerminal, 0);
    if (!dev->midi_port)
    {
      FLUID_LOG (FLUID_ERR, "Failed to create Jack MIDI port");
      return FLUID_FAILED;
    }

    return FLUID_OK;
  }

  dev = driver;

  fluid_settings_getint (settings, "audio.jack.multi", &multi);

  if (multi)
  {
    /* create the two audio output ports */
    dev->num_output_ports = 1;

    dev->output_ports = FLUID_ARRAY (jack_port_t*, 2 * dev->num_output_ports);

    if (dev->output_ports == NULL)
    {
      FLUID_LOG (FLUID_PANIC, "Jack server not running?");
      return FLUID_FAILED;
    }

    dev->output_bufs = FLUID_ARRAY (float*, 2 * dev->num_output_ports);
    FLUID_MEMSET (dev->output_ports, 0, 2 * dev->num_output_ports * sizeof(jack_port_t*));

    dev->output_ports[0]
      = jack_port_register (client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    dev->output_ports[1]
      = jack_port_register (client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  }
  else
  {
    fluid_settings_getint (settings, "synth.audio-channels", &dev->num_output_ports);

    dev->output_ports = FLUID_ARRAY (jack_port_t *, 2 * dev->num_output_ports);

    if (dev->output_ports == NULL)
    {
      FLUID_LOG (FLUID_PANIC, "Out of memory");
      return FLUID_FAILED;
    }

    dev->output_bufs = FLUID_ARRAY (float *, 2 * dev->num_output_ports);

    if (dev->output_bufs == NULL)
    {
      FLUID_LOG (FLUID_PANIC, "Out of memory");
      return FLUID_FAILED;
    }

    FLUID_MEMSET (dev->output_ports, 0, 2 * dev->num_output_ports * sizeof (jack_port_t*));

    for (i = 0; i < dev->num_output_ports; i++)
    {
      sprintf(name, "l_%02d", i);
      dev->output_ports[2 * i]
        = jack_port_register (client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

      sprintf(name, "r_%02d", i);
      dev->output_ports[2 * i + 1]
        = jack_port_register (client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    }
  }


  /* Adjust sample rate to match JACK's */
  jack_srate = jack_get_sample_rate (client);
  FLUID_LOG (FLUID_DBG, "Jack engine sample rate: %lu", jack_srate);

  fluid_settings_getnum (settings, "synth.sample-rate", &sample_rate);

  if ((int)sample_rate != jack_srate) {
    FLUID_LOG(FLUID_INFO, "Jack sample rate mismatch, adjusting."
	      " (synth.sample-rate=%lu, jackd=%lu)", (int)sample_rate, jack_srate);
    fluid_settings_setnum (settings, "synth.sample-rate", jack_srate);
  }

  /* Changing sample rate is non RT, so make sure we process it and/or other things now */
  if (dev->callback == NULL) 
    fluid_synth_process_event_queue(dev->data);

  return FLUID_OK;
}

static void
fluid_jack_client_close (fluid_jack_client_t *client_ref, void *driver)
{
  if (client_ref->audio_driver == driver)
    fluid_atomic_pointer_set (&client_ref->audio_driver, NULL);
  else if (client_ref->midi_driver == driver)
    fluid_atomic_pointer_set (&client_ref->midi_driver, NULL);

  if (client_ref->audio_driver || client_ref->midi_driver)
  {
    fluid_sleep (100);  /* FIXME - Hack to make sure that resources don't get freed while Jack callback is active */
    return;
  }

  fluid_mutex_lock (last_client_mutex);

  if (client_ref == last_client)
    last_client = NULL;

  fluid_mutex_unlock (last_client_mutex);

  if (client_ref->client)
    jack_client_close (client_ref->client);

  if (client_ref->server)
    FLUID_FREE (client_ref->server);

  FLUID_FREE (client_ref);
}


fluid_audio_driver_t*
new_fluid_jack_audio_driver(fluid_settings_t* settings, fluid_synth_t* synth)
{
  return new_fluid_jack_audio_driver2(settings, NULL, synth);
}

fluid_audio_driver_t*
new_fluid_jack_audio_driver2(fluid_settings_t* settings, fluid_audio_func_t func, void* data)
{
  fluid_jack_audio_driver_t* dev = NULL;
  jack_client_t *client;
  const char ** jack_ports;     /* for looking up ports */
  int autoconnect = 0;
  int i;

  dev = FLUID_NEW(fluid_jack_audio_driver_t);
  if (dev == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }
  FLUID_MEMSET(dev, 0, sizeof(fluid_jack_audio_driver_t));

  dev->callback = func;
  dev->data = data;

  dev->client_ref = new_fluid_jack_client (settings, TRUE, dev);

  if (!dev->client_ref)
  {
    FLUID_FREE (dev);
    return NULL;
  }

  client = dev->client_ref->client;

  /* connect the ports. */


  /* FIXME: should be done by a patchbay application */

  /* find some physical ports and connect to them */
  fluid_settings_getint (settings, "audio.jack.autoconnect", &autoconnect);
  if (autoconnect) {
    jack_ports = jack_get_ports (client, NULL, NULL, JackPortIsInput|JackPortIsPhysical);
    if (jack_ports) {
      int err;
      int connected = 0;

      for (i = 0; jack_ports[i] && i<2 * dev->num_output_ports; ++i) {
        err = jack_connect (client, jack_port_name(dev->output_ports[i]), jack_ports[i]);
        if (err) {
          FLUID_LOG(FLUID_ERR, "Error connecting jack port");
        } else {
          connected++;
        }
      }

      jack_free (jack_ports);  /* free jack ports array (not the port values!) */
    } else {
      FLUID_LOG(FLUID_WARN, "Could not connect to any physical jack ports; fluidsynth is unconnected");
    }
  }

  return (fluid_audio_driver_t*) dev;
}

/*
 * delete_fluid_jack_audio_driver
 */
int
delete_fluid_jack_audio_driver(fluid_audio_driver_t* p)
{
  fluid_jack_audio_driver_t* dev = (fluid_jack_audio_driver_t*) p;

  if (dev == NULL) return 0;

  if (dev->client_ref != NULL)
    fluid_jack_client_close (dev->client_ref, dev);

  if (dev->output_bufs)
    FLUID_FREE (dev->output_bufs);

  if (dev->output_ports)
    FLUID_FREE (dev->output_ports);

  FLUID_FREE(dev);
  return 0;
}

/* Process function for audio and MIDI Jack drivers */
int
fluid_jack_driver_process (jack_nframes_t nframes, void *arg)
{
  fluid_jack_client_t *client = (fluid_jack_client_t *)arg;
  fluid_jack_audio_driver_t *audio_driver;
  fluid_jack_midi_driver_t *midi_driver;
  float *left, *right;
  int i, k;

  jack_midi_event_t midi_event;
  fluid_midi_event_t *evt;
  void *midi_buffer;
  jack_nframes_t event_count;
  jack_nframes_t event_index;
  unsigned int u;

  /* Process MIDI events first, so that they take effect before audio synthesis */
  midi_driver = fluid_atomic_pointer_get (&client->midi_driver);

  if (midi_driver)
  {
    midi_buffer = jack_port_get_buffer (midi_driver->midi_port, 0);
    event_count = jack_midi_get_event_count (midi_buffer);

    for (event_index = 0; event_index < event_count; event_index++)
    {
      jack_midi_event_get (&midi_event, midi_buffer, event_index);

      /* let the parser convert the data into events */
      for (u = 0; u < midi_event.size; u++)
      {
        evt = fluid_midi_parser_parse (midi_driver->parser, midi_event.buffer[u]);

        /* send the event to the next link in the chain */
        if (evt != NULL) midi_driver->driver.handler (midi_driver->driver.data, evt);
      }
    }
  }

  audio_driver = client->audio_driver;
  if (!audio_driver) return 0;

  if (audio_driver->callback != NULL)
  {
    for (i = 0; i < audio_driver->num_output_ports * 2; i++)
      audio_driver->output_bufs[i] = (float *)jack_port_get_buffer (audio_driver->output_ports[i], nframes);

    return (*audio_driver->callback)(audio_driver->data, nframes, 0, NULL,
                                     2 * audio_driver->num_output_ports,
                                     audio_driver->output_bufs);
  }
  else if (audio_driver->num_output_ports == 1)
  {
    left = (float*) jack_port_get_buffer (audio_driver->output_ports[0], nframes);
    right = (float*) jack_port_get_buffer (audio_driver->output_ports[1], nframes);

    fluid_synth_write_float (audio_driver->data, nframes, left, 0, 1, right, 0, 1);
  }
  else
  {
    for (i = 0, k = audio_driver->num_output_ports; i < audio_driver->num_output_ports; i++, k++) {
      audio_driver->output_bufs[i] = (float *)jack_port_get_buffer (audio_driver->output_ports[2*i], nframes);
      audio_driver->output_bufs[k] = (float *)jack_port_get_buffer (audio_driver->output_ports[2*i+1], nframes);
    }

    fluid_synth_nwrite_float (audio_driver->data, nframes, audio_driver->output_bufs,
                              audio_driver->output_bufs + audio_driver->num_output_ports, NULL, NULL);
  }

  return 0;
}

int
fluid_jack_driver_bufsize(jack_nframes_t nframes, void *arg)
{
/*   printf("the maximum buffer size is now %lu\n", nframes); */
  return 0;
}

int
fluid_jack_driver_srate(jack_nframes_t nframes, void *arg)
{
/*   printf("the sample rate is now %lu/sec\n", nframes); */
  /* FIXME: change the sample rate of the synthesizer! */
  return 0;
}

void
fluid_jack_driver_shutdown(void *arg)
{
//  fluid_jack_audio_driver_t* dev = (fluid_jack_audio_driver_t*) arg;
  FLUID_LOG(FLUID_ERR, "Help! Lost the connection to the JACK server");
/*   exit (1); */
}


void fluid_jack_midi_driver_settings (fluid_settings_t *settings)
{
  fluid_settings_register_str (settings, "midi.jack.id", "fluidsynth-midi", 0, NULL, NULL);
  fluid_settings_register_str (settings, "midi.jack.server", "", 0, NULL, NULL);
}

/*
 * new_fluid_jack_midi_driver
 */
fluid_midi_driver_t *
new_fluid_jack_midi_driver (fluid_settings_t *settings,
			    handle_midi_event_func_t handler, void *data)
{
  fluid_jack_midi_driver_t* dev;

  fluid_return_val_if_fail (handler != NULL, NULL);

  /* allocate the device */
  dev = FLUID_NEW (fluid_jack_midi_driver_t);

  if (dev == NULL)
  {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    return NULL;
  }

  FLUID_MEMSET (dev, 0, sizeof (fluid_jack_midi_driver_t));

  dev->driver.handler = handler;
  dev->driver.data = data;

  /* allocate one event to store the input data */
  dev->parser = new_fluid_midi_parser ();

  if (dev->parser == NULL)
  {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    FLUID_FREE (dev);
    return NULL;
  }

  dev->client_ref = new_fluid_jack_client (settings, FALSE, dev);

  if (!dev->client_ref)
  {
    FLUID_FREE (dev);
    return NULL;
  }

  return (fluid_midi_driver_t *)dev;
}

int
delete_fluid_jack_midi_driver(fluid_midi_driver_t *p)
{
  fluid_jack_midi_driver_t *dev = (fluid_jack_midi_driver_t *)p;

  if (dev == NULL) return FLUID_OK;

  if (dev->client_ref != NULL)
    fluid_jack_client_close (dev->client_ref, dev);

  if (dev->parser != NULL)
    delete_fluid_midi_parser (dev->parser);

  FLUID_FREE (dev);

  return FLUID_OK;
}
