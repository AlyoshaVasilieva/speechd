/*
 * libao.c -- The libao backend for the spd_audio library.
 *
 * Author: Marco Skambraks <marco@openblinux.de>
 * Date:  2009-12-15
 *
 * Copied from Luke Yelavich's libao.c driver, and merged with code from
 * Marco's ao_pulse.c driver, by Bill Cox, Dec 21, 2009.
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Leser General Public License as published by the Free
 * Software Foundation; either version 2.1, or (at your option) any later
 * version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this package; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdarg.h>

/* send a packet of XXX bytes to the sound device */
#define PULSE_SEND_BYTES 256

/* This is the smallest audio sound we are expected to play immediately without buffering. */
#define PA_MIN_AUDIO_LENTH 100

static FILE *pulseDebugFile = NULL;

/* Write to /tmp/speech-dispatcher-pulse.log */
#ifdef DEBUG_PULSE
static void MSG(char *message, ...)
{
    va_list ap;

    if(pulseDebugFile == NULL) {
        pulseDebugFile = fopen ("/tmp/speech-dispatcher-pulse.log", "w");
    }
    va_start(ap, message);
    vfprintf(pulseDebugFile, message, ap);
    va_end(ap);
    fflush(pulseDebugFile);
}
#else
static void MSG(char *message, ...)
{
}
#endif

int pulse_stop (AudioID * id);

int pulse_open (AudioID * id, void **pars);

int pulse_close (AudioID * id);

int pulse_play (AudioID * id, AudioTrack track);

int pulse_set_volume (AudioID * id, int volume);

static volatile int pulse_stop_playback = 0;

static pa_simple *s = NULL;

int pulse_open (AudioID * id, void **pars)
{
    return 0;
}

int pulse_play (AudioID * id, AudioTrack track)
{
    int bytes_per_sample;
    int num_bytes;
    int outcnt = 0;
    signed short *output_samples;
    int i;
    pa_sample_spec ss;
    pa_buffer_attr buffAttr;
    int error;

    if(id == NULL) {
        return -1;
    }
    if(track.samples == NULL || track.num_samples <= 0) {
        return 0;
    }
    MSG("Starting playback\n");
    /* Choose the correct format */
    if(track.bits == 16){	
        bytes_per_sample = 2;
    } else if(track.bits == 8){
        bytes_per_sample = 1;
    } else {
        MSG("ERROR: Unsupported sound data format, track.bits = %d\n", track.bits);		
        return -1;
    }
    output_samples = track.samples;
    num_bytes = track.num_samples * bytes_per_sample;
    if(s == NULL) {
        /* Choose the correct format */
        ss.rate = track.sample_rate;
        ss.channels = track.num_channels;
        if(bytes_per_sample == 2) {
            ss.format = PA_SAMPLE_S16LE;
        } else {
            ss.format = PA_SAMPLE_U8;
        }
        /* Set prebuf to one sample so that keys are spoken as soon as typed rather than delayed until the next key pressed */
        buffAttr.maxlength = (uint32_t)-1;
        //buffAttr.tlength = (uint32_t)-1; - this is the default, which causes key echo to not work properly.
        buffAttr.tlength = PA_MIN_AUDIO_LENTH;
        buffAttr.prebuf = (uint32_t)-1;
        buffAttr.minreq = (uint32_t)-1;
        buffAttr.fragsize = (uint32_t)-1;
        if(!(s = pa_simple_new(NULL, "speech-dispatcher", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, &buffAttr, &error))) {
            fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
            return 1;
        }
    }
    MSG("bytes to play: %d, (%f secs)\n", num_bytes, (((float) (num_bytes) / 2) / (float) track.sample_rate));
    pulse_stop_playback = 0;
    outcnt = 0;
    i = 0;
    while((outcnt < num_bytes) && !pulse_stop_playback) {
       if((num_bytes - outcnt) > PULSE_SEND_BYTES) {
           i = PULSE_SEND_BYTES;
       } else {
           i = (num_bytes - outcnt);
       }
       if(pa_simple_write(s, ((char *)output_samples) + outcnt, i, &error) < 0) {
           pa_simple_drain(s, NULL);
           pa_simple_free(s);
           s = NULL;
           MSG("ERROR: Audio: pulse_play(): %s - closing device - re-open it in next run\n", pa_strerror(error));
       } else {
           MSG("Pulse: wrote %u bytes\n", i);
       }
       outcnt += i;
    }
    return 0;
}

/* stop the pulse_play() loop */
int pulse_stop (AudioID * id)
{
    pulse_stop_playback = 1;
    return 0;
}

int pulse_close (AudioID * id)
{
    if(s != NULL) {
        pa_simple_drain(s, NULL);
        pa_simple_free(s);
        s = NULL;
    }
    return 0;
}

int pulse_set_volume (AudioID * id, int volume)
{
    return 0;
}

/* Provide the pulse backend. */
AudioFunctions pulse_functions = {pulse_open, pulse_play, pulse_stop, pulse_close, pulse_set_volume};
