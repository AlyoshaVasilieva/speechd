
/*
 * dc_decl.h - Dotconf functions and types for Speech Dispatcher
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
 * $Id: dc_decl.h,v 1.20 2003-05-31 12:04:24 pdm Exp $
 */

#include "speechd.h"

int table_add(char *name, char *group);

char cur_mod_options[255];      /* Which section with parameters of output modules
                                   we are in? */

OutputModule* cur_mod;

/* define dotconf callbacks */
DOTCONF_CB(cb_LogFile);
DOTCONF_CB(cb_SndModule);
DOTCONF_CB(cb_AddTable);
DOTCONF_CB(cb_DefaultModule);
DOTCONF_CB(cb_DefaultRate);
DOTCONF_CB(cb_DefaultPitch);
DOTCONF_CB(cb_DefaultLanguage);
DOTCONF_CB(cb_DefaultPriority);
DOTCONF_CB(cb_DefaultPunctuationMode);
DOTCONF_CB(cb_DefaultPunctuationTable);
DOTCONF_CB(cb_PunctuationSome);
DOTCONF_CB(cb_DefaultSpellingTable);
DOTCONF_CB(cb_DefaultClientName);
DOTCONF_CB(cb_DefaultVoiceType);
DOTCONF_CB(cb_DefaultSpelling);
DOTCONF_CB(cb_DefaultCapLetRecognition);
DOTCONF_CB(cb_DefaultSpellingTable);
DOTCONF_CB(cb_DefaultCharacterTable);
DOTCONF_CB(cb_DefaultKeyTable);
DOTCONF_CB(cb_DefaultSoundTable);
DOTCONF_CB(cb_AddModule);
DOTCONF_CB(cb_EndAddModule);
DOTCONF_CB(cb_AddParam);
DOTCONF_CB(cb_AddVoice);
DOTCONF_CB(cb_ApolloLanguage);


/* define dotconf configuration options */
static const configoption_t options[] =
{
    {"LogFile", ARG_STR, cb_LogFile, 0, 0},
    {"SndModule", ARG_STR, cb_SndModule, 0, 0},
    {"AddTable", ARG_STR, cb_AddTable, 0, 0},
    {"DefaultModule", ARG_STR, cb_DefaultModule, 0, 0},
    {"DefaultRate", ARG_INT, cb_DefaultRate, 0, 0},
    {"DefaultPitch", ARG_INT, cb_DefaultPitch, 0, 0},
    {"DefaultLanguage", ARG_STR, cb_DefaultLanguage, 0, 0},
    {"DefaultPriority", ARG_INT, cb_DefaultPriority, 0, 0},
    {"DefaultPunctuationMode", ARG_STR, cb_DefaultPunctuationMode, 0, 0},
    {"DefaultPunctuationTable", ARG_STR, cb_DefaultPunctuationTable, 0, 0},
    {"PunctuationSome", ARG_STR, cb_PunctuationSome, 0, 0},
    {"DefaultClientName", ARG_STR, cb_DefaultClientName, 0, 0},
    {"DefaultVoiceType", ARG_INT, cb_DefaultVoiceType, 0, 0},
    {"DefaultSpelling", ARG_TOGGLE, cb_DefaultSpelling, 0, 0},
    {"DefaultSpellingTable", ARG_STR, cb_DefaultSpellingTable, 0, 0},
    {"DefaultCharacterTable", ARG_STR, cb_DefaultCharacterTable, 0, 0},
    {"DefaultKeyTable", ARG_STR, cb_DefaultKeyTable, 0, 0},
    {"DefaultSoundTable", ARG_STR, cb_DefaultSoundTable, 0, 0},
    {"DefaultCapLetRecognition", ARG_TOGGLE, cb_DefaultCapLetRecognition, 0, 0},
    {"AddModule", ARG_STR, cb_AddModule, 0, 0},
    {"EndAddModule", ARG_NONE, cb_EndAddModule, 0,0},
    {"AddParam", ARG_LIST, cb_AddParam, 0, 0},
    {"AddVoice", ARG_LIST, cb_AddVoice, 0, 0},
    {"ApolloLanguage", ARG_LIST, cb_ApolloLanguage, 0, 0},
    /*{"ExampleOption", ARG_STR, cb_example, 0, 0},
     *      {"MultiLineRaw", ARG_STR, cb_multiline, 0, 0},
     *           {"", ARG_NAME, cb_unknown, 0, 0},
     *                {"MoreArgs", ARG_LIST, cb_moreargs, 0, 0},*/
    LAST_OPTION
};

DOTCONF_CB(cb_LogFile)
{
    assert(cmd->data.str != NULL);
    if (!strncmp(cmd->data.str,"stdout",6)){
        logfile = (FILE*) malloc(sizeof(FILE*));
        logfile = stdout;
        return NULL;
    }
    if (!strncmp(cmd->data.str,"stderr",6)){
        logfile = (FILE*) malloc(sizeof(FILE*));
        logfile = stderr;
        return NULL;
    }
    logfile = fopen(cmd->data.str, "w");
    if (logfile == NULL){
        printf("Error: can't open logging file! Using stdout.\n");
        logfile = (FILE*) malloc(sizeof(FILE*));
        logfile = stdout;
    }
    MSG(2,"Speech Dispatcher Logging to file %s", cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_SndModule)
{
    sound_module = load_output_module(cmd->data.str);
    if (sound_module == NULL) FATAL("Couldn't load specified sound module");
    return NULL;
}


DOTCONF_CB(cb_AddTable)
{
    FILE *icons_file;
    char *language;
    char *tablename;
    GHashTable *icons_hash;
    char *helper;
    char *key;
    char *character;
    char *value;
    char *filename;
    char *line;
    char *group;
    int group_set = 0;
    char *bline;
	
    MSG(4,"Reading sound icons file...");
	
    language = NULL;
    tablename = NULL;
    line = (char*) spd_malloc(256);
    bline = (char*) spd_malloc(256);
    filename = (char*) spd_malloc(256);
	
    sprintf(filename, SYS_CONF"/%s", cmd->data.str);
    MSG(4, "Reading table from file %s", filename);
    if((icons_file = fopen(filename, "r"))==NULL){
        MSG(2,"Table %s file specified in speechd.conf doesn't exist!", filename);
        return NULL;	  
    }

    while(1){
        line = (char*) spd_malloc(256 * sizeof(char));
        if(fgets(line, 254, icons_file) == NULL){
            MSG(2, "Specified table %s empty or missing language, table or group identification.", filename);
            return NULL;
        }

        if(strlen(line) <= 2) continue;
        if ((line[0] == '#') && (line[1] == ' ')) continue;

        key = strtok(line,":\r\n");
        if(key == NULL) continue;
        g_strstrip(key);

        value = strtok(NULL,"\r\n");
        if(value == NULL){
            if(!strcmp(key,"definition")){
                if(tablename == NULL){
                    MSG(2, "Table name must preceed definition of symbols in [%s]!", filename);
                    return NULL;
                }
                if(language == NULL){
                    MSG(2, "Table language must preceed definition of symbols in [%s]!", filename);
                    return NULL;
                }
                if(group_set == 0){             
                    MSG(2, "Table group(s) must preceed definition of symbols in [%s]!", filename);
                    return NULL;
                }
                break;
            }            
            continue;
        }

        g_strstrip(value);

        if(!strcmp(key,"language"))  language = value;
        if(!strcmp(key,"table")) tablename = value;
        if(!strcmp(key,"group")){
            if(tablename == NULL){
                MSG(2, "Table name must preceed group definitions in [%s]!", filename);
                return NULL;
            }
            if(table_add(tablename, value) == 0) group_set = 1;
        }

    }

    MSG(4, "Parsing icons data: lang:%s name:%s", language, tablename);
    if(strlen(language)<2){
        MSG(2,"Invalid language code in table.");
        return NULL;
    }

    icons_hash = g_hash_table_lookup(snd_icon_langs, language);
    if(!icons_hash){
        icons_hash = g_hash_table_new(g_str_hash, g_str_equal);
        g_hash_table_insert(snd_icon_langs, language, icons_hash);
    }
    
    while(1){
        helper = (char*) spd_malloc(256*sizeof(char));
        if(fgets(helper, 255, icons_file) == NULL) break;
        g_strstrip(helper);
        strcpy(bline, helper);
        character = strtok(helper, "\"");

	    if (character == NULL){
            /* probably a black line or commentary */
            continue;
        }

        if(!strcmp(character,"\\")){
            strcpy(character,"\"");
            strtok(NULL, "="); /* skip the next quotes */
        }
        if(!strcmp(character,"\\\\")){
            strcpy(character,"\\");
        }

        key = malloc((strlen(character) + strlen(tablename) + 2) * sizeof(char));
        sprintf(key, "%s_%s", tablename, character);

        helper = strtok(NULL, "\r\n");
        g_strstrip(helper);
        value = strtok(helper, "=\r\n");

        if(value == NULL){
            MSG(2, "Bad syntax (no value for a given key) [%s] in %s", bline, filename);
            return NULL;
        }

        g_strstrip(value);
        g_hash_table_insert(icons_hash, key, value);
		MSG(5,"Pair |%s+%s| inserted", key, value);
    }

    return NULL;
}

DOTCONF_CB(cb_DefaultModule)
{
    GlobalFDSet.output_module = malloc(sizeof(char) * strlen(cmd->data.str) + 1);
    strcpy(GlobalFDSet.output_module,cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_DefaultRate)
{
    GlobalFDSet.rate = cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_DefaultPitch)
{
    GlobalFDSet.pitch = cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_DefaultLanguage)
{
    GlobalFDSet.language = malloc(sizeof(char) * strlen(cmd->data.str) + 1);
    strcpy(GlobalFDSet.language,cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_DefaultPriority)
{
    GlobalFDSet.priority = cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_DefaultPunctuationMode)
{
    assert(cmd->data.str != NULL);
    if(!strcmp(cmd->data.str, "all")) GlobalFDSet.punctuation_mode = 1;
    else if(!strcmp(cmd->data.str, "none")) GlobalFDSet.punctuation_mode = 0;
    else if(!strcmp(cmd->data.str, "some")) GlobalFDSet.punctuation_mode = 2;
    return NULL;
}

DOTCONF_CB(cb_PunctuationSome)
{
    assert(cmd->data.str != NULL);
    GlobalFDSet.punctuation_some = (char*) spd_malloc(sizeof(char) * (strlen(cmd->data.str) + 1));
    strcpy(GlobalFDSet.punctuation_some, cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_DefaultPunctuationTable)
{
    GlobalFDSet.punctuation_table = (char*) spd_malloc((strlen(cmd->data.str) + 1) * sizeof(char));
    strcpy(GlobalFDSet.punctuation_table, cmd->data.str);
    return NULL;
}


DOTCONF_CB(cb_DefaultClientName)
{
    GlobalFDSet.client_name = malloc(sizeof(char) * (strlen(cmd->data.str) + 1));
    strcpy(GlobalFDSet.client_name,cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_DefaultVoiceType)
{
    GlobalFDSet.voice_type = (EVoiceType)cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_DefaultSpelling)
{
    GlobalFDSet.spelling = cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_DefaultSpellingTable)
{
    GlobalFDSet.spelling_table = (char*) spd_malloc((strlen(cmd->data.str) + 1) * sizeof(char));
    strcpy(GlobalFDSet.spelling_table,cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_DefaultCharacterTable)
{
    GlobalFDSet.char_table = (char*) spd_malloc((strlen(cmd->data.str) + 1) * sizeof(char));
    strcpy(GlobalFDSet.char_table, cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_DefaultKeyTable)
{
    GlobalFDSet.key_table = (char*) spd_malloc((strlen(cmd->data.str) + 1) * sizeof(char));
    strcpy(GlobalFDSet.key_table,cmd->data.str);
    return NULL;
}

DOTCONF_CB(cb_DefaultSoundTable)
{
    GlobalFDSet.snd_icon_table = (char*) spd_malloc((strlen(cmd->data.str) + 1) * sizeof(char));
    strcpy(GlobalFDSet.snd_icon_table,cmd->data.str);
    return NULL;
}


DOTCONF_CB(cb_DefaultCapLetRecognition)
{
    GlobalFDSet.cap_let_recogn = cmd->data.value;
    return NULL;
}

DOTCONF_CB(cb_AddModule)
{
    if (cmd->data.str == NULL) FATAL("No output module name specified");
    cur_mod = load_output_module(cmd->data.str);
    if (cur_mod == NULL) FATAL("Couldn't load specified output module");
    assert(cur_mod->name != NULL);
    g_hash_table_insert(output_modules, cur_mod->name, cur_mod);

    return NULL;
}

DOTCONF_CB(cb_EndAddModule)
{
    if(cur_mod == NULL){
        FATAL("Configuration: Trying to end a BeginModuleOptions section that was never opened");       
    }

    if (init_output_module(cur_mod) == -1) FATAL("Couldn't initialize specified output module");

    cur_mod = NULL;
    return NULL;
}

DOTCONF_CB(cb_AddParam)
{
    char *key;
    char *value;

    if (cur_mod == NULL){
        MSG(2,"Output module parameter not inside an output modules section");
        return NULL;
    }

    if (cmd->data.list[0] == NULL){
        MSG(2,"Missing parameter name.");
        return NULL;
    }

    if (cmd->data.list[1] == NULL){
        MSG(2,"Missing option name for parameter %s.", cmd->data.list[0]);
        return NULL;
    }

    MSG(3,"Adding parameter: %s=%s", cmd->data.list[0],
        cmd->data.list[1]);
    
    key = (char*) spd_malloc(strlen(cmd->data.list[0]) * sizeof(char));
    value = (char*) spd_malloc(strlen(cmd->data.list[1]) * sizeof(char));
    strcpy(key, cmd->data.list[0]);
    strcpy(value, cmd->data.list[1]);

    g_hash_table_insert(cur_mod->settings.params, key, value);

    return NULL;
}

char*
set_voice(char *value){
    char *ret;
    if (value == NULL) return NULL;
    ret = (char*) spd_malloc((strlen(value) + 1) * sizeof(char));
    strcpy(ret, value);
    return ret;
}

DOTCONF_CB(cb_AddVoice)
{
    SPDVoiceDef *voices;
    char *language = cmd->data.list[0];
    char *symbolic = cmd->data.list[1];
    char *voicename = cmd->data.list[2];
    char *key;
    SPDVoiceDef *value;

   if (cur_mod == NULL){
        MSG(2,"Output module parameter not inside an output modules section");
        return NULL;
    }

    if (language == NULL){
        MSG(2,"Missing language.");
        return NULL;
    }

    if (symbolic == NULL){
        MSG(2,"Missing symbolic name.");
        return NULL;
    }

    if (voicename == NULL){
        MSG(2,"Missing voice name for %s", cmd->data.list[0]);
        return NULL;
    }

    MSG(3,"Adding voice: [%s] %s=%s", language,
        symbolic, voicename);

    voices = g_hash_table_lookup(cur_mod->settings.voices, language);
    if (voices == NULL){
        key = (char*) spd_malloc((strlen(language) + 1) * sizeof(char));
        strcpy(key, language);
        value = (SPDVoiceDef*) spd_malloc(sizeof(SPDVoiceDef));
        g_hash_table_insert(cur_mod->settings.voices, key, value);
        voices = value;
    }
    
    if (!strcmp(symbolic, "male1")) voices->male1 = set_voice(voicename);
    else if (!strcmp(symbolic, "male2")) voices->male2 = set_voice(voicename);
    else if (!strcmp(symbolic, "male3")) voices->male3 = set_voice(voicename);
    else if (!strcmp(symbolic, "female1")) voices->female1 = set_voice(voicename);
    else if (!strcmp(symbolic, "female2")) voices->female2 = set_voice(voicename);
    else if (!strcmp(symbolic, "female3")) voices->female3 = set_voice(voicename);
    else if (!strcmp(symbolic, "child_male")) voices->child_male = set_voice(voicename);
    else if (!strcmp(symbolic, "child_female")) voices->child_female = set_voice(voicename);
    else{
        MSG(2, "Unrecognized symbolic voice name.");
        return NULL;
    }

    return NULL;
}

DOTCONF_CB(cb_ApolloLanguage)
{
  /* This is just copy&paste&edit of cb_AddVoice.  I'm sorry, I don't want to
     spent my life with C programming... */
  
    SPDApolloLanguageDef *apollo_languages;
    char *language = cmd->data.list[0];
    char *rom = cmd->data.list[1];
    char *char_coding = cmd->data.list[2];
    char *key;
    SPDApolloLanguageDef *value;

    if (cur_mod == NULL){
        MSG(2,"Output module parameter not inside an output modules section");
        return NULL;
    }

    if (language == NULL){
        MSG(2,"Missing language.");
        return NULL;
    }

    if (rom == NULL){
        MSG(2,"Missing ROM number.");
        return NULL;
    }

    if (char_coding == NULL){
        MSG(2,"Missing character coding for %s", cmd->data.list[0]);
        return NULL;
    }

    apollo_languages = g_hash_table_lookup(cur_mod->settings.apollo_languages,
					   language);
    if (apollo_languages == NULL){
        key = (char*) spd_malloc((strlen(language) + 1) * sizeof(char));
        strcpy(key, language);
        value = (SPDApolloLanguageDef*) spd_malloc(sizeof(SPDApolloLanguageDef));
        g_hash_table_insert(cur_mod->settings.apollo_languages, key, value);
        apollo_languages = value;
    }

    return NULL;
}



int
table_add(char *name, char *group)
{
    char *p;

    if((name == NULL)||(strlen(name)<=1)){
        MSG(2,"Invalid table name!");
        return 0;
    }
    if((group == NULL)||(strlen(group)<=1)){
        MSG(2,"Invalid table group for %s!", name);
        return 0;
    }

    p = (char*) spd_malloc((strlen(name)+1) * sizeof(char));
    strcpy(p, name);

    if (!strcmp(group,"sound_icons"))
        tables.sound_icons = g_list_append(tables.sound_icons, p);
    else if(!strcmp(group,"spelling"))
        tables.spelling = g_list_append(tables.spelling, p);
    else if(!strcmp(group, "characters"))
        tables.characters = g_list_append(tables.characters, p);
    else if(!strcmp(group, "keys"))
        tables.keys = g_list_append(tables.keys, p);
    else if(!strcmp(group, "punctuation"))
        tables.punctuation = g_list_append(tables.punctuation, p);
    else return 1;

    return 0;
} 
