# Copyright (C) 2003 Brailcom, o.p.s.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

"""Python API to Speech Daemon

Python client API to Speech Daemon is provided in a nice OO style by the class
called 'Client'.

"""

import socket
import string
from util import *

class _CommunicationError(Exception):
    def __init__(self, code, msg, data):
        Exception.__init__(self, "%s: %s" % (code, msg))
        self._code = code
        self._msg = msg
        self._data = data

    def code(self):
        """Return the server response error code as integer number."""
        return self._code
        
    def msg(self):
        """Return server response error message as string."""
        return self._msg

    
class CommandError(_CommunicationError):
    """Exception risen after receiving error response when command was sent."""

    def command(self):
        """Return the command string which resulted in this error."""
        return self._data

    
class SendDataError(_CommunicationError):
    """Exception risen after receiving error response when data were sent."""

    def data(self):
        """Return the data which resulted in this error."""
        return self._data
        
        
class Client:
    """Speech Daemon client.

    This class implements most of the SSIP commands and provides OO style
    Python interface to all Speech Daemon functions.  Each connection to Speech
    Daemon server is represented by one instance of this class.  

    """
    
    SPEECH_PORT = 9876
    """Default port number for server connections."""
    
    RECV_BUFFER_SIZE = 1024
    """Size of buffer for receiving server responses."""

    SSIP_NEWLINE = "\r\n"
    """SSIP newline character sequence as a string."""
    
    _SSIP_END_OF_DATA = SSIP_NEWLINE + '.'  + SSIP_NEWLINE
    
    _SSIP_END_OF_DATA_ESCAPED = SSIP_NEWLINE + '..'  + SSIP_NEWLINE
    
    def __init__(self, client_name='python', conn_name='001',
                 host='127.0.0.1', port=SPEECH_PORT):
        """Initialize the instance and connect to the server.

        Arguments:

          client_name -- client identification string
          conn_name -- connection identification string
          host -- server hostname or IP address as string
          port -- server port as number (default value %d)
        
        For more information on client identification strings see Speech Daemod
        documentation.
          
        """ % self.SPEECH_PORT
        
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.connect((socket.gethostbyname(host), port))
        name = '%s:%s' % (client_name, conn_name)
        self._send_command('SET', 'CLIENT_NAME', name)
        
    def _send_command(self, command, *args):
        # Send command with given arguments and check server responsse.
        cmd = ' '.join((command,) + tuple(map(str, args))) + self.SSIP_NEWLINE
        self._socket.send(cmd)
        code, msg, data = self._recv_response()
        if code/100 != 2:
            raise CommandError(code, msg, cmd)
        return code, msg, data

    def _send_data(self, data):
        # Send data and check server responsse.
        edata = string.replace(data,
                               self._SSIP_END_OF_DATA,
                               self._SSIP_END_OF_DATA_ESCAPED)
        self._socket.send(edata + self._SSIP_END_OF_DATA)
        code, msg, data = self._recv_response()
        if code/100 != 2:
            raise SendDataError(code, msg, edata)
        return code, msg
        
    def _recv_response(self):
        # Read response from server; responses can be multiline as defined in
        # SSIP.  Return the tuple (code, msg, data), code as integer, msg as
        # string and data as tuple of strings (all lines).
        eol = self.SSIP_NEWLINE
        response = self._socket.recv(self.RECV_BUFFER_SIZE)
        while (len(response) < len(eol) or \
               not response.endswith(eol) or \
               response.split(eol)[-2][3] != ' '):
            response += self._socket.recv(self.RECV_BUFFER_SIZE)
        data = []
        c = None
        for line in response.split(eol):
            code, sep, text = line[:3], line[3], line[4:]
            assert code.isalnum(), "Malformed data received from server!"
            assert c is None or code == c, "Code changed through one response!"
            assert sep in ('-', ' '), "Malformed data received from server!"
            if sep == ' ':
                msg = text
                return int(code), msg, tuple(data)
            data.append(text)
        
    def close(self):
        """Close the connection to Speech Daemon server."""
        #self._send_command('BYE')
        self._socket.close()
        
    def say(self, text, priority=2):
        """Say given message with given priority.

        Arguments:

          text -- message text to be spoken as string.
          
          priority -- integer number 1 2 or 3.  1 stands for highest priority.
            For detailed description see Speech Daemon documentation.
        
        This method is non-blocking;  it just sends the command, given
        message is queued on the server and the method returns immediately.

        Server response code is checked and exception ('CommandError' or
        'SendDataError') is risen in case of non 2xx return code.  For more
        information about server responses and codes, see SSIP documentation.
        
        """
        self._send_command('SET', 'PRIORITY', str(priority))
        self._send_command('SPEAK')
        self._send_data(text)
        
    def stop(self, all=FALSE):
        """Immediately stop speaking and discard messages in queues.

        Arguments:

          all -- if true, all messages from all clients will be stopped and
            discarded.  If false (the default), only messages from this client
            will be affected.
        
        """
        if all:
            self._send_command('STOP', 'ALL')
        else:
            self._send_command('STOP')

    def get_client_list(self):
        code, msg, data = self._send_command('HISTORY', 'GET', 'CLIENT_LIST')
        return data
        
        
# int
# spd_get_client_list(int fd, char **client_names, int *client_ids, int* active){
# 	char command[128];
# 	char *reply;
# 	int footer_ok;
# 	int count;
# 	char *record;
# 	int record_int;
# 	char *record_str;	

# 	sprintf(command, "HISTORY GET CLIENT_LIST\r\n");
# 	reply = (char*) send_data(fd, command, 1);

# 	footer_ok = parse_response_footer(reply);
# 	if(footer_ok != 1){
# 		free(reply);
# 		return -1;
# 	}
	
# 	for(count=0;  ;count++){
# 		record = (char*) parse_response_data(reply, count+1);
# 		if (record == NULL) break;
# //		MSG(3,"record:(%s)\n", record);
# 		record_int = get_rec_int(record, 0);
# 		client_ids[count] = record_int;
# //		MSG(3,"record_int:(%d)\n", client_ids[count]);
# 		record_str = (char*) get_rec_str(record, 1);
# 		assert(record_str!=NULL);
# 		client_names[count] = record_str;
# //		MSG(3,"record_str:(%s)\n", client_names[count]);		
# 		record_int = get_rec_int(record, 2);
# 		active[count] = record_int;
# //		MSG(3,"record_int:(%d)\n", active[count]);		
# 	}	
# 	return count;
# }


        
# int
# spd_stop_fd(int fd, int target)
# {
#   char helper[32];

#   sprintf(helper, "STOP %d\r\n", target);
#   if(!ret_ok(send_data(fd, helper, 1))) return -1;
#   return 0;
# }

# int
# spd_pause(int fd)
# {
#   char helper[16];

#   sprintf(helper, "PAUSE\r\n");
#   if(!ret_ok(send_data(fd, helper, 1))) return -1;

#   return 0;
# }

# int
# spd_pause_fd(int fd, int target)
# {
#   char helper[16];

#   sprintf(helper, "PAUSE %d\r\n", target);
#   if(!ret_ok(send_data(fd, helper, 1))) return -1;
#   return 0;
# }

# int
# spd_resume(int fd)
# {
# 	char helper[16];

# 	sprintf(helper, "RESUME\r\n");

# 	if(!ret_ok(send_data(fd, helper, 1))) return -1;
# 	return 0;
# }

# int
# spd_resume_fd(int fd, int target)
# {
# 	char helper[16];

# 	sprintf(helper, "RESUME %d\r\n", target);

# 	if(!ret_ok(send_data(fd, helper, 1))) return -1;
# 	return 0;
# }

# int
# spd_icon(int fd, int priority, char *name)
# {
# 	static char helper[256];
# 	char *buf;

# 	sprintf(helper, "SET PRIORITY %d\r\n", priority);
# 	if(!ret_ok(send_data(fd, helper, 1))) return 0;
	
# 	sprintf(helper, "SND_ICON %s\r\n", name);
# 	if(!ret_ok(send_data(fd, helper, 1))) return -1;

# 	return 0;
# }

# int
# spd_say_letter(int fd, int priority, int c)
# {
# 	static char helper[] = "letter_x\0";
# 	static int r;

# 	helper[7]=c;
# 	r = spd_icon(fd, priority, helper);
# 	return r;
# }

# int
# spd_say_sgn(int fd, int priority, int c)
# {
#     static char helper[] = "sgn_x\0";
#     static int r;

# 	helper[4]=c;
#  	r = spd_icon(fd, priority, helper);
#     return r;
# }
# int
# spd_say_digit(int fd, int priority, int c)
# {
# 	static char helper[] = "digit_x\0";
# 	static int r;

# 	helper[6]=c;
# 	r = spd_icon(fd, priority, helper);
# 	return r;
# }

# int
# spd_say_key(int fd, int priority, int c)
# {
# 	static int r;

# 	if (isalpha(c)) r = spd_say_letter(fd, priority, c);
# 	else if (isdigit(c)) r = spd_say_digit(fd, priority, c);
# 	else if (isgraph(c)) r = spd_say_sgn(fd, priority, c);
# 	else return -1;

# 	return r;	
# }

# int
# spd_voice_rate(int fd, int rate)
# {
# 	if(rate < -100) return -1;
# 	if(rate > +100) return -1;
# 	return 0;
# }

# int
# spd_voice_pitch(int fd, int pitch)
# {
# 	if(pitch < -100) return -1;
# 	if(pitch > +100) return -1;
# 	return 0;
# }

# int
# spd_voice_punctuation(int fd, int flag)
# {
# 	if((flag != 0)&&(flag != 1)&&(flag != 2)) return -1;
# 	return 0;
# }

# int
# spd_voice_cap_let_recognition(int fd, int flag)
# {
# 	if((flag != 0)&&(flag != 1)) return -1;
# 	return 0;
# }

# int
# spd_voice_spelling(int fd, int flag)
# {
# 	if((flag != 0)&&(flag != 1)) return -1;
# 	return 0;
# }

# int
# spd_history_say_msg(int fd, int id)
# {
# 	char helper[32];
# 	sprintf(helper,"HISTORY SAY ID %d\r\n",id);

# 	if(!ret_ok(send_data(fd, helper, 1))) return -1;
# 	return 0;
# }

# int
# spd_history_select_client(int fd, int id)
# {
# 	char helper[64];
# 	sprintf(helper,"HISTORY CURSOR SET %d FIRST\r\n",id);

# 	if(!ret_ok(send_data(fd, helper, 1))) return -1;
# 	return 0;
# }

# /* Takes the buffer, position of cursor in the buffer
#  * and key that have been hit and tries to handle all
#  * the speech output, tabulator completion and other
#  * things and returns the new string. The cursor should
#  * be moved then by the client program to the end of
#  * this returned string.
#  * The client should call this function and display it's
#  * output every time there is some input to this command line.
#  */


# int
# spd_get_message_list_fd(int fd, int target, int *msg_ids, char **client_names)
# {
# 	char command[128];
# 	char *reply;
# 	int header_ok;
# 	int count;
# 	char *record;
# 	int record_int;
# 	char *record_str;	

# 	sprintf(command, "HISTORY GET MESSAGE_LIST %d 0 20\r\n", target);
# 	reply = send_data(fd, command, 1);

# /*	header_ok = parse_response_header(reply);
# 	if(header_ok != 1){
# 		free(reply);
# 		return -1;
# 	}*/

# 	for(count=0;  ;count++){
# 		record = (char*) parse_response_data(reply, count+1);
# 		if (record == NULL) break;
# //		MSG(3,"record:(%s)\n", record);
# 		record_int = get_rec_int(record, 0);
# 		msg_ids[count] = record_int;
# //		MSG(3,"record_int:(%d)\n", msg_ids[count]);
# 		record_str = (char*) get_rec_str(record, 1);
# 		assert(record_str!=NULL);
# 		client_names[count] = record_str;
# //		MSG(3,"record_str:(%s)\n", client_names[count]);		
# 	}
# 	return count;
# }

# char*
# spd_command_line(int fd, char *buffer, int pos, int c){
# 	char* new_s;

# 	if (buffer == NULL){
# 		new_s = malloc(sizeof(char) * 32);		
# 		buffer = malloc(sizeof(char));
# 		buffer[0] = 0;
# 	}else{
# 	   	if ((pos > strlen(buffer)) || (pos < 0)) return NULL;
# 		new_s = malloc(sizeof(char) * strlen(buffer) * 2);
# 	}
# 	new_s[0] = 0;
		
# 	/* Speech output for the symbol. */
# 	spd_say_key(fd, 3, c);	
	
# 	/* TODO: */
# 	/* What kind of symbol do we have. */
# 		switch(c){
# 			/* Completion. */
# 			case '\t':	
# 					break;
# 			/* Other tasks. */	
# 			/* Deleting symbol */
# 			case '-':	/* TODO: Should be backspace. */
# 				strcpy(new_s, buffer);
# 				new_s[pos-1]=0;
# 				pos--;
# 				break;
# 			/* Adding symbol */
# 			default:	
# 				sprintf(new_s, "%s%c", buffer, c);
# 				pos++;
# 		}
# 	/* Speech output */

# 	/* Return the new string. */
# 	return new_s;
# }

# /* --------------------- Internal functions ------------------------- */

# int
# ret_ok(char *reply)
# {
# 	int err;

# 	err = get_err_code(reply);
		
# 	if ((err>=100) && (err<300)) return 1;
# 	if (err>=300) return 0;
	
# 	printf("reply:%d", err);
# 	FATAL("Internal error during communication.");
# }

# char*
# get_rec_part(char *record, int pos)
# {
# 	int i, n;
# 	char *part;
# 	int p = 0;

# 	part = malloc(sizeof(char)*strlen(record));
# 	for(i=0;i<=strlen(record)-1;i++){
# 		if (record[i]==' '){
# 			   	p++;
# 				continue;
# 		}
# 		if(p == pos){
# 			n = 0;
# 			for(  ;i<=strlen(record)-1;i++){
# 				if(record[i] != ' '){
# 						part[n] = record[i];
# 						n++;
# 				}else{
# 						part[n] = 0;
# 						return part;
# 				}
# 			}
# 			part[n] = 0;
# 			return part;
# 		}
# 	}
# 	return NULL;
# }

# char*
# get_rec_str(char *record, int pos)
# {
# 	return(get_rec_part(record, pos));
# }

# int
# get_rec_int(char *record, int pos)
# {
# 	char *num_str;
# 	int num;

# 	assert(record!=0);
# 	num_str = get_rec_part(record, pos);
# 	if(LIBSPEECHD_DEBUG) fprintf(debugg, "pos:%d, rec:%s num_str:_%s_", pos, record, num_str);
# 	if (!isanum(num_str)) return -9999;
# 	num = atoi(num_str);
# 	free(num_str);
# 	return num;
# }

# int parse_response_footer(char *resp)
# {
# 	int ret;
# 	int i;
# 	int n = 0;
# 	char footer[256];
# 	for(i=0;i<=strlen(resp)-1;i++){
# 		if (resp[i]=='\r'){
# 			i+=2;
# 			if(resp[i+3] == ' '){
# 				for(; i<=strlen(resp)-1; i++){
# 					if (resp[i] != '\r'){
# 							footer[n] = resp[i];
# 							n++;
# 					}else{
# 						footer[n]=0;
# 						ret = ret_ok(footer);
# 						return ret;
# 					}
# 				}					
# 			}
# 		}
# 	}
# 	ret = ret_ok(footer);

# 	return ret;
# }

# char*
# parse_response_data(char *resp, int pos)
# {
# 	int p = 1;
# 	char *data;
# 	int i;
# 	int n = 0;
		
# 	data = malloc(sizeof(char) * strlen(resp));
# 	assert(data!=NULL);

# 	if (resp == NULL) return NULL;
# 	if (pos<1) return NULL;
	
# 	for(i=0;i<=strlen(resp)-1;i++){
# 		if (resp[i]=='\r'){
# 			   	p++;
# 				/* Skip the LFCR sequence */
# 				i++;
# 				if(i+3 < strlen(resp)-1){
# 					if(resp[i+4] == ' ') return NULL;
# 				}
# 				continue;
# 		}
# 		if (p==pos){
# 			/* Skip the ERR code */
# 			i+=4;	
# 			/* Read the data */
# 			for(; i<=strlen(resp)-1; i++){
# 				if (resp[i] != '\r'){
# 						data[n] = resp[i];
# 						n++;
# 				}else{
# 					data[n]=0;
# 					return data;	
# 				}
# 			}	
# 			data[n]=0;
# 			break;
# 		}
# 	}
# 	free(data);
# 	return NULL;
# }

# int
# get_err_code(char *reply)
# {
# 	char err_code[4];
# 	int err;
	
# 	if (reply == NULL) return -1;
# 	if(LIBSPEECHD_DEBUG) fprintf(debugg,"send_data:	reply: %s\n", reply);

# 	err_code[0] = reply[0];	err_code[1] = reply[1];
# 	err_code[2] = reply[2];	err_code[3] = '\0';

# 	if(LIBSPEECHD_DEBUG) fprintf(debugg,"ret_ok: err_code:	|%s|\n", err_code);
   
# 	if(isanum(err_code)){
# 		err = atoi(err_code);
# 	}else{
# 		fprintf(debugg,"ret_ok: not a number\n");
# 		return -1;	
# 	}

# 	return err;
# }

# char*
# send_data(int fd, char *message, int wfr)
# {
# 	char *reply;
# 	int bytes;

# 	/* TODO: 1000?! */
# 	reply = malloc(sizeof(char) * 1000);
   
# 	/* write message to the socket */
# 	write(fd, message, strlen(message));

# 	message[strlen(message)] = 0;
# 	if(LIBSPEECHD_DEBUG) fprintf(debugg,"command sent:	|%s|", message);

# 	/* read reply to the buffer */
# 	if (wfr == 1){
# 		bytes = read(fd, reply, 1000);
# 		/* print server reply to as a string */
# 		reply[bytes] = 0; 
# 		if(LIBSPEECHD_DEBUG) fprintf(debugg, "reply from server:	%s\n", reply);
# 	}else{
# 		if(LIBSPEECHD_DEBUG) fprintf(debugg, "reply from server: no reply\n\n");
# 		return "NO REPLY";
# 	} 

# 	return reply;
# }

# /* isanum() tests if the given string is a number,
#  *  returns 1 if yes, 0 otherwise. */
# int
# isanum(char *str){
#     int i;
# 	if (str == NULL) return 0;
#     for(i=0;i<=strlen(str)-1;i++){
#         if (!isdigit(str[i]))   return 0;
#     }
#     return 1;
# }
