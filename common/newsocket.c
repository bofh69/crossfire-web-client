/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, please
 * see COPYING and LICENSE.
 *
 * The authors can be reached via e-mail at <crossfire@metalforge.org>.
 */

/**
 * @file
 * Made this either client or server specific for 0.95.2 release - getting too
 * complicated to keep them the same, and the common code is pretty much
 * frozen now.
 */

#include "client.h"

#include <errno.h>

#include "shared/newclient.h"
#include "script.h"

/**
 * Write at least a specified amount of data in a buffer to the socket unless
 * an error occurs.
 *
 * @param fd  Socket to write to.
 * @param buf Buffer with data to write.
 * @param len
 * @return
 */
static int write_socket(int fd, const unsigned char *buf, int len) {
    const unsigned char *pos = buf;
    int amt = 0;

    /* If we manage to write more than we wanted, take it as a bonus */
    while (len>0) {
        do {
#ifndef WIN32
            amt=write(fd, pos, len);
        } while ((amt<0) && ((errno==EINTR) || (errno=EAGAIN)));
#else
            amt=send(fd, pos, len, 0);
        }
        while ((amt<0) && (WSAGetLastError()==EINTR));
#endif
        if (amt < 0) { /* We got an error */
            LOG(LOG_ERROR,"write_socket","New socket (fd=%d) write failed: %s.\n", fd, strerror(errno));
            return -1;
        }
        if (amt==0) {
            LOG(LOG_ERROR,"write_socket","Write_To_Socket: No data written out.\n");
        }
        len -= amt;
        pos += amt;
    }

    beat_reset();
    return 0;
}

/**
 *
 * @param sl
 * @param buf
 */
void SockList_Init(SockList *sl, guint8 *buf)
{
    sl->len=0;
    sl->buf=buf + 2;    /* reserve two bytes for total length */
}

/**
 *
 * @param sl
 * @param c
 */
void SockList_AddChar(SockList *sl, char c)
{
    if (sl->len + 1 < MAX_BUF - 2){
        sl->buf[sl->len++]=c;
    }
    else{
        /*
         * Cast c to an unsigned short so it displays correctly in the error message.
         * Otherwise, it prints as a hexadecimal number in a funny box.
         *
         * SilverNexus 2014-06-12
         */
        LOG(LOG_ERROR,"SockList_AddChar","Could not write %hu to socket: Buffer full.\n", (unsigned short)c);
    }
}

/**
 *
 * @param sl
 * @param data
 */
void SockList_AddShort(SockList *sl, guint16 data)
{
    if (sl->len + 2 < MAX_BUF - 2){
        sl->buf[sl->len++] = (data>>8)&0xff;
        sl->buf[sl->len++] = data & 0xff;
    }
    else{
        LOG(LOG_ERROR,"SockList_AddShort","Could not write %hu to socket: Buffer full.\n", data);
    }
}

/**
 *
 * @param sl
 * @param data
 */
void SockList_AddInt(SockList *sl, guint32 data)
{
    if (sl->len + 4 < MAX_BUF - 2){
        sl->buf[sl->len++] = (data>>24)&0xff;
        sl->buf[sl->len++] = (data>>16)&0xff;
        sl->buf[sl->len++] = (data>>8)&0xff;
        sl->buf[sl->len++] = data & 0xff;
    }
    else{
        LOG(LOG_ERROR,"SockList_AddInt","Could not write %u to socket: Buffer full.\n", data);
    }
}

/**
 *
 * @param sl
 * @param str
 */
void SockList_AddString(SockList *sl, const char *str)
{
    int len = strlen(str);

    if (sl->len + len > MAX_BUF-2) {
        len = MAX_BUF-2 - sl->len;
    }
    memcpy(sl->buf + sl->len, str, len);
    sl->len += len;
}

/**
 * Send data from a socklist to the socket.
 *
 * @param sl
 * @param fd
 */
int SockList_Send(SockList *sl, int fd) {
    sl->buf[-2] = sl->len / 256;
    sl->buf[-1] = sl->len % 256;

    return write_socket(fd, sl->buf-2, sl->len+2);
}

/**
 *
 * @param data
 * @return
 */
char GetChar_String(const unsigned char *data)
{
    return (data[0]);
}

/**
 * The reverse of SockList_AddInt, but on strings instead.  Same for the
 * GetShort, but for 16 bits.
 *
 * @param data
 * @return
 */
int GetInt_String(const unsigned char *data)
{
    return ((data[0]<<24) + (data[1]<<16) + (data[2]<<8) + data[3]);
}

/**
 * The reverse of SockList_AddInt, but on strings instead.  Same for the
 * GetShort, but for 64 bits
 *
 * @param data
 * @return
 */
gint64 GetInt64_String(const unsigned char *data)
{
#ifdef WIN32
    return (((gint64)data[0]<<56) + ((gint64)data[1]<<48) +
            ((gint64)data[2]<<40) + ((gint64)data[3]<<32) +
            ((gint64)data[4]<<24) + ((gint64)data[5]<<16) + ((gint64)data[6]<<8) + (gint64)data[7]);
#else
    return (((guint64)data[0]<<56) + ((guint64)data[1]<<48) +
            ((guint64)data[2]<<40) + ((guint64)data[3]<<32) +
            ((guint64)data[4]<<24) + (data[5]<<16) + (data[6]<<8) + data[7]);
#endif
}

/**
 *
 * @param data
 * @return
 */
short GetShort_String(const unsigned char *data)
{
    return ((data[0]<<8)+data[1]);
}

/**
 * Reads from the socket and puts data into a socklist.  The only processing
 * done is to remove the initial size value. An assumption made is that the
 * buffer is at least 2 bytes long.
 *
 * @param fd   Socket to read from.
 * @param sl   Pointer to a buffer to put the read data.
 * @param len  Size of the buffer allocated to accept data.
 * @return     Return true if we think we have a full packet, 0 if we have
 *             a partial packet, or -1 if an error occurred.
 */
int SockList_ReadPacket(int fd, SockList *sl, int len)
{
    int stat, toread;

    /* We already have a partial packet */
    if (sl->len<2) {
#ifndef WIN32
        do {
            stat=read(fd, sl->buf + sl->len, 2-sl->len);
        } while ((stat==-1) && (errno==EINTR));
#else
        do {
            stat=recv(fd, sl->buf + sl->len, 2-sl->len, 0);
        } while ((stat==-1) && (WSAGetLastError()==EINTR));
#endif

        if (stat<0) {
            /* In non blocking mode, EAGAIN is set when there is no data
             * available.
             */
#ifndef WIN32
            if (errno!=EAGAIN && errno!=EWOULDBLOCK)
#else
            if (WSAGetLastError()!=EAGAIN && WSAGetLastError()!=WSAEWOULDBLOCK)
#endif
            {
                perror("ReadPacket got an error.");
                LOG(LOG_DEBUG,"SockList_ReadPacket","ReadPacket got error %d, returning -1",errno);
                return -1;
            }
            return 0;   /*Error */
        }
        if (stat==0) {
            return -1;
        }

        sl->len += stat;
#ifdef CS_LOGSTATS
        cst_tot.ibytes += stat;
        cst_lst.ibytes += stat;
#endif
        if (stat<2) {
            return 0;    /* Still don't have a full packet */
        }
    }

    /* Figure out how much more data we need to read.  Add 2 from the
     * end of this - size header information is not included.
     */
    toread = 2+(sl->buf[0] << 8) + sl->buf[1] - sl->len;
    if ((toread + sl->len) > len) {
        LOG(LOG_ERROR,"SockList_ReadPacket","Want to read more bytes than will fit in buffer.\n");
        /* return error so the socket is closed */
        return -1;
    }
    do {
#ifndef WIN32
        do {
            stat = read(fd, sl->buf+ sl->len, toread);
        } while ((stat<0) && (errno==EINTR));
#else
        do {
            stat = recv(fd, sl->buf+ sl->len, toread, 0);
        } while ((stat<0) && (WSAGetLastError()==EINTR));
#endif
        if (stat<0) {

#ifndef WIN32
            if (errno!=EAGAIN && errno!=EWOULDBLOCK)
#else
            if (WSAGetLastError()!=EAGAIN && WSAGetLastError()!=WSAEWOULDBLOCK)
#endif
            {
                perror("ReadPacket got an error.");
                LOG(LOG_DEBUG,"SockList_ReadPacket","ReadPacket got error %d, returning 0",errno);
            }
            return 0;       /*Error */
        }
        if (stat==0) {
            return -1;
        }
        sl->len += stat;

#ifdef CS_LOGSTATS
        cst_tot.ibytes += stat;
        cst_lst.ibytes += stat;
#endif
        toread -= stat;
        if (toread==0) {
            return 1;
        }

        if (toread < 0) {
            LOG(LOG_ERROR,"SockList_ReadPacket","SockList_ReadPacket: Read more bytes than desired.");
            return 1;
        }
    } while (toread>0);
    return 0;
}

/**
 * Send a printf-formatted packet to the socket.
 *
 * @param fd  The socket to send to.
 * @param str The printf format string.
 * @param ... An optional list of values to fulfill the format string.
 */
int cs_print_string(int fd, const char *str, ...)
{
    va_list args;
    SockList sl;
    guint8 buf[MAX_BUF];

    SockList_Init(&sl, buf);
    va_start(args, str);
    sl.len += vsprintf((char*)sl.buf + sl.len, str, args);
    va_end(args);

    script_monitor_str((char*)sl.buf);

    return SockList_Send(&sl, fd);
}
