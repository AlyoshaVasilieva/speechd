
/*
 * flite.c - Speech Dispatcher backend for Flite (Festival Lite)
 *
 * Copyright (C) 2001, 2002, 2003, 2007 Brailcom, o.p.s.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * $Id: flite.c,v 1.58 2008-02-23 07:48:32 lloehrer Exp $
 */


#include <flite/flite.h>
#include "spd_audio.h"

#include "fdset.h"

#include "module_utils.h"

#define MODULE_NAME     "flite"
#define MODULE_VERSION  "0.5"

#define DEBUG_MODULE 1
DECLARE_DEBUG();

/* Thread and process control */
static int flite_speaking = 0;

static pthread_t flite_speak_thread;
static sem_t *flite_semaphore;

static char **flite_message;
static EMessageType flite_message_type;

static int flite_position = 0;
static int flite_pause_requested = 0;

signed int flite_volume = 0;

/* Internal functions prototypes */
static void flite_set_rate(signed int rate);
static void flite_set_pitch(signed int pitch);
static void flite_set_volume(signed int pitch);
static void flite_set_voice(EVoiceType voice);

static void flite_strip_silence(AudioTrack*);
static void* _flite_speak(void*);

/* Voice */
cst_voice *register_cmu_us_kal();
cst_voice *flite_voice;

AudioID *flite_audio_id = NULL;
AudioOutputType flite_audio_output_method;
char *flite_pars[10];
int flite_stop = 0;


MOD_OPTION_1_INT(FliteMaxChunkLength);
MOD_OPTION_1_STR(FliteDelimiters);

MOD_OPTION_1_STR(FliteAudioOutputMethod);
MOD_OPTION_1_STR(FliteOSSDevice);
MOD_OPTION_1_STR(FliteNASServer);
MOD_OPTION_1_STR(FliteALSADevice);
MOD_OPTION_1_STR(FlitePulseServer);
MOD_OPTION_1_INT(FlitePulseMaxLength);
MOD_OPTION_1_INT(FlitePulseTargetLength);
MOD_OPTION_1_INT(FlitePulsePreBuffering);
MOD_OPTION_1_INT(FlitePulseMinRequest);

/* Public functions */

int
module_load(void)
{
   INIT_SETTINGS_TABLES();

   REGISTER_DEBUG();

   MOD_OPTION_1_INT_REG(FliteMaxChunkLength, 300);
   MOD_OPTION_1_STR_REG(FliteDelimiters, ".");

   MOD_OPTION_1_STR_REG(FliteAudioOutputMethod, "oss");
   MOD_OPTION_1_STR_REG(FliteOSSDevice, "/dev/dsp");
   MOD_OPTION_1_STR_REG(FliteNASServer, NULL);
   MOD_OPTION_1_STR_REG(FliteALSADevice, "default");
   MOD_OPTION_1_STR_REG(FlitePulseServer, "default");
   MOD_OPTION_1_INT_REG(FlitePulseMaxLength, 132300);
   MOD_OPTION_1_INT_REG(FlitePulseTargetLength, 4410);
   MOD_OPTION_1_INT_REG(FlitePulsePreBuffering, 2200);
   MOD_OPTION_1_INT_REG(FlitePulseMinRequest, 880);

   return 0;
}

#define ABORT(msg) g_string_append(info, msg); \
        DBG("FATAL ERROR:", info->str); \
	*status_info = info->str; \
	g_string_free(info, 0); \
	return -1;

int
module_init(char **status_info)
{
    int ret;
    char *error;
    GString *info;

    DBG("Module init");
    INIT_INDEX_MARKING();

    *status_info = NULL;
    info = g_string_new("");

    /* Init flite and register a new voice */
    flite_init();
    flite_voice = register_cmu_us_kal();

    if (flite_voice == NULL){
        DBG("Couldn't register the basic kal voice.\n");
	*status_info = strdup("Can't register the basic kal voice. "
			      "Currently only kal is supported. Seems your FLite "
			      "installation is incomplete.");
        return -1;
    }

    DBG("Opening audio");
    if (!strcmp(FliteAudioOutputMethod, "oss")){
	DBG("Using OSS sound output.");
	flite_pars[0] = strdup(FliteOSSDevice);
	flite_pars[1] = NULL;
	flite_audio_id = spd_audio_open(AUDIO_OSS, (void**) flite_pars, &error);
	flite_audio_output_method = AUDIO_OSS;
    }
    else if (!strcmp(FliteAudioOutputMethod, "nas")){
	DBG("Using NAS sound output.");
	flite_pars[0] = FliteNASServer;
	flite_pars[1] = NULL;
	flite_audio_id = spd_audio_open(AUDIO_NAS, (void**) flite_pars, &error);
	flite_audio_output_method = AUDIO_NAS;
    }
    else if (!strcmp(FliteAudioOutputMethod, "alsa")){
	DBG("Using ALSA sound output.");
	flite_pars[0] = FliteALSADevice;
	flite_pars[1] = NULL;
	flite_audio_id = spd_audio_open(AUDIO_ALSA, (void**) flite_pars, &error);
	flite_audio_output_method = AUDIO_ALSA;
    }
    else if (!strcmp(FliteAudioOutputMethod, "pulse")){
	DBG("Using PulseAudio sound output.");
	flite_pars[0] = (void *) FlitePulseServer;
	flite_pars[1] = (void *) FlitePulseMaxLength;
	flite_pars[2] = (void *) FlitePulseTargetLength;
	flite_pars[3] = (void *) FlitePulsePreBuffering;
	flite_pars[4] = (void *) FlitePulseMinRequest;
	flite_pars[5] = NULL;
	flite_audio_id = spd_audio_open(AUDIO_PULSE, (void**) flite_pars, &error);
	flite_audio_output_method = AUDIO_PULSE;
    }
    else{	
	ABORT("Sound output method specified in configuration not supported. "
	      "Please choose 'oss' or 'nas'.");
    }
    if (flite_audio_id == NULL){
	g_string_append_printf(info, "Opening sound device failed. Reason: %s. ", error);
	ABORT("Can't open sound device.");
    }

    DBG("FliteMaxChunkLength = %d\n", FliteMaxChunkLength);
    DBG("FliteDelimiters = %s\n", FliteDelimiters);

    flite_message = malloc (sizeof (char*));
    *flite_message = NULL;

    flite_semaphore = module_semaphore_init();

    DBG("Flite: creating new thread for flite_speak\n");
    flite_speaking = 0;
    ret = pthread_create(&flite_speak_thread, NULL, _flite_speak, NULL);
    if(ret != 0){
        DBG("Flite: thread failed\n");
	*status_info = strdup("The module couldn't initialize threads "
			      "This could be either an internal problem or an "
			      "architecture problem. If you are sure your architecture "
			      "supports threads, please report a bug.");
        return -1;
    }

    *status_info = strdup("Flite initialized successfully.");

    return 0;
}
#undef ABORT

VoiceDescription**
module_list_voices(void)
{
  return NULL;
}

int
module_speak(gchar *data, size_t bytes, EMessageType msgtype)
{
    DBG("write()\n");

    if (flite_speaking){
        DBG("Speaking when requested to write");
        return 0;
    }

    if(module_write_data_ok(data) != 0) return -1;

    DBG("Requested data: |%s|\n", data);

    if (*flite_message != NULL){
	xfree(*flite_message);
	*flite_message = NULL;
    }
    *flite_message = module_strip_ssml(data);
    flite_message_type = MSGTYPE_TEXT;
	
    /* Setting voice */
    UPDATE_PARAMETER(voice, flite_set_voice);
    UPDATE_PARAMETER(rate, flite_set_rate);
    UPDATE_PARAMETER(volume, flite_set_volume);
    UPDATE_PARAMETER(pitch, flite_set_pitch);

    /* Send semaphore signal to the speaking thread */
    flite_speaking = 1;    
    sem_post(flite_semaphore);    

    DBG("Flite: leaving write() normally\n\r");
    return bytes;
}

int
module_stop(void) 
{
    int ret;
    DBG("flite: stop()\n");

    flite_stop = 1;
    if (flite_audio_id){
	DBG("Stopping audio");
	ret = spd_audio_stop(flite_audio_id);
	if (ret != 0) DBG("WARNING: Non 0 value from spd_audio_stop: %d", ret);
    }

    return 0;
}

size_t
module_pause(void)
{
    DBG("pause requested\n");
    if(flite_speaking){
        DBG("Flite doesn't support pause, stopping\n");

	module_stop();

        return -1;
    }else{
        return 0;
    }
}

void
module_close(int status)
{

    DBG("flite: close()\n");

    DBG("Stopping speech");
    if(flite_speaking){
        module_stop();
    }

    DBG("Terminating threads");
    if (module_terminate_thread(flite_speak_thread) != 0)
        exit(1);

    xfree(flite_voice);

    DBG("Closing audio output");
    spd_audio_close(flite_audio_id);

    exit(status);
}

/* Internal functions */

void 
flite_strip_silence(AudioTrack* track)
{
  int playlen, skip;

  float stretch = get_param_float(flite_voice->features, "duration_stretch", 1.);
  int speed = (int) (1000./stretch);
	skip = (187 * track->sample_rate) / speed;
	playlen = track->num_samples - skip * 2;
	if (playlen > 0 && playlen < 500) playlen += (skip * 2) / 3;
	if (playlen < 0) playlen = 0;

  track->num_samples = playlen;
  assert(track->bits == 16);
  track->samples += skip * track->num_channels;
}

void*
_flite_speak(void* nothing)
{	
    AudioTrack track;
    cst_wave *wav;
    unsigned int pos;
    char *buf;
    int bytes;
    int ret;

    DBG("flite: speaking thread starting.......\n");

    set_speaking_thread_parameters();

    while(1){
        sem_wait(flite_semaphore);
        DBG("Semaphore on\n");

	flite_stop = 0;
	flite_speaking = 1;

	spd_audio_set_volume(flite_audio_id, flite_volume);

	/* TODO: free(buf) */
	buf = (char*) malloc((FliteMaxChunkLength+1) * sizeof(char));
	pos = 0;
	module_report_event_begin();
	while(1){
	    if (flite_stop){
		DBG("Stop in child, terminating");
		flite_speaking = 0;
		module_report_event_stop();
		break;
	    }
	    bytes = module_get_message_part(*flite_message, buf, &pos, 
					    FliteMaxChunkLength, FliteDelimiters);

	    if (bytes < 0){
		DBG("End of message");
		flite_speaking = 0;
		module_report_event_end();
		break;
	    }

	    buf[bytes] = 0;
	    DBG("Returned %d bytes from get_part\n", bytes);
	    DBG("Text to synthesize is '%s'\n", buf);

	    if (flite_pause_requested && (current_index_mark!=-1)){
		DBG("Pause requested in parent, position %d\n", current_index_mark);
		flite_pause_requested = 0;
		flite_position = current_index_mark;
		break;
	    }

	    if (bytes > 0){
		DBG("Speaking in child...");

		DBG("Trying to synthesize text");
		wav = flite_text_to_wave(buf, flite_voice);

		if (wav == NULL){
		    DBG("Stop in child, terminating");
		    flite_speaking = 0;
		    module_report_event_stop();
		    break;
		}

		track.num_samples = wav->num_samples;
		track.num_channels = wav->num_channels;
		track.sample_rate = wav->sample_rate;
		track.bits = 16;
		track.samples = wav->samples;
		flite_strip_silence(&track);

		DBG("Got %d samples", track.num_samples);
		if (track.samples != NULL){
		    if (flite_stop){
			DBG("Stop in child, terminating");
			flite_speaking = 0;
			module_report_event_stop();
			delete_wave(wav);
			break;
		    }
		    DBG("Playing part of the message");
		    switch (audio_endian){
		        case SPD_AUDIO_LE:
		            ret = spd_audio_play(flite_audio_id, track, SPD_AUDIO_LE);
		            break;
		        case SPD_AUDIO_BE:
		            ret = spd_audio_play(flite_audio_id, track, SPD_AUDIO_BE);
		            break;
		    }
		    if (ret < 0) DBG("ERROR: spd_audio failed to play the track");
		    if (flite_stop){
			DBG("Stop in child, terminating (s)");
			flite_speaking = 0;
			module_report_event_stop();
			delete_wave(wav);
			break;
		    }
		}
		delete_wave(wav);
	    }
	    else if (bytes == -1){
		DBG("End of data in speaking thread");
		flite_speaking = 0;
		module_report_event_end();
		break;
	    }else{
		flite_speaking = 0;
		module_report_event_end();
		break;
	    }

	    if (flite_stop){
		DBG("Stop in child, terminating");
		flite_speaking = 0;
		module_report_event_stop();
		break;
	    }
	}
	flite_stop = 0;
	xfree(buf);
    }

    flite_speaking = 0;

    DBG("flite: speaking thread ended.......\n");

    pthread_exit(NULL);
}

static void
flite_set_rate(signed int rate)
{
    float stretch = 1;

    assert(rate >= -100 && rate <= +100);
    if (rate < 0) stretch -= ((float) rate) / 50;
    if (rate > 0) stretch -= ((float) rate) / 175;
    feat_set_float(flite_voice->features, "duration_stretch",
                   stretch);
}

static void
flite_set_volume(signed int volume)
{
    assert(volume >= -100 && volume <= +100);
    flite_volume = volume;
}

static void
flite_set_pitch(signed int pitch)
{
    float f0;

    assert(pitch >= -100 && pitch <= +100);
    f0 = ( ((float) pitch) * 0.8 ) + 100;
    feat_set_float(flite_voice->features, "int_f0_target_mean", f0);
}

static void
flite_set_voice(EVoiceType voice)
{
    if (voice == MALE1){
        free(flite_voice);
        flite_voice = (cst_voice*) register_cmu_us_kal();
    }
}


#include "module_main.c"
