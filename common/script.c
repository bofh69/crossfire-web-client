const char *rcsid_common_script_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2003 Mark Wedel & Crossfire Development Team
    This source file also Copyright (C) 2003 Preston Crow

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    The author can be reached via e-mail to crossfire-devel@real-time.com
*/

/* This file has its own script.h for prototypes, so don't want to include
 * this when doing a 'make proto'
 */
#ifndef CPROTO


/*
 * This file handles the client-side scripting interface.
 *
 * Each script is an external process that keeps two pipes open between the
 * client and the script (one in each direction).  When the script starts,
 * it defaults to receiving no data from the client.  Normally, the first
 * command it sends to the client will be a request to have certain types
 * of data sent to the script as the client receives them from the server
 * (such as drawinfo commands).  The script can also request current
 * information from the client, such as the contents of the inventory or
 * the map data (either live or last viewed "fog-of-war" data).  The script
 * can also send commands for the client to pass to the server.
 *
 * Script Commands:
 *
 * watch <command type>
 *   whenever the server sends the given command type to the client, also send
 *   a copy to the script.
 *   Note that this checked before the client processes the command, so it will
 *   automatically handle new options that may be added in the future.
 *   If the command type is NULL, all commands are watched.
 *
 * unwatch <command type>
 *   turn off a previous watch command.  There may be a slight delay in
 *   response before the command is processed, so some unwanted data may
 *   still be sent to the script.
 *
 * request <data type>
 *   have the client send the given data to the script.
 *
 * issue [<repeat> <must_send>] <command>
 *   issue the specified command to the server.
 *   if <repeat> isn't numeric then the command is sent directly
 *   For "lock" and "mark" only, the parameters are converted to binary.
 *
 * draw <color> <text>
 *   display the text in the specified color as if the server had sent
 *   a drawinfo command.
 *
 * monitor
 *   send the script a copy of every command that is sent to the server.
 *
 * unmonitor
 *   turn off monitoring.
 *
 * sync <#>
 *   wait until the server has acknowledged all but <#> commands have been received
 *
 *
 * To implement this:
 *
 * Processing script commands: gtk/gx11.c:do_network() and
 * x11/x11.c:event_loop() are modified to also watch for input from scripts
 * in the select() call, in which case script_process(fd) in this file is
 * called.
 *
 * Handling watches: common/client.c:DoClient() is modified to pass a copy
 * of each command to script_watch() before checking for it in the table.
 *
 * Handling of monitor: common/player.c:send_command() is modified to pass
 * a copy of each command to script_monitor() before sending to the server.
 *
 * Handling of requests: global variables are directly accessed from within
 * this file.
 *
 * Handling of issues: send_command() is called directly.  Note that this
 * command will be sent to any scripts that are monitoring output.
 *
 * Launching new scripts: common/player.c:extended_command() is extended to
 * add a check for "script <scriptname>" as an additional command, calling
 * script_init().  Also added is the "scripts" command to list all running
 * scripts, the "scriptkill" command to terminate a script (close the pipes
 * and assume it takes the hint), and the "scripttell" command to send a
 * message to a running script.
 */

/*
 * Include files
 */

/*
This does not work under Windows for now.
Someday this will be fixed :)
*/

#ifndef WIN32
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#else
#include <windows.h>
#endif
#include <ctype.h>

#include <client.h>
#include <external.h>
#include <script.h>
#include <p_cmd.h>
#include "mapdata.h"

/*
 * Data structures
 */
struct script {
   char *name; /* the script name */
   char *params; /* the script parameters, if any */
#ifndef WIN32
   int out_fd; /* the file descriptor to which the client writes to the script */
   int in_fd; /* the file descriptor from which we read commands from the script */
#else
   HANDLE out_fd; /* the file descriptor to which the client writes to the script */
   HANDLE in_fd; /* the file descriptor from which we read commands from the script */
#endif /* WIN32 */
   int monitor; /* true if this script is monitoring commands sent to the server */
   int num_watch; /* number of commands we're watching */
   char **watch; /* array of commands that we're watching */
   int cmd_count; /* bytes already read in */
   char cmd[1024]; /* command from the script */
#ifndef WIN32
   int pid;
#else
   DWORD pid;	/* Handle to Win32 process ID */
   HANDLE process; /* Handle of Win32 process */
#endif
   int sync_watch;
};

/*
 * Global variables
 */
static struct script *scripts = NULL;
static int num_scripts = 0;

/*
 * Prototypes
 */
static int script_by_name(const char *name);
static void script_dead(int i);
static void script_process_cmd(int i);
static void send_map(int i,int x,int y);
static void script_send_item(int i, const char *head, const item *it);


/*
 * Functions
 */

#ifdef WIN32

#define write(x,y,z) emulate_write(x,y,z)
#define read(x,y,z) emulate_read(x,y,z)

static int emulate_read(HANDLE fd, char *buf, int len)
{
   DWORD dwBytesRead;
   BOOL	rc;

   FlushFileBuffers(fd);
   rc = ReadFile(fd, buf, len, &dwBytesRead, NULL);
   if (rc == FALSE)
      return(-1);
   buf[dwBytesRead] = '\0';

   return(dwBytesRead);
}

static int emulate_write(HANDLE fd, const char *buf, int len)
{
   DWORD dwBytesWritten;
   BOOL	rc;

   rc = WriteFile(fd, buf, len, &dwBytesWritten, NULL);
   FlushFileBuffers(fd);
   if (rc == FALSE)
      return(-1);

   return(dwBytesWritten);
}


#endif /* WIN32 */

void script_init(const char *cparams)
{
#ifndef WIN32
   int pipe1[2];
#ifdef USE_PIPE
   int pipe2[2];
#endif
   int pid;
   char *name, *args, params[MAX_BUF];

   if ( !cparams )
       {
       draw_info( "Please specifiy a script to launch!", NDI_RED );
       return;
       }

    /* cparams as passed in is a const value, so need to copy it
     * to data we can write over.
     */
    strncpy(params, cparams, MAX_BUF-1);
    params[MAX_BUF-1]=0;


   /* Get name and args */
   name=params;
   args=name;
   while ( *args && *args!=' ' ) ++args;
   while ( *args && *args==' ' ) *args++ = '\0';
   if ( *args==0 )
   {
      args=NULL;
   }

#ifdef USE_PIPE
   /* Create two pipes */
   if ( pipe(pipe1) )
   {
      draw_info("Unable to start script--pipe failed",NDI_RED);
      return;
   }
   if ( pipe(pipe2) )
   {
      close(pipe1[0]);
      close(pipe1[1]);
      draw_info("Unable to start script--pipe failed",NDI_RED);
      return;
   }
#else
   /* Create a pair of sockets */
   if ( socketpair(PF_LOCAL,SOCK_STREAM,AF_LOCAL,pipe1) )
   {
      draw_info("Unable to start script--socketpair failed",NDI_RED);
      return;
   }
#endif

   /* Fork */
   pid=fork();
   if (pid==-1)
   {
      close(pipe1[0]);
      close(pipe1[1]);
#ifdef USE_PIPE
      close(pipe2[0]);
      close(pipe2[1]);
#endif
      draw_info("Unable to start script--fork failed",NDI_RED);
      return;
   }

   /* Child--set stdin/stdout to the pipes, then exec */
   if ( pid==0 )
   {
      int i;
      int r;
      char *argv[256];

      /* Fill in argv[] */
      argv[0]=name;
      i=1;
      while (args && *args && i < sizeof(argv)/sizeof(*argv)-1)
      {
         argv[i++]=args;
         while ( *args && *args!=' ' ) ++args;
         while ( *args && *args==' ' ) *args++ = '\0';
      }
      argv[i]=NULL;

      /* Clean up file descriptor space */
      r=dup2(pipe1[0],0);
      if ( r != 0 ) {
         fprintf(stderr,"Script Child: Failed to set pipe1 as stdin\n");
      }
#ifdef USE_PIPE
      r=dup2(pipe2[1],1);
#else
      r=dup2(pipe1[0],1);
#endif
      if ( r != 1 ) {
         fprintf(stderr,"Script Child: Failed to set pipe2 as stdout\n");
      }
      for (i=3;i<100;++i) close(i);

      /* EXEC */
      r = execvp(argv[0],argv);

      /* If we get here, then there's been an failure of some sort.
       * In my case, it's often that I don't know what script name to
       * give to /script, so exec() can't find the script.
       *
       * Forward the error back to the client, using the script pipes.
       */

      if (r != -1) {
          printf("draw %d Script child: no error, but no execvp().\n", NDI_RED);
      } else {
          printf("draw %d Script child failed to start: %s\n", NDI_RED, strerror(errno));
      }

      exit(1);
   }

   /* Close the child's pipe ends */
   close(pipe1[0]);
#ifdef USE_PIPE
   close(pipe2[1]);
#endif

    if (fcntl(pipe1[1], F_SETFL, O_NDELAY)==-1) {
	    LOG(LOG_WARNING,"common::script_init","Error on fcntl.");
    }

   /* realloc script array to add new entry; fill in the data */
   scripts=realloc(scripts,sizeof(scripts[0])*(num_scripts+1));
   scripts[num_scripts].name=strdup(name);
   scripts[num_scripts].params=args?strdup(args):NULL;
   scripts[num_scripts].out_fd=pipe1[1];
#ifdef USE_PIPE
   scripts[num_scripts].in_fd=pipe2[0];
#else
   scripts[num_scripts].in_fd=pipe1[1];
#endif
   scripts[num_scripts].monitor=0;
   scripts[num_scripts].num_watch=0;
   scripts[num_scripts].watch=NULL;
   scripts[num_scripts].cmd_count=0;
   scripts[num_scripts].pid=pid;
   scripts[num_scripts].sync_watch = -1;
   ++num_scripts;

#else /* WIN32 */

   char *name,*args;
   char params[ MAX_BUF ];
   SECURITY_ATTRIBUTES saAttr;
   PROCESS_INFORMATION piProcInfo;
   STARTUPINFO siStartupInfo;
   HANDLE hChildStdinRd, hChildStdinWr, hChildStdinWrDup, hChildStdoutRd;
   HANDLE hChildStdoutWr, hChildStdoutRdDup, hSaveStdin, hSaveStdout;

   if ( !cparams )
        {
        draw_info( "Please specifiy a script to launch!", NDI_RED );
        return;
        }

   strncpy(params, cparams, MAX_BUF-1);
   params[MAX_BUF-1] = '\0';

   /* Get name and args */
   name=params;
   args=name;
   while ( *args && *args!=' ' ) ++args;
   while ( *args && *args==' ' ) *args++ = '\0';
   if ( *args==0 )
   {
      args=NULL;
   }

   saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
   saAttr.bInheritHandle = TRUE;
   saAttr.lpSecurityDescriptor = NULL;

   hSaveStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0))
   {
	   draw_info("Script support: stdout CreatePipe() failed", NDI_RED);
	   return;
   }

   if (!SetStdHandle(STD_OUTPUT_HANDLE, hChildStdoutWr))
   {
	   draw_info("Script support: failed to redirect stdout using SetStdHandle()", NDI_RED);
	   return;
   }

   if (!DuplicateHandle(GetCurrentProcess(), hChildStdoutRd, GetCurrentProcess(),
	   &hChildStdoutRdDup, 0, FALSE, DUPLICATE_SAME_ACCESS))
   {
	   draw_info("Script support: failed to duplicate stdout using DuplicateHandle()", NDI_RED);
	   return;
   }

   CloseHandle(hChildStdoutRd);

   hSaveStdin = GetStdHandle(STD_INPUT_HANDLE);
   if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0))
   {
	   draw_info("Script support: stdin CreatePipe() failed", NDI_RED);
	   return;
   }

   if (!SetStdHandle(STD_INPUT_HANDLE, hChildStdinRd))
   {
	   draw_info("Script support: failed to redirect stdin using SetStdHandle()", NDI_RED);
	   return;
   }

   if (!DuplicateHandle(GetCurrentProcess(), hChildStdinWr, GetCurrentProcess(),
	   &hChildStdinWrDup, 0, FALSE, DUPLICATE_SAME_ACCESS))
   {
	   draw_info("Script support: failed to duplicate stdin using DuplicateHandle()", NDI_RED);
	   return;
   }

   CloseHandle(hChildStdinWr);

   ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
   ZeroMemory(&siStartupInfo, sizeof(STARTUPINFO));
   siStartupInfo.cb = sizeof(STARTUPINFO);

   if (args)
	   args[-1] = ' ';

   if (!CreateProcess(NULL, name, NULL, NULL, TRUE, CREATE_NEW_PROCESS_GROUP, NULL, NULL, &siStartupInfo, &piProcInfo))
   {
	   draw_info("Script support: CreateProcess() failed", NDI_RED);
	   return;
   }

   CloseHandle(piProcInfo.hThread);

   if (args)
	   args[-1] = '\0';

	if (!SetStdHandle(STD_INPUT_HANDLE, hSaveStdin))
	{
		draw_info("Script support: restoring original stdin failed", NDI_RED);
		return;
	}

	if (!SetStdHandle(STD_OUTPUT_HANDLE, hSaveStdout))
	{
		draw_info("Script support: restoring original stdout failed", NDI_RED);
		return;
	}

   /* realloc script array to add new entry; fill in the data */
   scripts=realloc(scripts,sizeof(scripts[0])*(num_scripts+1));
   scripts[num_scripts].name=strdup(name);
   scripts[num_scripts].params=args?strdup(args):NULL;
   scripts[num_scripts].out_fd=hChildStdinWrDup;
   scripts[num_scripts].in_fd=hChildStdoutRdDup;
   scripts[num_scripts].monitor=0;
   scripts[num_scripts].num_watch=0;
   scripts[num_scripts].watch=NULL;
   scripts[num_scripts].cmd_count=0;
   scripts[num_scripts].pid=piProcInfo.dwProcessId;
   scripts[num_scripts].process = piProcInfo.hProcess;
   scripts[num_scripts].sync_watch = -1;
   ++num_scripts;

#endif /* WIN32 */
}

void script_sync(int commdiff)
{
   int i;

   if (commdiff<0) commdiff +=256;
   for (i=0;i<num_scripts; ++i) {
      if ( commdiff <= scripts[i].sync_watch && scripts[i].sync_watch >= 0 ) {
         char buf[1024];

         sprintf(buf,"sync %d\n",commdiff);
         write(scripts[i].out_fd,buf,strlen(buf));
         scripts[i].sync_watch = -1;
      }
   }
}

void script_list(void)
{
   if ( num_scripts == 0 )
   {
      draw_info("No scripts are currently running",NDI_BLACK);
   }
   else
   {
      int i;
      char buf[1024];

      sprintf(buf,"%d scripts currently running:",num_scripts);
      draw_info(buf,NDI_BLACK);
      for ( i=0;i<num_scripts;++i)
      {
         if ( scripts[i].params )
            sprintf(buf,"%d %s  %s",i+1,scripts[i].name,scripts[i].params);
         else
            sprintf(buf,"%d %s",i+1,scripts[i].name);
         draw_info(buf,NDI_BLACK);
      }
   }
}

void script_kill(const char *params)
{
   int i;

   /* Verify that the number is a valid array entry */
   i=script_by_name(params);
   if (i<0 || i>=num_scripts)
   {
      draw_info("No such running script",NDI_BLACK);
      return;
   }
#ifndef WIN32
   kill(scripts[i].pid,SIGHUP);
#else
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, scripts[i].pid);
#endif /* WIN32 */
   draw_info( "Killed script.", NDI_RED );
   script_dead(i);
}

#ifdef WIN32
void script_killall(void)
{
   while (num_scripts > 0)
   {
      GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, scripts[0].pid);
      script_dead(0);
   }
}
#endif /* WIN32 */

void script_fdset(int *maxfd,fd_set *set)
{
#ifndef WIN32
   int i;

   for ( i=0;i<num_scripts;++i)
   {
      FD_SET(scripts[i].in_fd,set);
      if ( scripts[i].in_fd >= *maxfd ) *maxfd = scripts[i].in_fd+1;
   }
#endif /* WIN32 */
}

void script_process(fd_set *set)
{
   int i;
   int r;
#ifdef WIN32
   DWORD nAvailBytes = 0;
   char cTmp;
   BOOL bRC;
   DWORD dwStatus;
   BOOL bStatus;
#endif


   /* Determine which script's fd is set */
   for(i=0;i<num_scripts;++i)
   {
#ifndef WIN32
      if ( FD_ISSET(scripts[i].in_fd,set) )
#else

      bStatus = GetExitCodeProcess(scripts[i].process,&dwStatus);
      bRC = PeekNamedPipe(scripts[i].in_fd, &cTmp, 1, NULL, &nAvailBytes, NULL);
	  if (nAvailBytes)
#endif /* WIN32 */
      {
         /* Read in script[i].cmd */
         r=read(scripts[i].in_fd,scripts[i].cmd+scripts[i].cmd_count,sizeof(scripts[i].cmd)-scripts[i].cmd_count-1);
         if ( r>0 )
         {
            scripts[i].cmd_count+=r;
         }
#ifndef WIN32
         else if ( r==0 || errno==EBADF )
#else
         else if ( r==0 || GetLastError() == ERROR_BROKEN_PIPE )
#endif
         {
            /* Script has exited; delete it */
            script_dead(i);
            return;
         }
         /* If a newline or full buffer has been reached, process it */
         scripts[i].cmd[scripts[i].cmd_count]=0; /* terminate string */
         while ( scripts[i].cmd_count == sizeof(scripts[i].cmd)-1
#ifndef WIN32
              || strchr(scripts[i].cmd,'\n') )
#else
              || strchr(scripts[i].cmd,'\r\n') )
#endif /* WIN32 */
         {
            script_process_cmd(i);
            scripts[i].cmd[scripts[i].cmd_count]=0; /* terminate string */
         }
         return; /* Only process one script at a time */
      }
#ifdef WIN32
	  else if (!bRC || ( bStatus && ( dwStatus != STILL_ACTIVE ) ) ) /* Error: assume dead */
		 script_dead(i);
#endif /* WIN32 */
   }
}

void script_watch(const char *cmd, uint8 *data, int len, enum CmdFormat format)
{
   int i;
   int w;
   int l;

   /* For each script... */
   for (i=0;i<num_scripts;++i)
   {
      /* For each watch... */
      for (w=0;w<scripts[i].num_watch;++w)
      {
         /* Does this command match our watch? */
         l=strlen(scripts[i].watch[w]);
         if ( !l || strncmp(cmd,scripts[i].watch[w],l)==0 )
         {
            char buf[10240];
            if ( !len ) sprintf(buf,"watch %s\n",cmd);
            else switch (format) {
               case ASCII:
                  sprintf(buf,"watch %s %s\n",cmd,data);
                  break;
               case SHORT_INT:
                  sprintf(buf,"watch %s %d %d\n",cmd,GetShort_String(data),GetInt_String(data+2));
                  break;
               case SHORT_ARRAY:
                  {
                     int be;
                     int p;

                     be=sprintf(buf,"watch %s",cmd);
                     for(p=0;p*2<len && p<100;++p) {
                        be+=sprintf(buf+be," %d",GetShort_String(data+p*2));
                     }
                     be+=sprintf(buf+be,"\n");
                  }
                  break;
               case INT_ARRAY:
                  {
                     int be;
                     int p;

                     be=sprintf(buf,"watch %s",cmd);
                     for(p=0;p*4<len;++p) {
                        be+=sprintf(buf+be," %d",GetInt_String(data+p*4));
                     }
                     be+=sprintf(buf+be,"\n");
                  }
                  break;
               case STATS:
		  {
                     /*
                      * We cheat here and log each stat as a separate command, even
                      * if the server sent a bunch of updates as a single message;
                      * most scripts will be easier to write if they only parse a fixed
                      * format.
                      */
                     int be = 0;
                     while (len) {
                        int c; /* which stat */

                        be+=sprintf(buf+be,"watch %s",cmd);
                        c=*data;
                        ++data; --len;
                        if (c>=CS_STAT_RESIST_START && c<=CS_STAT_RESIST_END) {
                           be+=sprintf(buf+be," resists %d %d\n",c,GetShort_String(data));
                           data+=2; len-=2;
                        } else if (c >= CS_STAT_SKILLINFO && c < (CS_STAT_SKILLINFO+CS_NUM_SKILLS)) {
                           be+=sprintf(buf+be," skill %d %d %" FMT64 "\n",c,*data,GetInt64_String(data+1));
                           data+=9; len-=9;
                        } else switch (c) {
                           case CS_STAT_HP:
                              be+=sprintf(buf+be," hp %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_MAXHP:
                              be+=sprintf(buf+be," maxhp %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_SP:
                              be+=sprintf(buf+be," sp %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_MAXSP:
                              be+=sprintf(buf+be," maxspp %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_GRACE:
                              be+=sprintf(buf+be," grace %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_MAXGRACE:
                              be+=sprintf(buf+be," maxgrace %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_STR:
                              be+=sprintf(buf+be," str %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_INT:
                              be+=sprintf(buf+be," int %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_POW:
                              be+=sprintf(buf+be," pow %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_WIS:
                              be+=sprintf(buf+be," wis %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_DEX:
                              be+=sprintf(buf+be," dex %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_CON:
                              be+=sprintf(buf+be," con %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_CHA:
                              be+=sprintf(buf+be," cha %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_EXP:
                              be+=sprintf(buf+be," exp %d\n",GetInt_String(data));
                              data+=4; len-=4; break;
                           case CS_STAT_EXP64:
                              be+=sprintf(buf+be," exp %" FMT64 "\n",GetInt64_String(data));
                              data+=8; len-=8; break;
                           case CS_STAT_LEVEL:
                              be+=sprintf(buf+be," level %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_WC:
                              be+=sprintf(buf+be," wc %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_AC:
                              be+=sprintf(buf+be," ac %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_DAM:
                              be+=sprintf(buf+be," dam %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_ARMOUR:
                              be+=sprintf(buf+be," armour %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_SPEED:
                              be+=sprintf(buf+be," speed %d\n",GetInt_String(data));
                              data+=4; len-=4; break;
                           case CS_STAT_FOOD:
                              be+=sprintf(buf+be," food %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_WEAP_SP:
                              be+=sprintf(buf+be," weap_sp %d\n",GetInt_String(data));
                              data+=4; len-=4; break;
                           case CS_STAT_FLAGS:
                              be+=sprintf(buf+be," flags %d\n",GetShort_String(data));
                              data+=2; len-=2; break;
                           case CS_STAT_WEIGHT_LIM:
                              be+=sprintf(buf+be," weight_lim %d\n",GetInt_String(data));
                              data+=4; len-=4; break;
                           case CS_STAT_SKILLEXP_AGILITY:
                           case CS_STAT_SKILLEXP_PERSONAL:
                           case CS_STAT_SKILLEXP_MENTAL:
                           case CS_STAT_SKILLEXP_PHYSIQUE:
                           case CS_STAT_SKILLEXP_MAGIC:
                           case CS_STAT_SKILLEXP_WISDOM:
                              be+=sprintf(buf+be," skillexp %d %d\n",c,GetInt_String(data));
                              data+=4; len-=4; break;
                           case CS_STAT_SKILLEXP_AGLEVEL:
                           case CS_STAT_SKILLEXP_PELEVEL:
                           case CS_STAT_SKILLEXP_MELEVEL:
                           case CS_STAT_SKILLEXP_PHLEVEL:
                           case CS_STAT_SKILLEXP_MALEVEL:
                           case CS_STAT_SKILLEXP_WILEVEL:
                              be+=sprintf(buf+be," skilllevel %d %d\n",c,GetShort_String(data));
                              data+=2; len-=2; break;

                           case CS_STAT_RANGE: {
                              int rlen=*data;
                              ++data; --len;
                              be+=sprintf(buf+be," range %*.*s\n",rlen,rlen,data);
                              data+=rlen; len-=rlen; break;
                           }
                           case CS_STAT_TITLE: {
                              int rlen=*data;
                              ++data; --len;
                              be+=sprintf(buf+be," title %*.*s\n",rlen,rlen,data);
                              data+=rlen; len-=rlen; break;
                           }
                           default:
                              be+=sprintf(buf+be," unknown %d %d bytes left\n",c,len);
                              len=0;
                        }
                     }
		  }
                  break;
               case MIXED:
                  /* magicmap */
                  /* mapextended */
                  /* item1 item2 */
                  /* upditem */
                  /* image image2 */
                  /* face face1 face2 */
                  /* sound */
                  /* player */
                  /*
                   * If we find that scripts need data from any of the above, we can
                   * write special-case code as with stats.  In the meantime, fall
                   * through and just give a hex dump.  Script writers should not
                   * depend on that data format.
                   */
               case NODATA:
               default: {
                     int be;
                     int p;

                     /*we may receive an null data, in which case len has no meaning*/
                     if (!data)
                        len=0;
                     be=sprintf(buf,"watch %s %d bytes unparsed:",cmd,len);
                     for(p=0;p<len && p<100;++p) {
                        be+=sprintf(buf+be," %02x",data[p]);
                     }
                     be+=sprintf(buf+be,"\n");
                  }
                  break;
            }
            write(scripts[i].out_fd,buf,strlen(buf));
         }
      }
   }
}

void script_monitor(const char *command, int repeat, int must_send)
{
   int i;

   /* For each script... */
   for (i=0;i<num_scripts;++i)
   {
      /* Do we send the command? */
      if ( scripts[i].monitor )
      {
         char buf[1024];

         sprintf(buf,"monitor %d %d %s\n",repeat,must_send,command);
         write(scripts[i].out_fd,buf,strlen(buf));
      }
   }
}

void script_monitor_str(const char *command)
{
   int i;

   /* For each script... */
   for (i=0;i<num_scripts;++i)
   {
      /* Do we send the command? */
      if ( scripts[i].monitor )
      {
         char buf[1024];

         sprintf(buf,"monitor %s\n",command);
         write(scripts[i].out_fd,buf,strlen(buf));
      }
   }
}

void script_tell(const char *params)
{
   int i;

   /* Find the script */
   i=script_by_name(params);
   if ( i<0 )
   {
      draw_info("No such running script",NDI_BLACK);
      return;
   }

   /* Send the message */
   write(scripts[i].out_fd,"scripttell ",11);
   write(scripts[i].out_fd,params,strlen(params));
   write(scripts[i].out_fd,"\n",1);
}

static int script_by_name(const char *name)
{
   int i;
   int l;

   if ( name==NULL )
   {
      return(num_scripts==1?0:-1);
   }

   /* Parse script number */
   if ( isdigit(*name) )
   {
      i=atoi(name);
      --i;
      if (i>=0 && i<num_scripts) return(i);
   }

   /* Parse script name */
   l=0;
   while ( name[l] && name[l]!=' ' ) ++l;
   for (i=0;i<num_scripts;++i)
   {
      if ( strncmp(name,scripts[i].name,l)==0 ) return(i);
   }
   return(-1);
}

static void script_dead(int i)
{
   int w;

   /* Release resources */
#ifndef WIN32
   close(scripts[i].in_fd);
   close(scripts[i].out_fd);
#else
   CloseHandle(scripts[i].in_fd);
   CloseHandle(scripts[i].out_fd);
   CloseHandle(scripts[i].process);
#endif
   free(scripts[i].name);
   free(scripts[i].params);
   for(w=0;w<scripts[i].num_watch;++w) free(scripts[i].watch[w]);
   free(scripts[i].watch);

#ifndef WIN32
   waitpid(-1,NULL,WNOHANG);
#endif

   /* Move scripts with higher index numbers down one slot */
   if ( i < (num_scripts-1) )
   {
      memmove(&scripts[i],&scripts[i+1],sizeof(scripts[i])*(num_scripts-i-1));
   }

   /* Update our count */
   --num_scripts;
}

static void send_map(int i,int x,int y)
{
   char buf[1024];

   if (x<0 || y<0 || the_map.x<=x || the_map.y<=y)
   {
      sprintf(buf,"request map %d %d unknown\n",x,y);
      write(scripts[i].out_fd,buf,strlen(buf));
   }
   /*** FIXME *** send more relevant data ***/
   sprintf(buf,"request map %d %d  %d %c %c %c %c"
           " smooth %d %d %d heads %d %d %d tails %d %d %d\n",
           x,y,the_map.cells[x][y].darkness,
           'n'+('y'-'n')*the_map.cells[x][y].need_update,
           'n'+('y'-'n')*the_map.cells[x][y].have_darkness,
           'n'+('y'-'n')*the_map.cells[x][y].need_resmooth,
           'n'+('y'-'n')*the_map.cells[x][y].cleared,
           the_map.cells[x][y].smooth[0],the_map.cells[x][y].smooth[1],the_map.cells[x][y].smooth[2],
           the_map.cells[x][y].heads[0].face,the_map.cells[x][y].heads[1].face,the_map.cells[x][y].heads[2].face,
           the_map.cells[x][y].tails[0].face,the_map.cells[x][y].tails[1].face,the_map.cells[x][y].tails[2].face
      );
      write(scripts[i].out_fd,buf,strlen(buf));
}

static void script_process_cmd(int i)
{
   char cmd[1024];
   char *c;
   int l;

   /*
    * Strip out just this one command
    */
   for (l=0;l<scripts[i].cmd_count;++l)
   {
      if ( scripts[i].cmd[l]=='\n' ) break;
   }
   ++l;
   memcpy(cmd,scripts[i].cmd,l);
#ifndef WIN32
   cmd[l-1]=0;
#else
   cmd[l-2]=0;
#endif
   if ( l<scripts[i].cmd_count )
   {
      memmove(scripts[i].cmd,scripts[i].cmd+l,scripts[i].cmd_count-l);
      scripts[i].cmd_count-=l;
   }
   else
   {
      scripts[i].cmd_count=0;
   }

   /*
    * Now the data in scripts[i] is ready for the next read.
    * We have a complete command in cmd[].
    * Process it.
    */
   /*
    * Script commands
    *
    * watch <command type>
    * unwatch <command type>
    * request <data type>
    * issue <repeat> <must_send> <command>
    * localcmd <command> [<params>]
    * draw <color> <text>
    * monitor
    * unmonitor
    */
   if ( strncmp(cmd,"sync",4)==0 ) {
      c=cmd+4;
      while ( *c && *c!=' ' ) ++c;
      while ( *c==' ' ) ++c;
      scripts[i].sync_watch = -1;
      if ( isdigit(*c) ) {
         scripts[i].sync_watch = atoi(c);
      }
      script_sync(csocket.command_sent - csocket.command_received); /* in case we are already there */
   }
   else if ( strncmp(cmd,"watch",5)==0 ) {
      c=cmd+5;
      while ( *c && *c!=' ' ) ++c;
      while ( *c==' ' ) ++c;
      c=strdup(c);
      scripts[i].watch=realloc(scripts[i].watch,(scripts[i].num_watch+1)*sizeof(scripts[i].watch[1]));
      scripts[i].watch[scripts[i].num_watch]=c;
      ++scripts[i].num_watch;
   }
   else if ( strncmp(cmd,"unwatch",7)==0 ) {
      int w;

      c=cmd+7;
      while ( *c && *c!=' ' ) ++c;
      while ( *c==' ' ) ++c;
      for (w=0;w<scripts[i].num_watch;++w) {
         if ( strcmp(c,scripts[i].watch[w])==0 ) {
            free(scripts[i].watch[w]);
            while ( w+1<scripts[i].num_watch ) {
               scripts[i].watch[w]=scripts[i].watch[w+1];
               ++w;
            }
            --scripts[i].num_watch;
            break;
         }
      }
   }
   else if ( strncmp(cmd,"request",7)==0 ) {
      c=cmd+7;
      while ( *c && *c!=' ' ) ++c;
      while ( *c==' ' ) ++c;
      if ( !*c ) return; /* bad request */
      /*
       * Request information from the client's view of the world
       * (mostly defined in client.h)
       *
       * Valid requests:
       *
       *   range        Return the type and name of the currently selected range attack
       *   stat <type>  Return the specified stats
       *   stat stats   Return Str,Con,Dex,Int,Wis,Pow,Cha
       *   stat cmbt    Return wc,ac,dam,speed,weapon_sp
       *   stat hp      Return hp,maxhp,sp,maxsp,grace,maxgrace,food
       *   stat xp      Return level,xp,skill-1 level,skill-1 xp,...
       *   stat resists Return resistances
       *   weight       Return maxweight, weight
       *   flags        Return flags (fire, run)
       *   items inv    Return a list of items in the inventory, one per line
       *   items actv   Return a list of inventory items that are active, one per line
       *   items on     Return a list of items under the player, one per line
       *   items cont   Return a list of items in the open container, one per line
       *   map pos      Return the players x,y within the current map
       *   map near     Return the 3x3 grid of the map centered on the player
       *   map all      Return all the known map information
       *   map <x> <y>  Return the information about square x,y in the current map
       */
      if ( strncmp(c,"range",5)==0 ) {
         char buf[1024];

         sprintf(buf,"request range %s\n",cpl.range);
         write(scripts[i].out_fd,buf,strlen(buf));
      }
      else if ( strncmp(c,"weight",5)==0 ) {
         char buf[1024];

         sprintf(buf,"request weight %d %d\n",cpl.stats.weight_limit,(int)(cpl.ob->weight*1000));
         write(scripts[i].out_fd,buf,strlen(buf));
      }
      else if ( strncmp(c,"stat ",5)==0 ) {
         c+=4;
         while ( *c && *c!=' ' ) ++c;
         while ( *c==' ' ) ++c;
         if ( !*c ) return; /* bad request */
         /*
          *   stat stats   Return Str,Con,Dex,Int,Wis,Pow,Cha
          *   stat cmbt    Return wc,ac,dam,speed,weapon_sp
          *   stat hp      Return hp,maxhp,sp,maxsp,grace,maxgrace,food
          *   stat xp      Return level,xp,skill-1 level,skill-1 xp,...
          *   stat resists Return resistances
          */
         if ( strncmp(c,"stats",5)==0 ) {
            char buf[1024];

            sprintf(buf,"request stat stats %d %d %d %d %d %d %d\n",cpl.stats.Str,cpl.stats.Con,cpl.stats.Dex,cpl.stats.Int,cpl.stats.Wis,cpl.stats.Pow,cpl.stats.Cha);
            write(scripts[i].out_fd,buf,strlen(buf));
         }
         if ( strncmp(c,"cmbt",4)==0 ) {
            char buf[1024];

            sprintf(buf,"request stat cmbt %d %d %d %d %d\n",cpl.stats.wc,cpl.stats.ac,cpl.stats.dam,cpl.stats.speed,cpl.stats.weapon_sp);
            write(scripts[i].out_fd,buf,strlen(buf));
         }
         if ( strncmp(c,"hp",2)==0 ) {
            char buf[1024];

            sprintf(buf,"request stat hp %d %d %d %d %d %d %d\n",cpl.stats.hp,cpl.stats.maxhp,cpl.stats.sp,cpl.stats.maxsp,cpl.stats.grace,cpl.stats.maxgrace,cpl.stats.food);
            write(scripts[i].out_fd,buf,strlen(buf));
         }
         if ( strncmp(c,"xp",2)==0 ) {
            char buf[1024];
            int s;

            sprintf(buf,"request stat xp %d %" FMT64 ,cpl.stats.level,cpl.stats.exp);
            write(scripts[i].out_fd,buf,strlen(buf));
            for(s=0;s<MAX_SKILL;++s) {
               sprintf(buf," %d %" FMT64 ,cpl.stats.skill_level[s],cpl.stats.skill_exp[s]);
               write(scripts[i].out_fd,buf,strlen(buf));
            }
            write(scripts[i].out_fd,"\n",1);
         }
         if ( strncmp(c,"resists",7)==0 ) {
            char buf[1024];
            int s;

            sprintf(buf,"request stat resists");
            write(scripts[i].out_fd,buf,strlen(buf));
            for(s=0;s<30;++s) {
               sprintf(buf," %d",cpl.stats.resists[s]);
               write(scripts[i].out_fd,buf,strlen(buf));
            }
            write(scripts[i].out_fd,"\n",1);
         }
      }
      else if ( strncmp(c,"flags",5)==0 ) {
         char buf[1024];

         sprintf(buf,"request flags %d %d %d %d\n",cpl.stats.flags,cpl.fire_on,cpl.run_on,cpl.no_echo);
         write(scripts[i].out_fd,buf,strlen(buf));
      }
      else if ( strncmp(c,"items ",6)==0 ) {
         c+=5;
         while ( *c && *c!=' ' ) ++c;
         while ( *c==' ' ) ++c;
         if ( !*c ) return; /* bad request */
         /*
          *   items inv    Return a list of items in the inventory, one per line
          *   items actv   Return a list of inventory items that are active, one per line
          *   items on     Return a list of items under the player, one per line
          *   items cont   Return a list of items in the open container, one per line
          */
         if ( strncmp(c,"inv",3)==0 ) {
            char *buf;

            item *it=cpl.ob->inv;
            while (it) {
               script_send_item(i,"request items inv ",it);
               it=it->next;
            }
            buf="request items inv end\n";
            write(scripts[i].out_fd,buf,strlen(buf));
         }
         if ( strncmp(c,"actv",4)==0 ) {
            char *buf;

            item *it=cpl.ob->inv;
            while (it) {
               if (it->applied) script_send_item(i,"request items actv ",it);
               it=it->next;
            }
            buf="request items actv end\n";
            write(scripts[i].out_fd,buf,strlen(buf));
         }
         if ( strncmp(c,"on",2)==0 ) {
            char *buf;

            item *it=cpl.below->inv;
            while (it) {
               script_send_item(i,"request items on ",it);
               it=it->next;
            }
            buf="request items on end\n";
            write(scripts[i].out_fd,buf,strlen(buf));
         }
         if ( strncmp(c,"cont",4)==0 ) {
            char *buf;

            item *it=cpl.container->inv;
            while (it) {
               script_send_item(i,"request items cont ",it);
               it=it->next;
            }
            buf="request items cont end\n";
            write(scripts[i].out_fd,buf,strlen(buf));
         }
      }
      else if ( strncmp(c,"map ",4)==0 ) {
         int x,y;

         c+=3;
         while ( *c && *c!=' ' ) ++c;
         while ( *c==' ' ) ++c;
         if ( !*c ) return; /* bad request */
         /*
          *   map pos      Return the players x,y within the current map
          *   map near     Return the 3x3 grid of the map centered on the player
          *   map all      Return all the known map information
          *   map <x> <y>  Return the information about square x,y in the current map
          */
         if ( strncmp(c,"pos",3)==0 ) {
            char buf[1024];

            sprintf(buf,"request map pos %d %d\n",pl_pos.x,pl_pos.y);
            write(scripts[i].out_fd,buf,strlen(buf));
         }
         else if ( strncmp(c,"near",4)==0 ) {
            for(y=0;y<3;++y)
               for(x=0;x<3;++x)
                  send_map(i,
                           x+pl_pos.x+use_config[CONFIG_MAPWIDTH]/2-1,
                           y+pl_pos.y+use_config[CONFIG_MAPHEIGHT]/2-1
                     );
         }
         else if ( strncmp(c,"all",3)==0 ) {
            char buf[1024];

            for(y=0;y<the_map.y;++y)
               for(x=0;x<the_map.x;++x)
                  send_map(i,x,y);
            sprintf(buf,"request map end\n");
            write(scripts[i].out_fd,buf,strlen(buf));
         }
         else {
            while ( *c && !isdigit(*c) ) ++c;
            if ( !*c ) return; /* No x specified */
            x=atoi(c);
            while ( *c && *c!=' ' ) ++c;
            while ( *c && !isdigit(*c) ) ++c;
            if ( !*c ) return; /* No y specified */
            y=atoi(c);
            send_map(i,x,y);
         }
      }
      else {
         char buf[1024];

         sprintf(buf,"Script %d %s malfunction; unimplemented request:",i+1,scripts[i].name);
         draw_info(buf,NDI_RED);
         draw_info(cmd,NDI_RED);
      }
   }
   else if ( strncmp(cmd,"issue",5)==0 ) {
      int repeat;
      int must_send;

      c=cmd+5;
      while ( *c && *c==' ' ) ++c;
      if ( *c && (isdigit(*c) || *c=='-') ) { /* repeat specified; use send_command() */
         repeat=atoi(c);
         while ( *c && *c!=' ' ) ++c;
         while ( *c && !isdigit(*c) && *c!='-' ) ++c;
         if ( !*c ) return; /* No must_send specified */
         must_send=atoi(c);
         while ( *c && *c!=' ' ) ++c;
         if ( !*c ) return; /* No command specified */
         while ( *c==' ' ) ++c;
         if ( repeat != -1 )
         {
            int r;

            r=send_command(c,repeat,must_send);
            if ( r!=1 ) {
               char buf[1024];

               sprintf(buf,"Script %d %s malfunction; command not sent",i+1,scripts[i].name);
               draw_info(buf,NDI_RED);
               draw_info(cmd,NDI_RED);
            }
         }
      }
      else
      {
         c=cmd+5;
         while ( *c && *c!=' ' ) ++c;
         while ( *c==' ' ) ++c;

         /*
          * Check special cases: "mark <tag>" or "lock <new state> <tag>"
          */
         if ( strncmp(c,"mark",4)==0 ) {
            int tag;
            SockList sl;
	    uint8 buf[MAX_BUF];

            c+=4;

            while ( *c && !isdigit(*c) ) ++c;
            if ( !*c ) return; /* No tag specified */
            tag=atoi(c);

            SockList_Init(&sl, buf);
            SockList_AddString(&sl, "mark ");
            SockList_AddInt(&sl, tag);
            SockList_Send(&sl, csocket.fd);
         }
         else if ( strncmp(c,"lock",4)==0 ) {
            int tag,locked;
            SockList sl;
	    uint8 buf[MAX_BUF];

            c+=4;

            while ( *c && !isdigit(*c) ) ++c;
            if ( !*c ) return; /* No state specified */
            locked=atoi(c);
            while ( *c && *c!=' ' ) ++c;
            while ( *c && !isdigit(*c) ) ++c;
            if ( !*c ) return; /* No tag specified */
            tag=atoi(c);

            SockList_Init(&sl, buf);
            SockList_AddString(&sl, "lock ");
            SockList_AddChar(&sl, locked);
            SockList_AddInt(&sl, tag);
            SockList_Send(&sl, csocket.fd);
         }
         else {
            cs_print_string(csocket.fd, "%s", c);
         }
      }
   }
   else if ( strncmp(cmd,"localcmd",8)==0){
      char* param;
      c=cmd+8;
      while (*c==' ') c++;
      param=c;
      while ( (*param!='\0') && (*param!=' ')) param++;
      if (*param==' '){
         *param='\0';
         param++;
      } else
         param=NULL;

      if (!handle_local_command(c, param)){
         char buf[1024];
         sprintf(buf,"Script %s malfunction; localcmd not understood",scripts[i].name);
         draw_info(buf,NDI_RED);
         sprintf(buf,"Script <<localcmd %s %s>>",c,(param==NULL)?"":param);
         draw_info(buf,NDI_RED);
      }
   }
   else if ( strncmp(cmd,"draw",4)==0 ) {
      int color;

      c=cmd+4;
      while ( *c && !isdigit(*c) ) ++c;
      if ( !*c ) return; /* No color specified */
      color=atoi(c);
      while ( *c && *c!=' ' ) ++c;
      if ( !*c ) return; /* No message specified */
      while ( *c==' ' ) ++c;
      draw_info(c,color);
   }
   else if ( strncmp(cmd,"monitor",7)==0 ) scripts[i].monitor=1;
   else if ( strncmp(cmd,"unmonitor",9)==0 ) scripts[i].monitor=0;
   else {
      char buf[1024];

      sprintf(buf,"Script %d %s malfunction; invalid command:",i+1,scripts[i].name);
      draw_info(buf,NDI_RED);
      draw_info(cmd,NDI_RED);
   }
}

/*
 * script_send_item()
 *
 * Send one line to the script with item information.
 *
 * A header string is passed in.  The format is:
 *
 * <header>  tag num weight flags type name
 *
 * flags are a bitmask:
 *   magic, cursed, damned, unpaid, locked, applied, open, was_open, inv_updated
 *    256     128     64      32       16      8       4      2         1
 */
static void script_send_item(int i, const char *head, const item *it)
{
   char buf[4096];
   int flags;

   flags=it->magical;
   flags= (flags<<1)|it->cursed;
   flags= (flags<<1)|it->damned;
   flags= (flags<<1)|it->unpaid;
   flags= (flags<<1)|it->locked;
   flags= (flags<<1)|it->applied;
   flags= (flags<<1)|it->open;
   flags= (flags<<1)|it->was_open;
   flags= (flags<<1)|it->inv_updated;
   sprintf(buf,"%s%d %d %f %d %d %s\n",head,it->tag,it->nrof,it->weight,flags,it->type,it->d_name);
   write(scripts[i].out_fd,buf,strlen(buf));
}

#endif /* CPROTO */
