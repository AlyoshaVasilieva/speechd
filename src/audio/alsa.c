
/*
 * alsa.c -- The Advanced Linux Sound System backend for Speech Dispatcher
 *
 * Copyright (C) 2004,2005 Brailcom, o.p.s.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: alsa.c,v 1.10 2005-08-01 09:53:08 hanke Exp $
 */

/* NOTE: This module uses the non-blocking write() / poll() approach to
    alsa-lib functions.*/

#ifndef timersub
#define	timersub(a, b, result) \
do { \
         (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	 (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
	 if ((result)->tv_usec < 0) { \
		 --(result)->tv_sec; \
		 (result)->tv_usec += 1000000; \
	 } \
 } while (0)
#endif

/* Put a message into the logfile (stderr) */
#define MSG(arg...) \
 { \
     time_t t; \
     struct timeval tv; \
     char *tstr; \
     t = time(NULL); \
     tstr = strdup(ctime(&t)); \
     tstr[strlen(tstr)-1] = 0; \
     gettimeofday(&tv,NULL); \
     fprintf(stderr," %s [%d]",tstr, (int) tv.tv_usec); \
     fprintf(stderr," ALSA: "); \
     fprintf(stderr,arg); \
     fprintf(stderr,"\n"); \
     fflush(stderr); \
  }

#define ERR(arg...) \
 { \
     time_t t; \
     struct timeval tv; \
     char *tstr; \
     t = time(NULL); \
     tstr = strdup(ctime(&t)); \
     tstr[strlen(tstr)-1] = 0; \
     gettimeofday(&tv,NULL); \
     fprintf(stderr," %s [%d]",tstr, (int) tv.tv_usec); \
     fprintf(stderr," ALSA ERROR: "); \
     fprintf(stderr,arg); \
     fprintf(stderr,"\n"); \
     fflush(stderr); \
  }

/* I/O error handler */
int
xrun(AudioID *id)
{
    snd_pcm_status_t *status;
    int res;
    
    if (id == NULL) return -1;

    MSG("WARNING: Entering XRUN handler");

    
    snd_pcm_status_alloca(&status);
    if ((res = snd_pcm_status(id->pcm, status))<0) {
	ERR("status error: %s", snd_strerror(res));
	
	return -1;
    }
    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
	struct timeval now, diff, tstamp;
	gettimeofday(&now, 0);
	snd_pcm_status_get_trigger_tstamp(status, &tstamp);
	timersub(&now, &tstamp, &diff);
	MSG("underrun!!! (at least %.3f ms long)",
	    diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
	if ((res = snd_pcm_prepare(id->pcm))<0) {
	    ERR("xrun: prepare error: %s", snd_strerror(res));
	    
	    return -1;
	}
	
	return 0;		/* ok, data should be accepted again */
    }
    ERR("read/write error, state = %s",
	snd_pcm_state_name(snd_pcm_status_get_state(status)));
        
    return -1;
}

/* I/O suspend handler */
int
suspend(AudioID *id)
{
    int res;

    MSG("WARNING: Entering SUSPEND handler.");
    
    if (id == NULL) return -1;
    
    while ((res = snd_pcm_resume(id->pcm)) == -EAGAIN)
	sleep(1);	/* wait until suspend flag is released */
    
    if (res < 0) {
	if ((res = snd_pcm_prepare(id->pcm)) < 0) {
	    ERR("suspend: prepare error: %s", snd_strerror(res));
	    
	    return -1;
	}
    }
    
    return 0;
}


/* Open the device so that it's ready for playing on the default
   device. Internal function used by the public alsa_open. */
int
_alsa_open(AudioID *id)
{
    int err;
    struct pollfd alsa_stop_pipe_pfd;

    MSG("Opening ALSA device");
    fflush(stderr);

    /* Open the device */
    if ((err = snd_pcm_open (&id->pcm, id->alsa_device_name,
			     SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) { 
	ERR("Cannot open audio device %s (%s)", id->alsa_device_name, snd_strerror (err));
	return -1;
    }

    /* Allocate space for hw_params (description of the sound parameters) */
    MSG("Allocating new hw_params structure");
    if ((err = snd_pcm_hw_params_malloc (&id->hw_params)) < 0) {
	ERR("Cannot allocate hardware parameter structure (%s)", 
	    snd_strerror(err));
	return -1;
    }
    
    /* Initialize hw_params on our pcm */
    if ((err = snd_pcm_hw_params_any (id->pcm, id->hw_params)) < 0) {
	ERR("Cannot initialize hardware parameter structure (%s)", 
	    snd_strerror (err));
	return -1;
    }
    
    /* Create the pipe for communication about stop requests */
    if (pipe (id->alsa_stop_pipe))
	{
	    ERR("Stop pipe creation failed (%s)", strerror(errno));
	    return -1;
	}   

    /* Find how many descriptors we will get for poll() */
    id->alsa_fd_count = snd_pcm_poll_descriptors_count(id->pcm);
    if (id->alsa_fd_count <= 0){
	ERR("Invalid poll descriptors count returned from ALSA.");
	return -1;
    }

    /* Create and fill in struct pollfd *alsa_poll_fds with ALSA descriptors */
    id->alsa_poll_fds = malloc ((id->alsa_fd_count + 1) * sizeof(struct pollfd));
    assert(id->alsa_poll_fds);    
    if ((err = snd_pcm_poll_descriptors(id->pcm, id->alsa_poll_fds, id->alsa_fd_count)) < 0) {
	ERR("Unable to obtain poll descriptors for playback: %s\n", snd_strerror(err));
	return -1;
    }
    
    /* Create a new pollfd structure for requests by alsa_stop()*/
    alsa_stop_pipe_pfd.fd = id->alsa_stop_pipe[0];
    alsa_stop_pipe_pfd.events = POLLIN;
    alsa_stop_pipe_pfd.revents = 0;
        
    /* Join this our own pollfd to the ALSAs ones */
    id->alsa_poll_fds[id->alsa_fd_count] = alsa_stop_pipe_pfd;
    id->alsa_fd_count++;

    id->alsa_opened = 1;

    MSG("Opening ALSA device ... success");

    return 0;
}

/* 
   Close the device. Internal function used by public alsa_close. 
*/

int
_alsa_close(AudioID *id)
{
    int err;

    MSG("Closing ALSA device");

    id->alsa_opened = 0;
    
    if ((err = snd_pcm_close (id->pcm)) < 0) {
	MSG("Cannot close ALSA device (%s)", snd_strerror (err));
	return -1;
    }
    
    close(id->alsa_stop_pipe[0]);
    close(id->alsa_stop_pipe[1]);

    snd_pcm_hw_params_free (id->hw_params);
    free(id->alsa_poll_fds);

    MSG("Opening ALSA device ... success");
    
    return 0;
}


/* Open ALSA for playback.
  
  These parameters are passed in pars:
  (char*) pars[0] ... null-terminated string containing the name
                      of the device to be used for sound output
                      on ALSA
  (void*) pars[1] ... =NULL
*/
int
alsa_open(AudioID *id, void **pars)
{
    int ret;

    id->alsa_opened = 0;

    if (id == NULL){
	ERR("Can't open ALSA sound output, invalid AudioID structure.");
	return 0;
    }

    if (pars[0] == NULL){
	ERR("Can't open ALSA sound output, missing parameters in argument.");
	return -1;
    }
    
    MSG("Opening ALSA sound output");

    id->alsa_device_name = strdup(pars[0]);
    
    ret = _alsa_open(id);
    if (ret){
	ERR("Cannot initialize Alsa device '%s': Can't open.", pars[0]);
	return -1;
    }
    _alsa_close(id);
    if (ret){
	ERR("Cannot initialize Alsa device '%s': Can't close.", pars[0]);
	return -1;
    }
    
    MSG("Device '%s' initialized succesfully.", pars[0]);
    
    return 0; 
}

/* Close ALSA */
int
alsa_close(AudioID *id)
{
    int err;

    /* Close device */
    if ((err = _alsa_close(id)) < 0) { 
	ERR("Cannot close audio device (%s)");
	return -1;
    }
    MSG("ALSA closed.");

    id = NULL;

    return 0;
}

/* Wait until ALSA is readdy for more samples or alsa_stop() was called.

Returns 0 if ALSA is ready for more input, +1 if a request to stop
the sound output was received and a negative value on error.  */

int wait_for_poll(AudioID *id, struct pollfd *alsa_poll_fds, 
			 unsigned int count)
{
        unsigned short revents;
	snd_pcm_state_t state;
	int ret;

	MSG("Waiting for poll");

	/* Wait for certain events */
        while (1) {
                ret = poll(id->alsa_poll_fds, count, -1);
		// MSG("wait_for_poll: activity on %d descriptors", ret);

		/* Check for stop request from alsa_stop on the last file
		 descriptors*/
		if (revents = id->alsa_poll_fds[count-1].revents){
		    if (revents & POLLIN){
			MSG("wait_for_poll: stop requested");
			return 1;
		    }
		}

		/* Check the first count-1 descriptors for ALSA events */
		snd_pcm_poll_descriptors_revents(id->pcm, id->alsa_poll_fds, count-1, &revents);
	       
		/* Check for errors */
                if (revents & POLLERR) 
		    return -EIO;

		/* Ensure we are in the right state */
		state = snd_pcm_state(id->pcm);

		MSG("State after poll returned is %s", snd_pcm_state_name(state));

		if (snd_pcm_state(id->pcm) == SND_PCM_STATE_XRUN){
		    MSG("WARNING: Buffer underrun detected!");
		    if (xrun(id) != 0) return -1;
		}

		if (snd_pcm_state(id->pcm) == SND_PCM_STATE_SUSPENDED){
		    MSG("WARNING: Suspend detected!");
		    if (suspend(id) != 0) return -1;
		}

		/* Is ALSA ready for more input? */
                if (revents & POLLOUT){
		    MSG("Poll: Ready for more input");
		    return 0;	       
		}
        }
}


#define ERROR_EXIT()\
    free(track_volume.samples); \
    ERR("alsa_play() abnormal exit"); \
    _alsa_close(id); \
    return -1;

/* Play the track _track_ (see spd_audio.h) using the id->pcm device and
 id-hw_params parameters. This is a blocking function, however, it's possible
 to interrupt playing from a different thread with alsa_stop(). alsa_play
 returns after and immediatelly after the whole sound was played on the
 speakers.

 The idea is that we get the ALSA file descriptors and we will poll() to see
 when alsa is ready for more input while sleeping in the meantime. We will
 additionally poll() for one more descriptor used by alsa_stop() to notify the
 thread with alsa_play() that the stop of the playback is requested. The
 variable can_be_stopped is used for very simple synchronization between the
 two threads. */
int
alsa_play(AudioID *id, AudioTrack track)
{
    snd_pcm_format_t format;
    int channels;
    int bytes_per_sample;
    int num_bytes;
    int frames;

    int bytes; 

    signed short* output_samples;

    AudioTrack track_volume;
    float real_volume;
    int i;

    int err;
    int ret;

    snd_pcm_uframes_t framecount;

    char buf[100];

    snd_pcm_state_t state;

    MSG("Start of playback on ALSA");

    if (id == NULL){
	ERR("Invalide device passed to alsa_play()");
	return -1;
    }

    /* This is needed, otherwise we can't set different
     parameters than in tha last call. */
    _alsa_open(id);
    
    /* Is it not an empty track? */
    /* Passing an empty track is not an error */
    if (track.samples == NULL) return 0;

    /* Report current state state */
    state = snd_pcm_state(id->pcm);
    MSG("PCM state before setting audio parameters: %s",
	snd_pcm_state_name(state));

    /* Choose the correct format */
    if (track.bits == 16){	
	format = SND_PCM_FORMAT_S16_LE;
	bytes_per_sample = 2;
    }else if (track.bits == 8){
	bytes_per_sample = 1;
	format = SND_PCM_FORMAT_S8;
    }else{
	ERR("Unsupported sound data format, track.bits = %d", track.bits);		
	return -1;
    }

    /* Set access mode, bitrate, sample rate and channels */
    MSG("Setting access type to INTERLEAVED");
    if ((err = snd_pcm_hw_params_set_access (id->pcm, 
					     id->hw_params, 
					     SND_PCM_ACCESS_RW_INTERLEAVED)
	 )< 0) {
	ERR("Cannot set access type (%s)",
		 snd_strerror (err));	
	return -1;
    }
    
    MSG("Setting sample format to %s", snd_pcm_format_name(format));
    if ((err = snd_pcm_hw_params_set_format (id->pcm, id->hw_params, format)) < 0) {
	ERR("Cannot set sample format (%s)",
		 snd_strerror (err));
	return -1;
    }

    MSG("Setting sample rate to %i", track.sample_rate);	
    if ((err = snd_pcm_hw_params_set_rate_near (id->pcm, id->hw_params,
						&(track.sample_rate), 0)) < 0) {
	ERR("Cannot set sample rate (%s)",
		 snd_strerror (err));
	
	return -1;
    }

    MSG("Setting channel count to %i", track.num_channels);
    if ((err = snd_pcm_hw_params_set_channels (id->pcm, id->hw_params,
					       track.num_channels)) < 0) {
	MSG("cannot set channel count (%s)",
		 snd_strerror (err));	
	return -1;
    }

    MSG("Setting hardware parameters on the ALSA device");	
    if ((err = snd_pcm_hw_params (id->pcm, id->hw_params)) < 0) {
	MSG("cannot set parameters (%s) state=%s",
	    snd_strerror (err), snd_pcm_state_name(snd_pcm_state(id->pcm)));	
	return -1;
    }


    MSG("Preparing device for playback");
    if ((err = snd_pcm_prepare (id->pcm)) < 0) {
	ERR("Cannot prepare audio interface for playback (%s)",
		 snd_strerror (err));
	
	return -1;
    }

    /* Get period size. */
    snd_pcm_uframes_t period_size;
    snd_pcm_hw_params_get_period_size(id->hw_params, &period_size, 0);

    /* Calculate size of silence at end of buffer. */
    size_t samples_per_period = period_size * track.num_channels;
    MSG("samples per period = %i", samples_per_period);
    MSG("num_samples = %i", track.num_samples);
    size_t silent_samples = samples_per_period - (track.num_samples % samples_per_period);
    MSG("silent samples = %i", silent_samples);

    /* Calculate space needed to round up to nearest period size. */
    size_t volume_size = bytes_per_sample*(track.num_samples + silent_samples);
    MSG("volume size = %i", volume_size);

    /* Create a copy of track with adjusted volume. */
    MSG("Making copy of track and adjusting volume");
    track_volume = track;
    track_volume.samples = (short*) malloc(volume_size);
    real_volume = ((float) id->volume + 100)/(float)200;
    for (i=0; i<=track.num_samples-1; i++)
        track_volume.samples[i] = track.samples[i] * real_volume;

    if (silent_samples > 0) {
        /* Fill remaining space with silence */
        MSG("Filling with silence up to the period size, silent_samples=%d", silent_samples);
        /* TODO: This hangs.  Why?
        snd_pcm_format_set_silence(format,
            track_volume.samples + (track.num_samples * bytes_per_sample), silent_samples);
        */
        u_int16_t silent16;
        u_int8_t silent8;
        switch (bytes_per_sample) {
	case 2:
	    silent16 = snd_pcm_format_silence_16(format);
	    for (i = 0; i < silent_samples; i++)
		track_volume.samples[track.num_samples + i] = silent16;
	    break;
	case 1:	    
	    silent8 = snd_pcm_format_silence(format);
	    for (i = 0; i < silent_samples; i++)
		track_volume.samples[track.num_samples + i] = silent8;
	    break;
        }
    }

    /* Loop until all samples are played on the device. */
    output_samples = track_volume.samples;
    num_bytes = (track.num_samples + silent_samples)*bytes_per_sample;
    MSG("Still %d bytes left to be played", num_bytes);
    while(num_bytes > 0) {
	
	/* Write as much samples as possible */
        framecount = num_bytes/bytes_per_sample/track.num_channels;
        if (framecount < period_size) framecount = period_size;

	/* Report current state state */
	state = snd_pcm_state(id->pcm);
	MSG("PCM state before writei: %s",
	    snd_pcm_state_name(state));

	/* MSG("snd_pcm_writei() called") */
	ret = snd_pcm_writei (id->pcm, output_samples, framecount);
        MSG("Sent %d frames of %d remaining", ret, num_bytes);

        if (ret == -EAGAIN) {
	    MSG("Warning: Forced wait!");
	    snd_pcm_wait(id->pcm, 100);
        } else if (ret == -EPIPE) {
            if (xrun(id) != 0) ERROR_EXIT();
	} else if (ret == -ESTRPIPE) {
	    if (suspend(id) != 0) ERROR_EXIT();
	} else if (ret == -EBUSY){
            MSG("WARNING: sleeping while PCM BUSY");
            usleep(100);
            continue;
        } else if (ret < 0) {	    
	    ERR("Write to audio interface failed (%s)",
		snd_strerror (ret));
	    ERROR_EXIT();
	}

	if (ret > 0) {
            /* Update counter of bytes left and move the data pointer */
            num_bytes -= ret*bytes_per_sample*track.num_channels;
            output_samples += ret*bytes_per_sample*track.num_channels/2;
        }
	

	/* Report current state state */
	state = snd_pcm_state(id->pcm);
	MSG("PCM state before polling: %s",
	    snd_pcm_state_name(state));

	err = wait_for_poll(id, id->alsa_poll_fds, id->alsa_fd_count);
	if (err < 0) {
	    ERR("Wait for poll() failed\n");
	    ERROR_EXIT();
	}	
	else if (err == 1){
	    MSG("Playback stopped");

	    /* Drop the playback on the sound device (probably
	       still in progress up till now) */
	    snd_pcm_drop(id->pcm);
	    
	    break;	   
	}
	
	if (num_bytes <= 0) break;
	MSG("ALSA ready for more samples");

	/* Stop requests can be issued again */
    }

    /* Terminating (successfully or after a stop) */
    if (track_volume.samples != NULL)
	free(track_volume.samples);
    
    _alsa_close(id);
    

    MSG("End of playback on ALSA");

    return 0;   
}

#undef ERROR_EXIT

/*
 Stop the playback on the device and interrupt alsa_play()
*/
int
alsa_stop(AudioID *id)
{
    int ret;
    char buf;

    if (id->alsa_opened){
	/* This constant is arbitrary */
	buf = 42;
	
	/* TODO: remove, debug message */
	MSG("Request for stop, device state is %s",
	    snd_pcm_state_name(snd_pcm_state(id->pcm)));
	
	if (id == NULL) return 0;
	
	write(id->alsa_stop_pipe[1], &buf, 1);
    }	

    return 0;
}

/* 
  Set volume

  Comments: It's not possible to set individual track volume with Alsa, so we
   handle volume in alsa_play() by scalar multiplication of each sample.
*/
int
alsa_set_volume(AudioID*id, int volume)
{
    return 0;
}

/* Provide the Alsa backend. */
AudioFunctions alsa_functions = {alsa_open, alsa_play, alsa_stop, alsa_close, alsa_set_volume};

#undef MSG
#undef ERR
