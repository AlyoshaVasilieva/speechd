
/*
 * speechd.h - Speech Dispatcher header
 *
 * Copyright (C) 2001, 2002, 2003 Brailcom, o.p.s.
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
 * $Id: speechd.h,v 1.41 2003-08-11 15:02:01 hanke Exp $
 */

#ifndef SPEECHDH
 #define SPEECHDH

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <glib.h>
#include <gmodule.h>
#include <dotconf.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>
#include <signal.h>

#ifndef PATH_MAX
#  define PATH_MAX 255
#endif

#include "def.h"
#include "module.h"
#include "history.h"
#include "fdset.h"
#include "set.h"
#include "msg.h"
#include "parse.h"
#include "sndicon.h"
#include "compare.h"
#include "alloc.h"

#define BUF_SIZE 4096

typedef enum{
    SPD_MODE_DAEMON,
    SPD_MODE_SINGLE
}TSpeechDMode;

/*  TSpeechDQueue is a queue for messages. */
typedef struct{
    GList *p1;			/* important */
    GList *p2;			/* text */
    GList *p3;			/* message */
    GList *p4;                  /* notification */
    GList *p5;                  /* progress */
}TSpeechDQueue;

/*  TSpeechDMessage is an element of TSpeechDQueue,
    that is, some text with ''formating'' tags inside. */
typedef struct{
    guint id;			/* unique id */
    time_t time;                /* when was this message received */
    char *buf;			/* the actual text */
    int bytes;			/* number of bytes in buf */
    TFDSetElement settings;	/* settings of the client when queueing this message */
}TSpeechDMessage;

#include "alloc.h"

/* EStopCommands describes the stop family of commands */
typedef enum
{
    STOP = 1,
    PAUSE = 2,
    RESUME = 3
}EStopCommands;

typedef struct
{
    GList *sound_icons;
    GList *spelling;
    GList *characters;
    GList *keys;
    GList *punctuation;
    GList *cap_let_recogn;
}TSpeechDTables;

TSpeechDMode spd_mode;

int spd_port;
int spd_port_set;

char *SOUND_DATA_DIR;

/* Message logging */
int spd_log_level;
int spd_log_level_set;

void MSG(int level, char *format, ...);
#define FATAL(msg) { MSG(0,"Fatal error [%s:%d]:"msg, __FILE__, __LINE__); exit(EXIT_FAILURE); }
#define DIE(msg) { MSG(0,"Error [%s:%d]:"msg, __FILE__, __LINE__); exit(EXIT_FAILURE); }
FILE *logfile;

/* Variables for socket communication */
int fdmax;
fd_set readfds;

/* The largest assigned uid + 1 */
int max_uid;

/* Unique identifier for group id (common for messages of the same group) */
int max_gid;

int MaxHistoryMessages;

/* speak() thread */
pthread_t speak_thread;
pthread_mutex_t element_free_mutex;

/* How many messages in total are waiting in the queues */
sem_t *sem_messages_waiting;

/* Loading options from DotConf */
configoption_t *spd_options;
int spd_num_options;


/* Table of all configured (and succesfully loaded) output modules */
GHashTable *output_modules;	
/* Table of settings for each active client (=each active socket)*/
GHashTable *fd_settings;	
/* Table of sound icons for different languages */
GHashTable *snd_icon_langs;
/* Table of default output modules for different languages */
GHashTable *language_default_modules;
/* Table of relations between client file descriptors and their uids*/
GHashTable *fd_uid;
/* A list of all defined tables grouped by their categories */
TSpeechDTables tables;

/* Speech Dispatcher main priority queue for messages */
TSpeechDQueue *MessageQueue;
/* List of messages from paused clients waiting for resume */
GList *MessagePausedList;
/* List of settings related to history */
GList *history_settings;
/* List of messages in history */
GList *message_history;

TSpeechDMessage *last_p5_message;

/* Global default settings */
TFDSetElement GlobalFDSet;

OutputModule *sound_module;

/* Arrays needed for receiving data over socket */
int *awaiting_data;
int *inside_block;
size_t *o_bytes;
GString **o_buf;
int fds_allocated;

int pause_requested;
int pause_requested_fd;
int pause_requested_uid;

/* Loads output module */
OutputModule* load_output_module(gchar* modname, gchar* modlib);

/* speak() runs in a separate thread, pulls messages from
 * the priority queue and sends them to synthesizers */
void* speak(void*);						

/* serve() reads data from clients and sends it to parse() */
int serve(int fd);

/* Functions for parsing the input from clients */
/* also parse() */
char* get_param(const char *buf, const int n, const int bytes, const int lower_case);

/* Stops all messages from client on fd */
void stop_from_client(int fd);

/* isanum() tests if the given string is a number,
 * returns 1 if yes, 0 otherwise. */
int isanum(const char *str);

TFDSetElement* get_client_settings_by_uid(int uid);
TFDSetElement* get_client_settings_by_fd(int fd);
int get_client_uid_by_fd(int fd);


#endif
