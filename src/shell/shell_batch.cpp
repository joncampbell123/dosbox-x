/*
 *  Copyright (C) 2002-2020  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "support.h"

BatchFile::BatchFile(DOS_Shell * host,char const * const resolved_name,char const * const entered_name, char const * const cmd_line) {
	location = 0;
	prev=host->bf;
	echo=host->echo;
	shell=host;
	char totalname[DOS_PATHLENGTH+4];
	DOS_Canonicalize(resolved_name,totalname); // Get fullname including drive specificiation
	cmd = new CommandLine(entered_name,cmd_line);
	filename = totalname;

	//Test if file is openable
	if (!DOS_OpenFile(totalname,(DOS_NOT_INHERIT|OPEN_READ),&file_handle)) {
		//TODO Come up with something better
		E_Exit("SHELL:Can't open BatchFile %s",totalname);
	}
	DOS_CloseFile(file_handle);
}

BatchFile::~BatchFile() {
	delete cmd;
	shell->bf=prev;
	shell->echo=echo;
}

bool BatchFile::ReadLine(char * line) {
	//Open the batchfile and seek to stored postion
	if (!DOS_OpenFile(filename.c_str(),(DOS_NOT_INHERIT|OPEN_READ),&file_handle)) {
		LOG(LOG_MISC,LOG_ERROR)("ReadLine Can't open BatchFile %s",filename.c_str());
		delete this;
		return false;
	}
	DOS_SeekFile(file_handle,&(this->location),DOS_SEEK_SET);

	uint8_t c=0;Bit16u n=1;
	char temp[CMD_MAXLINE];
emptyline:
	char * cmd_write=temp;
	do {
		n=1;
		DOS_ReadFile(file_handle,&c,&n);
		if (n>0) {
			/* Why are we filtering this ?
			 * Exclusion list: tab for batch files 
			 * escape for ansi
			 * backspace for alien odyssey */
			if (c>31 || c==0x1b || c=='\t' || c==8) {
				//Only add it if room for it (and trailing zero) in the buffer, but do the check here instead at the end
				//So we continue reading till EOL/EOF
				if (((cmd_write - temp) + 1) < (CMD_MAXLINE - 1))
					*cmd_write++ = (char)c;
			} else if (c!=0x1a) {
                            if (c != '\n' && c != '\r')
					shell->WriteOut(MSG_Get("SHELL_ILLEGAL_CONTROL_CHARACTER"), c, c);
                        }
		}
	} while (c!='\n' && n);
	*cmd_write=0;
	if (!n && cmd_write==temp) {
		//Close file and delete bat file
		DOS_CloseFile(file_handle);
		delete this;
		return false;	
	}
	if (!strlen(temp)) goto emptyline;
	if (temp[0]==':') goto emptyline;

	/* Now parse the line read from the bat file for % stuff */
	cmd_write=line;
	char * cmd_read=temp;
	while (*cmd_read) {
		if (*cmd_read == '%') {
			cmd_read++;
			if (cmd_read[0] == '%') {
				cmd_read++;
				if (((cmd_write - line) + 1) < (CMD_MAXLINE - 1))
					*cmd_write++ = '%';
				continue;
			}
			if (cmd_read[0] == '0') {  /* Handle %0 */
				const char *file_name = cmd->GetFileName();
				cmd_read++;
				size_t name_len = strlen(file_name);
				if (((size_t)(cmd_write - line) + name_len) < (CMD_MAXLINE - 1)) {
					strcpy(cmd_write,file_name);
					cmd_write += name_len;
				}
				continue;
			}
			char next = cmd_read[0];
			if(next > '0' && next <= '9') {
				/* Handle %1 %2 .. %9 */
				cmd_read++; //Progress reader
				next -= '0';
				if (cmd->GetCount()<(unsigned int)next) continue;
				std::string word;
				if (!cmd->FindCommand((unsigned int)next,word)) continue;
				size_t name_len = strlen(word.c_str());
				if (((size_t)(cmd_write - line) + name_len) < (CMD_MAXLINE - 1)) {
					strcpy(cmd_write,word.c_str());
					cmd_write += name_len;
				}
				continue;
			} else {
				/* Not a command line number has to be an environment */
				char * first = strchr(cmd_read,'%');
				/* No env afterall. Ignore a single % */
				if (!first) {/* *cmd_write++ = '%';*/continue;}
				*first++ = 0;
				std::string env;
				if (shell->GetEnvStr(cmd_read,env)) {
					const char* equals = strchr(env.c_str(),'=');
					if (!equals) continue;
					equals++;
					size_t name_len = strlen(equals);
					if (((size_t)(cmd_write - line) + name_len) < (CMD_MAXLINE - 1)) {
						strcpy(cmd_write,equals);
						cmd_write += name_len;
					}
				}
				cmd_read = first;
			}
		} else {
			if (((cmd_write - line) + 1) < (CMD_MAXLINE - 1))
				*cmd_write++ = *cmd_read++;
		}
	}
	*cmd_write = 0;
	//Store current location and close bat file
	this->location = 0;
	DOS_SeekFile(file_handle,&(this->location),DOS_SEEK_CUR);
	DOS_CloseFile(file_handle);
	return true;	
}

bool BatchFile::Goto(const char * where) {
	//Open bat file and search for the where string
	if (!DOS_OpenFile(filename.c_str(),128,&file_handle)) {
		LOG(LOG_MISC,LOG_ERROR)("SHELL:Goto Can't open BatchFile %s",filename.c_str());
		delete this;
		return false;
	}

	char cmd_buffer[CMD_MAXLINE];
	char * cmd_write;

	/* Scan till we have a match or return false */
	uint8_t c;Bit16u n;
again:
	cmd_write=cmd_buffer;
	do {
		n=1;
		DOS_ReadFile(file_handle,&c,&n);
		if (n>0) {
			if (c>31) {
				if (((cmd_write - cmd_buffer) + 1) < (CMD_MAXLINE - 1))
					*cmd_write++ = (char)c;
			} else if (c!=0x1a && c!=0x1b && c!='\t' && c!=8) {
                                if (c != '\n' && c != '\r')
					shell->WriteOut(MSG_Get("SHELL_ILLEGAL_CONTROL_CHARACTER"), c, c);
                        }
		}
	} while (c!='\n' && n);
	*cmd_write++ = 0;
	char *nospace = trim(cmd_buffer);
	if (nospace[0] == ':') {
		nospace++; //Skip :
		//Strip spaces and = from it.
		while(*nospace && (isspace(*reinterpret_cast<unsigned char*>(nospace)) || (*nospace == '=')))
			nospace++;

		//label is until space/=/eol
		const char* beginlabel = nospace;
		while(*nospace && !isspace(*reinterpret_cast<unsigned char*>(nospace)) && (*nospace != '=')) 
			nospace++;

		*nospace = 0;
		if (strcasecmp(beginlabel,where)==0) {
		//Found it! Store location and continue
			this->location = 0;
			DOS_SeekFile(file_handle,&(this->location),DOS_SEEK_CUR);
			DOS_CloseFile(file_handle);
			return true;
		}
	   
	}
	if (!n) {
		DOS_CloseFile(file_handle);
		delete this;
		return false;	
	}
	goto again;
	return false;
}

void BatchFile::Shift(void) {
	cmd->Shift(1);
}
