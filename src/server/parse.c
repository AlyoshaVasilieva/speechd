
/*
 * parse.c - Parses commands Speech Deamon got from client
 *
 * Copyright (C) 2001,2002,2003 Ceska organizace pro podporu free software
 * (Czech Free Software Organization), Prague 2, Vysehradska 3/255, 128 00,
 * <freesoft@freesoft.cz>
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
 * $Id: parse.c,v 1.11 2003-03-24 22:22:47 hanke Exp $
 */

#include "speechd.h"

/* isanum() tests if the given string is a number,
 *  *  * returns 1 if yes, 0 otherwise. */
int
isanum(char *str){
   int i;
   if (!isdigit(str[0]) && !( (str[0]=='+') || (str[0]=='-'))) return 0;
   for(i=1;i<=strlen(str)-1;i++){
       if (!isdigit(str[i]))   return 0;
    }
    return 1;
}


/* Gets command parameter _n_ from the text buffer _buf_
 * which has _bytes_ bytes. Note that the parameter with
 * index 0 is the command itself. */
char* 
get_param(char *buf, int n, int bytes)
{
	char* param;
	int i, y, z;

	param = (char*) malloc(bytes);
	assert(param != NULL);
	
	strcpy(param,"");
	i = 0;

	/* Read all the parameters one by one,
	 * but stop after the one with index n,
	 * while maintaining it's value in _param_ */
	for(y=0; y<=n; y++){
		z=0;
		for(; i<bytes; i++){
			if (buf[i] == ' ') break;
			param[z] = buf[i];
			z++;
      }
      i++;
   }
   
	/* Write the trailing zero */
	if (i >= bytes){
		param[z>1?z-2:0] = 0;
	}else{
		param[z] = 0;
	}

   return param;
}

/* CONTINUE: Cleaning the rest of the file from here. */

/* Parses @history commands and calls the appropriate history_ functions. */
char*
parse_history(char *buf, int bytes, int fd)
{
	char *param;
	char *helper1, *helper2, *helper3;				
		
         param = get_param(buf,1,bytes);
         MSG(3, "  param 1 caught: %s\n", param);
         if (!strcmp(param,"GET")){
            param = get_param(buf,2,bytes);
            if (!strcmp(param,"LAST")){
               return (char*) history_get_last(fd);
            }
            if (!strcmp(param,"CLIENT_LIST")){
               return (char*) history_get_client_list();
            }  
            if (!strcmp(param,"MESSAGE_LIST")){
				helper1 = get_param(buf,3,bytes);
				if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
				helper2 = get_param(buf,4,bytes);
				if (!isanum(helper2)) return ERR_NOT_A_NUMBER;
				helper3 = get_param(buf,5,bytes);
				if (!isanum(helper2)) return ERR_NOT_A_NUMBER;
               return (char*) history_get_message_list( atoi(helper1), atoi(helper2), atoi (helper3));
            }  
         }
         if (!strcmp(param,"SORT")){
			return ERR_NOT_IMPLEMENTED;
         	// TODO: everything :)
         }
         if (!strcmp(param,"CURSOR")){
            param = get_param(buf,2,bytes);
            MSG(3, "    param 2 caught: %s\n", param);
            if (!strcmp(param,"SET")){
               param = get_param(buf,4,bytes);
               MSG(3, "    param 4 caught: %s\n", param);
               if (!strcmp(param,"LAST")){
				  helper1 = get_param(buf,3,bytes);
				  if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
                  return (char*) history_cursor_set_last(fd,atoi(helper1));
               }
               if (!strcmp(param,"FIRST")){
				  helper1 = get_param(buf,3,bytes);
				  if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
                  return (char*) history_cursor_set_first(fd, atoi(helper1));
               }
               if (!strcmp(param,"POS")){
				  helper1 = get_param(buf,3,bytes);
				  if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
				  helper2 = get_param(buf,5,bytes);
				  if (!isanum(helper2)) return ERR_NOT_A_NUMBER;
                  return (char*) history_cursor_set_pos( fd, atoi(helper1), atoi(helper2) );
               }
            }
            if (!strcmp(param,"NEXT")){
               return (char*) history_cursor_next(fd);
            }
            if (!strcmp(param,"PREV")){
               return (char*) history_cursor_prev(fd);
            }
            if (!strcmp(param,"GET")){
               return (char*) history_cursor_get(fd);
            }
         }
         if (!strcmp(param,"SAY")){
            param = get_param(buf,2,bytes);
            if (!strcmp(param,"ID")){
				helper1 = get_param(buf,3,bytes);
				if (!isanum(helper1)) return ERR_NOT_A_NUMBER;
              return (char*) history_say_id(fd, atoi(helper1));
            }
            if (!strcmp(param,"TEXT")){
			return ERR_NOT_IMPLEMENTED;
            }
         }
 
         return ERR_INVALID_COMMAND;
}

char*
parse_set(char *buf, int bytes, int fd)
{
	char *param;
	char *language;
	char *client_name;
	char *priority;
	char *rate;
	char *pitch;
	char *punct;
	char *spelling;
	char *recog;
	int helper;
	int ret;

	param = get_param(buf,1,bytes);
	if (!strcmp(param, "PRIORITY")){
		priority = get_param(buf,2,bytes);
		if (!isanum(priority)) return ERR_NOT_A_NUMBER;
		helper = atoi(priority);
		MSG(3, "Setting priority to %d \n", helper);
		ret = set_priority(fd, helper);
		if(!ret) return ERR_COULDNT_SET_PRIORITY;	
		return OK_PRIORITY_SET;
	}

      if (!strcmp(param,"LANGUAGE")){
        language = get_param(buf,2,bytes);
        MSG(3, "Setting language to %s \n", language);
		ret = set_language(fd, language);
		if (!ret) return ERR_COULDNT_SET_LANGUAGE;
        return OK_LANGUAGE_SET;
      }

	if (!strcmp(param,"CLIENT_NAME")){
		MSG(3, "Setting client name. \n");

		client_name = get_param(buf,2,bytes);
		ret = set_client_name(fd, client_name);
		if (!ret) return ERR_COULDNT_SET_LANGUAGE;
		return OK_CLIENT_NAME_SET;
	}		 

	if (!strcmp(param,"RATE")){
		rate = get_param(buf,2,bytes);
		if (!isanum(rate)) return ERR_NOT_A_NUMBER;
		helper = atoi(rate);
		if(helper < -100) return ERR_COULDNT_SET_RATE;
		if(helper > +100) return ERR_COULDNT_SET_RATE;
		MSG(3, "Setting rate to %d \n", helper);
		ret = set_rate(fd, helper);
		if (!ret) return ERR_COULDNT_SET_RATE;
		return OK_RATE_SET;
	}

	if (!strcmp(param,"PITCH")){
		pitch = get_param(buf,2,bytes);
		if (!isanum(pitch)) return ERR_NOT_A_NUMBER;
		helper = atoi(pitch);
		if(helper < -100) return ERR_COULDNT_SET_PITCH;
		if(helper > +100) return ERR_COULDNT_SET_PITCH;
		pitch = get_param(buf,2,bytes);
		if (!isanum(pitch)) return ERR_NOT_A_NUMBER;
		helper = atoi(pitch);
		MSG(3, "Setting pitch to %s \n", language);
		ret = set_pitch(fd, helper);
		if (!ret) return ERR_COULDNT_SET_LANGUAGE;
		return OK_RATE_SET;
	}

	if (!strcmp(param,"PUNCTUATION")){
		punct = get_param(buf,2,bytes);
		if (!isanum(punct)) return ERR_NOT_A_NUMBER;
		helper = atoi(punct);
		if((helper != 0)&&(helper != 1)&&(helper != 2)) return ERR_COULDNT_SET_PUNCT_MODE;
		MSG(3, "Setting punctuation mode to %d \n", helper);
		ret = set_punct_mode(fd, helper);
		if (!ret) return ERR_COULDNT_SET_PUNCT_MODE;
		return OK_PUNCT_MODE_SET;
	}

	if (!strcmp(param,"CAP_LET_RECOGN")){
		recog = get_param(buf,2,bytes);
		if (!isanum(recog)) return ERR_NOT_A_NUMBER;
		helper = atoi(recog);
		if((helper != 0)&&(helper != 1)) return ERR_COULDNT_SET_CAP_LET_RECOG;
		MSG(3, "Setting capital letter recognition to %d \n", helper);
		ret = set_cap_let_recog(fd, helper);
		if (!ret) return ERR_COULDNT_SET_CAP_LET_RECOG;
		return OK_CAP_LET_RECOGN_SET;
	}

	if (!strcmp(param,"SPELLING")){
		spelling = get_param(buf,2,bytes);
		if (!isanum(spelling)) return ERR_NOT_A_NUMBER;
		helper = atoi(spelling);
		if((helper != 0)&&(helper != 1)) return ERR_COULDNT_SET_SPELLING;
		MSG(3, "Setting spelling to %d \n", helper);
		ret = set_spelling(fd, helper);
		if (!ret) return ERR_COULDNT_SET_SPELLING;
		return OK_SPELLING_SET;
	}
	   
	return ERR_INVALID_COMMAND;
}

char*
parse_stop(char *buf, int bytes, int fd)
{
	int ret;
	int target = 0;
	char *param;

    param = get_param(buf,1,bytes);
	if (isanum(param)) target = atoi(param);
			
	MSG(4, "Stop recieved.");
	ret = stop_c(STOP, fd, target);
	if(ret) return ERR_NO_SUCH_CLIENT;
	else return OK_STOPED;
}

char*
parse_pause(char *buf, int bytes, int fd)
{
	int target = 0;
	char *param;
	int ret;
	param = get_param(buf,1,bytes);
	if (isanum(param)) target = atoi(param);
	MSG(4, "Pause recieved.\n");
	ret = stop_c(PAUSE, fd, target);
	if(ret)	return ERR_NO_SUCH_CLIENT;
	else return OK_PAUSED;
}

char*
parse_resume(char *buf, int bytes, int fd)
{
	int target = 0;
	char *param;
	int ret;
	param = get_param(buf,1,bytes);
	if (isanum(param)) target = atoi(param);
	MSG(4, "Resume received.");
	ret = stop_c(RESUME, fd, target);
	if(ret)	return ERR_NO_SUCH_CLIENT;
	else return OK_RESUMED;
}

char*
parse_snd_icon(char *buf, int bytes, int fd)
{
    char *param;
    int ret;
	GHashTable *icons;
	char *word;
	TSpeechDMessage *new;
    TFDSetElement *settings;		
    GList *gl;		
	
    param = get_param(buf,1,bytes);
	if (param == NULL) return ERR_MISSING_PARAMETER;
	
	settings = (TFDSetElement*) g_hash_table_lookup(fd_settings, &fd);
	if (settings == NULL)
	        FATAL("Couldn't find settings for active client, internal error.");
	
	icons = g_hash_table_lookup(snd_icon_langs, settings->language);
	if (icons == NULL){
		icons = g_hash_table_lookup(snd_icon_langs, GlobalFDSet.language);
		if (icons == NULL) return ERR_NO_SND_ICONS;
	}
	
	word = g_hash_table_lookup(icons, param); 

	if(word==NULL) return ERR_UNKNOWN_ICON;

	new = malloc(sizeof(TSpeechDMessage));
	new->bytes = strlen(word)+1;
	new->buf = malloc(new->bytes);
	strcpy(new->buf, word);
	if(queue_message(new,fd))  FATAL("Couldn't queue message\n");
																
	return OK_SND_ICON_QUEUED;
}				 
