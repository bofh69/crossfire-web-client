
/*
 * static char *rcsid_sockets_c =
 *    "$Id$";
 */

/*
    CrossFire, A Multiplayer game for X-windows

    Copyright (C) 1992 Frank Tore Johansen

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

    The author can be reached via e-mail to mark@pyramid.com
*/

/* Made this either client or server specific for 0.95.2 release - getting
 * too complicated to keep them the same, and the common code is pretty much
 * frozen now.
 */


#ifdef DEBUG
#include <stdio.h>
#include <stdarg.h>
#endif

/* The LOG function is normally part of the libcross.  If client compile,
 * we need to supply something since some of the common code calls this.
 */
#include <client.h>
void LOG (int logLevel, char *format, ...)
{
#ifdef DEBUG
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
#endif
}
/* We don't care what these values are in the client, since
 * we toss them
 */
#define llevDebug 0
#define llevError 0
#include <errno.h>

#include <newclient.h>

void SockList_Init(SockList *sl)
{
    sl->len=0;
    sl->buf=NULL;
}

void SockList_AddChar(SockList *sl, char c)
{
    sl->buf[sl->len]=c;
    sl->len++;
}

void SockList_AddShort(SockList *sl, uint16 data)
{
    sl->buf[sl->len++]= (data>>8)&0xff;
    sl->buf[sl->len++] = data & 0xff;
}


void SockList_AddInt(SockList *sl, uint32 data)
{
    sl->buf[sl->len++]= (data>>24)&0xff;
    sl->buf[sl->len++]= (data>>16)&0xff;
    sl->buf[sl->len++]= (data>>8)&0xff;
    sl->buf[sl->len++] = data & 0xff;
}

/* Basically does the reverse of SockList_AddInt, but on
 * strings instead.  Same for the GetShort, but for 16 bits.
 */
int GetInt_String(unsigned char *data)
{
    return ((data[0]<<24) + (data[1]<<16) + (data[2]<<8) + data[3]);
}

short GetShort_String(unsigned char *data) {
    return ((data[0]<<8)+data[1]);
}

/* this readsfrom fd and puts the data in sl.  We return true if we think
 * we have a full packet, 0 if we have a partial packet.  The only processing
 * we do is remove the intial size value.  len (As passed) is the size of the
 * buffer allocated in the socklist.  We make the assumption the buffer is
 * at least 2 bytes long.
 */
 
int SockList_ReadPacket(int fd, SockList *sl, int len)
{
    int stat,toread,readsome=0;
    extern int errno;

    /* We already have a partial packet */
    if (sl->len<2) {
	do {
	    stat=read(fd, sl->buf + sl->len, 2-sl->len);
	} while ((stat==-1) && (errno==EINTR));
	if (stat<0) {
	    /* In non blocking mode, EAGAIN is set when there is no
	     * data available.
	     */
	    if (errno!=EAGAIN && errno!=EWOULDBLOCK) {
		perror("ReadPacket got an error.");
		LOG(llevDebug,"ReadPacket got error %d, returning 0",errno);
	    }
	    return 0;	/*Error */
	}
	if (stat==0) return -1;
	sl->len += stat;
#ifdef CS_LOGSTATS
	cst_tot.ibytes += stat;
	cst_lst.ibytes += stat;
#endif
	if (stat<2) return 0;	/* Still don't have a full packet */
	readsome=1;
    }
    /* Figure out how much more data we need to read.  Add 2 from the
     * end of this - size header information is not included.
     */
    toread = 2+(sl->buf[0] << 8) + sl->buf[1] - sl->len;
    if ((toread + sl->len) > len) {
	LOG(llevError,"SockList_ReadPacket: Want to read more bytes than will fit in buffer.\n");
	/* return error so the socket is closed */
	return -1;
    }
    do {
	do {
	    stat = read(fd, sl->buf+ sl->len, toread);
	} while ((stat<0) && (errno==EINTR));
	if (stat<0) {
	    if (errno!=EAGAIN && errno!=EWOULDBLOCK) {
		perror("ReadPacket got an error.");
		LOG(llevDebug,"ReadPacket got error %d, returning 0",errno);
	    }
	    return 0;	/*Error */
	}
	if (stat==0) return -1;
	sl->len += stat;
#ifdef CS_LOGSTATS
	cst_tot.ibytes += stat;
	cst_lst.ibytes += stat;
#endif
	toread -= stat;
	if (toread==0) return 1;
	if (toread < 0) {
	    LOG(llevError,"SockList_ReadPacket: Read more bytes than desired.");
	    return 1;
	}
    } while (toread>0);
    return 0;
}

/* This writes data to the socket.  we precede the len information on the
 * packet.  Len needs to be passed here because buf could be binary
 * data
 */
static int write_socket(int fd, unsigned char *buf, int len)
{
    int amt=0;
    unsigned char *pos=buf;

    /* If we manage to write more than we wanted, take it as a bonus */
    while (len>0) {
	do {
	    amt=write(fd, pos, len);
	} while ((amt<0) && (errno==EINTR));

	if (amt < 0) { /* We got an error */
	    LOG(llevError,"New socket (fd=%d) write failed.\n", fd);
	    return -1;
	}
	if (amt==0) {
	    LOG(llevError,"Write_To_Socket: No data written out.\n");
	}
	len -= amt;
	pos += amt;
    }
    return 0;
}

/* Send With Handling - cnum is the client number, msg is what we want
 * to send.
 */
int send_socklist(int fd,SockList  msg)
{
    unsigned char sbuf[2];

    sbuf[0] = ((uint32)(msg.len) >> 8) & 0xFF;
    sbuf[1] = ((uint32)(msg.len)) & 0xFF;

    write_socket(fd, sbuf, 2);
    return write_socket(fd, msg.buf, msg.len);
}

/* Takes a string of data, and writes it out to the socket. A very handy
 * shortcut function.
 */
int cs_write_string(int fd, char *buf, int len)
{
    SockList sl;

    sl.len = len;
    sl.buf = (unsigned char*)buf;
    return send_socklist(fd, sl);
}
