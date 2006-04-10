/*
 * module_main.c - One way of doing main() in output modules.
 * 
 * Copyright (C) 2001, 2002, 2003, 2006 Brailcom, o.p.s.
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
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
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id: module_main.c,v 1.9 2006-04-10 17:20:40 hanke Exp $
 */

/* So that gcc doesn't comply */
int getline(char**, int*, FILE*);

#define PROCESS_CMD(command, function) \
if (!strcmp(cmd_buf, #command"\n")){ \
 char *msg; \
 pthread_mutex_lock(&module_stdout_mutex); \
 if (printf("%s\n", msg = (char*) function()) < 0){ \
     DBG("Broken pipe, exiting...\n"); \
     module_close(2); \
 } \
 pthread_mutex_unlock(&module_stdout_mutex);\
 xfree(msg); \
 fflush(stdout); \
}

#define PROCESS_CMD_NRP(command, function) \
if (!strcmp(cmd_buf, #command"\n")){ \
 pthread_mutex_lock(&module_stdout_mutex); \
 function(); \
 pthread_mutex_unlock(&module_stdout_mutex);\
 fflush(stdout); \
}

int 
main(int argc, char *argv[])
{
    char *cmd_buf;
    int ret;
    int ret_init;
    int n;
    char *configfilename;
    char *status_info;        

    module_num_dc_options = 0;
    
    if (argc >= 2){
        configfilename = strdup(argv[1]);
    }else{
        configfilename = NULL;
    }

    ret = module_load();
    if (ret == -1) module_close(1);

    if (configfilename != NULL){
        /* Add the LAST option */
        module_dc_options = add_config_option(module_dc_options,
                                              &module_num_dc_options, "", 0, NULL, NULL, 0);

        configfile = dotconf_create(configfilename, module_dc_options, 0, CASE_INSENSITIVE);
        if (configfile){
            if (dotconf_command_loop(configfile) == 0){
                DBG("Error reading config file\n");
                module_close(1);
            }
	    dotconf_cleanup(configfile);
	    DBG("Configuration (pre) has been read from \"%s\"\n", configfilename);    
	    
	    xfree(configfilename);
        }else{
            DBG("Can't read specified config file!\n");        
        }
    }else{
        DBG("No config file specified, using defaults...\n");        
    }
    
    ret_init = module_init(&status_info);

    cmd_buf = NULL;  n=0;
    ret = getline(&cmd_buf, &n, stdin);
    if (ret == -1){
	DBG("Broken pipe when reading INIT, exiting... \n");
	module_close(2); 
    }

    if (!strcmp(cmd_buf, "INIT\n")){
	if (ret_init == 0){
	    printf("299-%s\n", status_info);
	    ret = printf("%s\n", "299 OK LOADED SUCCESSFULLY");
	}else{
	    printf("399-%s\n", status_info);
	    ret = printf("%s\n", "399 ERR CANT INIT MODULE");
	    return -1;
	}
      	xfree(status_info);

	if (ret < 0){ 
	    DBG("Broken pipe, exiting...\n");
            module_close(2); 
	}
	fflush(stdout);
    }else{
	DBG("ERROR: Wrong communication from module client: didn't call INIT\n");
	module_close(3);
    }
    xfree(cmd_buf);

    while(1){
        cmd_buf = NULL;  n=0;
        ret = getline(&cmd_buf, &n, stdin);
        if (ret == -1){
            DBG("Broken pipe, exiting... \n");
            module_close(2); 
        }

	DBG("CMD: <%s>", cmd_buf);

	PROCESS_CMD(SPEAK, do_speak) 
        else PROCESS_CMD(SOUND_ICON, do_sound_icon)
        else PROCESS_CMD(CHAR, do_char)
        else PROCESS_CMD(KEY, do_key)
        else PROCESS_CMD_NRP(STOP, do_stop) 
        else PROCESS_CMD_NRP(PAUSE, do_pause) 
        else PROCESS_CMD(SET, do_set) 
        else PROCESS_CMD(QUIT, do_quit) 
        else{
          printf("300 ERR UNKNOWN COMMAND\n"); 
          fflush(stdout);
        }
	
	xfree(cmd_buf);
    } 
}

#undef PROCESS_CMD
