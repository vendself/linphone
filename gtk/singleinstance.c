/*
linphone, gtk interface.
Copyright (C) 2011 Belledonne Communications SARL
Author: Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "linphone.h"

extern gchar *aupkg_file;		// Ref to main.c
extern gboolean g_enable_video;	// Ref to main.c

static ms_thread_t pipe_thread;
static ortp_pipe_t server_pipe=(ortp_pipe_t)-1;
static gboolean server_pipe_running=TRUE;
static char *pipe_name=NULL;

void linphone_gtk_set_menu_items(bool_t video_enabled);

gchar *make_name(const char *appname){
	const char *username=getenv("USER");
	if (username){
		return g_strdup_printf("%s-%s",appname,username);
	}
	return g_strdup(appname);
}

static gboolean execute_wakeup(char *uri){
	linphone_gtk_show_main_window();
	if (strlen(uri)>0)
	{
		//linphone_gtk_refer_received(linphone_gtk_get_core(),uri);
		char	szTemp[1024];
		char 	*pszAuPkgFile = NULL;
		char 	*pszSelectMedia = NULL;
		int		enabled_video = -1;
		
		strcpy(szTemp, uri);
		// uri 에서 uri 와 aupkg_file를 구분해야 한다.
		char *pszUri = strtok(szTemp, " \t\r\n");
		if(pszUri != NULL)
		{
			pszAuPkgFile = strtok(NULL, " \t\r\n");
			if(pszAuPkgFile != NULL)
			{
				strcpy(aupkg_file, pszAuPkgFile);
				pszSelectMedia = strtok(NULL, " \t\r\n");
				if(pszSelectMedia != NULL)
				{
					if(strcasecmp(pszSelectMedia, "audio") == 0)
					{
						enabled_video = FALSE;
					}
					else if(strcasecmp(pszSelectMedia, "video") == 0)
					{
						enabled_video = TRUE;
					}
				}
			}
			
			ms_message("execute_wakeup(%s), Uri=[%s], aupkg_file=[%s], enabled_video=[%d]", uri, pszUri, pszAuPkgFile, enabled_video);
			
			linphone_gtk_refer_received(linphone_gtk_get_core(), pszUri);
			if(enabled_video >= 0)
			{
				//LinphoneVideoPolicy policy = {0};
				//policy.automatically_initiate = policy.automatically_accept = (bool_t)enabled_video;
				//linphone_core_set_video_policy(linphone_gtk_get_core(), &policy);
				g_enable_video = (bool_t)enabled_video;
				linphone_gtk_set_menu_items(g_enable_video);
			}
		}
	}
	g_free(uri);
	return FALSE;
}

static void * server_pipe_thread(void *pointer){
	ortp_pipe_t child;
	
	do{
		child=ortp_server_pipe_accept_client(server_pipe);
		if (server_pipe_running && child!=(ortp_pipe_t)-1){
			char buf[256]={0};
			if (ortp_pipe_read(child,(uint8_t*)buf,sizeof(buf))>0){
				g_message("Received wakeup command with arg %s",buf);
				gdk_threads_enter();
				g_timeout_add(20,(GSourceFunc)execute_wakeup,g_strdup(buf));
				gdk_threads_leave();
			}
			ortp_server_pipe_close_client(child);
		}
	}while(server_pipe_running);
	ortp_server_pipe_close(server_pipe);
	return NULL;
}

static void linphone_gtk_init_pipe(const char *name){
	server_pipe=ortp_server_pipe_create(name);
	if (server_pipe==(ortp_pipe_t)-1){
		g_warning("Fail to create server pipe for name %s: %s",name,strerror(errno));
	}
	ms_thread_create(&pipe_thread,NULL,server_pipe_thread,NULL);
}

bool_t linphone_gtk_init_instance(const char *app_name, const char *addr_to_call){
	pipe_name=make_name(app_name);
	ortp_pipe_t p=ortp_client_pipe_connect(pipe_name);
	if (p!=(ortp_pipe_t)-1){
		uint8_t buf[256]={0};
		g_message("There is already a running instance.");
		if (addr_to_call!=NULL){
			//strncpy((char*)buf,addr_to_call,sizeof(buf)-1);
			sprintf((char*)buf, "%s %s %s", addr_to_call, aupkg_file, (g_enable_video==FALSE)?"audio":"video");
		}
		if (ortp_pipe_write(p,buf,sizeof(buf))==-1){
			g_error("Fail to send wakeup command to running instance: %s",strerror(errno));
		}else{
			g_message("Message to running instance sent.");
		}
		ortp_client_pipe_close(p);
		return FALSE;
	}else{
		linphone_gtk_init_pipe(pipe_name);
	}
	return TRUE;
}

void linphone_gtk_uninit_instance(void){
	if (server_pipe!=(ortp_pipe_t)-1){
		ortp_pipe_t client;
		server_pipe_running=FALSE;
		/*this is to unblock the accept() of the server pipe*/
		client=ortp_client_pipe_connect(pipe_name);
		ortp_pipe_write(client,(uint8_t*)" ",1);
		ortp_client_pipe_close(client);
		ms_thread_join(pipe_thread,NULL);
		server_pipe=(ortp_pipe_t)-1;
		g_free(pipe_name);
		pipe_name=NULL;
	}
}
