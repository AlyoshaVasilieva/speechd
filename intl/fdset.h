
/*
 * fdset.h - Settings for Speech Deamon
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
 * $Id: fdset.h,v 1.12 2003-04-17 10:22:54 hanke Exp $
 */

typedef enum 
{                  /* Type of voice */
    MALE1 = 0,
    MALE2 = 1,
    MALE3 = 2,
    FEMALE1 = 3,
    FEMALE2 = 4,
    FEMALE3 = 5,
    CHILD_MALE = 6,
    CHILD_FEMALE = 7
}EVoiceType;

typedef enum
{
    BY_TIME = 0,
    BY_ALPHABET = 1
}ESort;

typedef enum
{
    TEXT = 0,
    SOUND_ICON = 1,
    CHAR = 2,
    KEY = 3
}EMessageType;
    

typedef struct{
    unsigned int uid;		/* Unique ID of the client */
    int fd;                     /* File descriptor the client is on. */
    int active;                 /* Is this client still active on socket or gone?*/
    int paused;                 /* Internal flag, 1 for paused client or 0 for normal. */
    EMessageType type;          /* Type of the message (1=text, 2=icon, 3=char, 4=key) */
    int priority;               /* Priority between 1 and 3 (1 - highest, 3 - lowest) */
    int punctuation_mode;	/* Punctuation mode: 0, 1 or 2
                                   0	-	no punctuation
                                   1 	-	all punctuation
                                   2	-	only user-selected punctuation */
    char *punctuation_some;
    char *punctuation_table;    /*  Selected punctuation table */
    signed int speed; 		/* Speed of voice from <-100;+100>, 0 is the default medium */
    signed int pitch;		/* Pitch of voice from <-100;+100>, 0 is the default medium */
    char *client_name;		/* Name of the client. */
    char *language;             /* Selected language name. (e.g. "english", "czech", "french", ...) */
    char *output_module;        /* Output module name. (e.g. "festival", "flite", "apollo", ...) */
    EVoiceType voice_type;      /* see EVoiceType definition above */
    int spelling;               /* Spelling mode: 0 or 1 (0 - off, 1 - on) */
    char* spelling_table;	/* Selected spelling table */
    int cap_let_recogn;         /* Capital letters recognition: (0 - off, 1 - on) */
    unsigned int hist_cur_uid;
    int hist_cur_pos;
    ESort hist_sorted;
}TFDSetElement;

