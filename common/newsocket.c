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

#include "script.h"

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
 */
int SockList_Send(SockList *sl, GSocketConnection* c) {
    sl->buf[-2] = sl->len / 256;
    sl->buf[-1] = sl->len % 256;
    if (c == NULL) {
        LOG(LOG_WARNING, "SockList_Send", "Sending data while not connected!");
        return 1;
    }
    GOutputStream* out = g_io_stream_get_output_stream(G_IO_STREAM(c));
    bool ret = g_output_stream_write_all(out, sl->buf - 2, sl->len + 2, NULL,
                                         NULL, NULL);
    return ret ? 0 : -1;
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
 * Get an unsigned short from the stream.
 * Useful for checking size of server packets.
 *
 * @param data
 * The character stream to read.
 *
 * @return
 * The first two bytes, converted to a uint16.
 *
 * @note Currently static since it is only used in this file.
 * Can be added to the header and made non-static if needed elsewhere
 */
static guint16 GetUShort_String(const unsigned char data[static 2]) // We want at least two characters.
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
bool SockList_ReadPacket(GSocketConnection c[static 1], SockList sl[static 1],
                         size_t len, GError** error) {
    GInputStream* in = g_io_stream_get_input_stream(G_IO_STREAM(csocket.fd));
    gsize read;
    if (!g_input_stream_read_all(in, sl->buf, 2, &read, NULL, error)) {
        return false;
    }
    if (read != 2) {
        sl->len = 0;
        return true;
    }
    size_t to_read = (size_t)GetUShort_String(sl->buf);
    if (to_read + 2 > len) {
        g_set_error(error, CLIENT_ERROR, CLIENT_ERROR_TOOBIG,
                    "Server packet too big");
        return false;
    }
    if (!g_input_stream_read_all(in, sl->buf + 2, to_read, &read, NULL, error)) {
        return false;
    }
    if (read != to_read) {
        sl->len = 0;
        return true;
    }
    sl->len = to_read + 2;
#ifdef CS_LOGSTATS
    cst_tot.ibytes += sl->len;
    cst_lst.ibytes += sl->len;
#endif
    return true;
}

/**
 * Send a printf-formatted packet to the socket.
 *
 * @param fd  The socket to send to.
 * @param str The printf format string.
 * @param ... An optional list of values to fulfill the format string.
 */
int cs_print_string(GSocketConnection* fd, const char *str, ...)
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
