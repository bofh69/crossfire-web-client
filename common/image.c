const char *rcsid_common_image_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2001 Mark Wedel & Crossfire Development Team

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

/*
 * This file contains image related functions - this is a higher level up -
 * it mostly deals with the caching of the images, processing the image commands
 * from the server, etc.
 */

#include "config.h"
#include <stdlib.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include <direct.h>
#endif
#include <ctype.h>

#include "client.h"
#include "external.h"

/* Rotate right from bsd sum. */
#define ROTATE_RIGHT(c) if ((c) & 01) (c) = ((c) >>1) + 0x80000000; else (c) >>= 1;

/*#define CHECKSUM_DEBUG*/


struct FD_Cache {
    char    name[MAX_BUF];
    int	    fd;
} fd_cache[MAX_FACE_SETS];


/* given the filename, this tries to load the data.  It returns 0 success,
 * -1 on failure.  It returns the data and len, the passed
 * options.  csum is set to zero or unset - changes have made it such
 * that the caller knows whether or not the checksum matches, so there
 * is little point to re-do it.
 * data should be a buffer already allocated.
 */

static int load_image(char *filename, uint8 *data, int *len, uint32 *csum)
{
    int fd, i;
    char *cp;

    /* If the name includes an @, then that is a combined image file,
     * so we need to load the image a bit specially.  By using these
     * combined image files, it reduces number of opens needed.
     * In fact, we keep track of which ones we have opened to improve
     * performance.  Note that while not currently done, this combined
     * image scheme could be done when storing images in the players
     * ~/.crossfire-images directory.
     */
    if ((cp=strchr(filename,'@'))!=NULL) {
	char *lp;
	int offset, length, last=-1;

	offset = atoi(cp + 1);
	lp = strchr(cp,':');
	if (!lp) {
	    LOG(LOG_ERROR,"common::load_image","Corrupt filename - has '@' but no ':' ?(%s)", filename);
	    return -1;
	}
	length = atoi(lp + 1);
	*cp = 0;
	for (i=0; i<MAX_FACE_SETS; i++) {
	    if (!strcmp(fd_cache[i].name, filename)) break;
	    if (last==-1 && fd_cache[i].fd == -1) last = i;
	}
	/* Didn't find a matching entry yet, so make one */
	if (i == MAX_FACE_SETS) {
	    if (last == -1) {
		LOG(LOG_WARNING,"common::load_image","fd_cache filled up?  unable to load matching cache entry");
		*cp = '@';	/* put @ back in string */
		return -1;
	    }
#ifdef WIN32
	    if ((fd_cache[last].fd = open(filename, O_RDONLY | O_BINARY))==-1) {
#else
	    if ((fd_cache[last].fd = open(filename, O_RDONLY))==-1) {
#endif
		LOG(LOG_WARNING,"common::load_image","unable to load listed cache file %s",filename);
		*cp = '@';	/* put @ back in string */
		return -1;
	    }
	    strcpy(fd_cache[last].name, filename);
	    i=last;
	}
	lseek(fd_cache[i].fd, offset, SEEK_SET);
#ifdef WIN32
	*len = read(fd_cache[i].fd, data, length);
#else
	*len = read(fd_cache[i].fd, data, 65535);
#endif
	*cp = '@';
    }
    else {
#ifdef WIN32
    int length = 0;
	if ((fd=open(filename, O_RDONLY | O_BINARY))==-1) return -1;
    length = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    *len=read(fd, data, length);
#else
	if ((fd=open(filename, O_RDONLY))==-1) return -1;
	*len=read(fd, data, 65535);
#endif
	close(fd);
    }

    face_info.cache_hits++;
    *csum = 0;
    return 0;

#if 0
    /* Shouldn't be needed anymore */
	*csum=0;
	for (i=0; i<*len; i++) {
	    ROTATE_RIGHT(*csum);
	    *csum += data[i];
	    *csum &= 0xffffffff;
	}
#endif

}
/*******************************************************************************
 *
 * This is our image caching logic.  We use a hash to make the name lookups
 * happen quickly - this is done for speed, but also because we don't really
 * have a good idea on how many images may used.  It also means that as the
 * cache gets filled up with images in a random order, the lookup is still
 * pretty quick.
 *
 * If a bucket is filled with an entry that is not of the right name,
 * we store/look for the correct one in the next bucket.
 *
 *******************************************************************************
 */

/* This should be a power of 2 */
#define IMAGE_HASH  8192

Face_Information face_info;

char	home_dir[MAX_BUF];

/* This holds the name we recieve with the 'face' command so we know what
 * to save it as when we actually get the face.
 */
static char *facetoname[MAXPIXMAPNUM];


struct Image_Cache {
    char    *image_name;
    struct Cache_Entry	*cache_entry;
} image_cache[IMAGE_HASH];


/* This function is basically hasharch from the server, common/arch.c
 * a few changes - first, we stop processing when we reach the first
 * . - this is because I'm not sure if hashing .111 at the end of
 * all the image names will be very useful.
 */

static uint32 image_hash_name(char *str, int tablesize) {
    uint32 hash = 0;
    char *p;

    /* use the same one-at-a-time hash function the server now uses */
    for (p = str; *p!='\0' && *p != '.'; p++) {
        hash += *p;
        hash += hash << 10;
        hash ^= hash >>  6;
    }
    hash += hash <<  3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash % tablesize;
}

/* This function returns an index into the image_cache for
 * a matching entry, -1 if no match is found.
 */
static sint32 image_find_hash(char *str)
{
    uint32  hash = image_hash_name(str, IMAGE_HASH), newhash;

    newhash = hash;
    do {
	/* No entry - return immediately */
	if (image_cache[newhash].image_name == NULL) return -1;
	if (!strcmp(image_cache[newhash].image_name, str)) return newhash;
	newhash ++;
	if (newhash == IMAGE_HASH) newhash=0;
    } while (newhash != hash);

    /* If the hash table is full, this is bad because we won't be able to
     * add any new entries.
     */
    LOG(LOG_WARNING,"common::image_find_hash","Hash table is full, increase IMAGE_CACHE size");
    return -1;
}

static void image_remove_hash(char *imagename, Cache_Entry *ce)
{
    int	hash_entry;
    Cache_Entry	*last;

    hash_entry = image_find_hash(imagename);
    if (hash_entry == -1) {
	LOG(LOG_ERROR,"common::image_remove_hash","Unable to find cache entry for %s, %s", imagename, ce->filename);
	return;
    }
    if (image_cache[hash_entry].cache_entry == ce) {
	image_cache[hash_entry].cache_entry = ce->next;
	free(ce->filename);
	free(ce);
	return;
    }
    last = image_cache[hash_entry].cache_entry;
    while (last->next && last->next != ce) last=last->next;
    if (!last->next) {
	LOG(LOG_ERROR,"common::image_rmove_hash","Unable to find cache entry for %s, %s", imagename, ce->filename);
	return;
    }
    last->next = ce->next;
    free(ce->filename);
    free(ce);
}



/* This finds and returns the Cache_Entry of the image that matches name
 * and checksum if has_sum is set.  If has_sum is not set, we can't
 * do a checksum comparison.
 */

static Cache_Entry *image_find_cache_entry(char *imagename, uint32 checksum, int has_sum) 
{
    int	hash_entry;
    Cache_Entry	*entry;

    hash_entry = image_find_hash(imagename);
    if (hash_entry == -1) return NULL;
    entry = image_cache[hash_entry].cache_entry;
    if (has_sum) {
	while (entry) {
	    if (entry->checksum == checksum) break;
	    entry = entry->next;
	}
    }
    return entry;   /* This could be NULL */
}

/* Add a hash entry.  Returns the entry we added, NULL on failure.. */
static Cache_Entry *image_add_hash(char *imagename, char *filename, uint32 checksum, uint32 ispublic)
{
    Cache_Entry *new_entry;
    uint32  hash = image_hash_name(imagename, IMAGE_HASH), newhash;

    newhash = hash;
    while (image_cache[newhash].image_name != NULL && 
      strcmp(image_cache[newhash].image_name, imagename)) {
	newhash ++;
	if (newhash == IMAGE_HASH) newhash=0;
	/* If the hash table is full, can't do anything */
	if (newhash == hash) {
	    LOG(LOG_WARNING,"common::image_find_hash","Hash table is full, increase IMAGE_CACHE size");
	    return NULL;
	}
    }
    if (!image_cache[newhash].image_name) {
	image_cache[newhash].image_name = strdup(imagename);
    }

    /* We insert the new entry at the start of the list of the buckets
     * for this entry.  In the case of the players entries, this probably
     * improves performance, presuming ones later in the file are more likely
     * to be used compared to those at the start of the file.
     */
    new_entry = malloc(sizeof(struct Cache_Entry));
    new_entry->filename = strdup(filename);
    new_entry->checksum = checksum;
    new_entry->ispublic = ispublic;
    new_entry->image_data = NULL;
    new_entry->next = image_cache[newhash].cache_entry;
    image_cache[newhash].cache_entry = new_entry;
    return new_entry;
}

/* Process a line from the bmaps.client file.  In theory, the
 * format should be quite strict, as it is computer generated,
 * but we try to be lenient/follow some conventions.
 * Note that this is destructive to the data passed in line.
 */
static void image_process_line(char *line, uint32 ispublic)
{
    char imagename[MAX_BUF], filename[MAX_BUF];
    uint32 checksum;

    if (line[0] == '#') return;		    /* Ignore comments */

    if (sscanf(line, "%s %u %s", imagename, &checksum, filename)==3) {
	image_add_hash(imagename, filename, checksum, ispublic);
    } else {
	LOG(LOG_WARNING,"common::image_process_line","Did not parse line %s properly?", line);
    }
}

void init_common_cache_data()
{
    FILE *fp;
    char    bmaps[MAX_BUF], inbuf[MAX_BUF];
    int i;

    if (!want_config[CONFIG_CACHE])
	return;

    for (i = 0; i < MAXPIXMAPNUM; i++)
	facetoname[i] = NULL;

    /* First, make sure that image_cache is nulled out */
    memset(image_cache, 0, IMAGE_HASH * sizeof(struct Image_Cache));


    sprintf(bmaps,"%s/bmaps.client",DATADIR);
    if ((fp=fopen(bmaps,"r"))!=NULL) {
	while (fgets(inbuf, MAX_BUF-1, fp)!=NULL) {
	    image_process_line(inbuf, 1);
	}
	fclose(fp);
    } else {
	sprintf(inbuf,"Unable to open %s.  You may wish to download and install the image file to improve performance.\n", bmaps);
	draw_info(inbuf, NDI_RED);
    }

    sprintf(bmaps,"%s/.crossfire/crossfire-images/bmaps.client", getenv("HOME"));
    if ((fp=fopen(bmaps,"r"))!=NULL) {
	while (fgets(inbuf, MAX_BUF-1, fp)!=NULL) {
	    image_process_line(inbuf, 0);
	}
	fclose(fp);
    } /* User may not have a cache, so no error if not found */
    for (i=0; i<MAX_FACE_SETS; i++) {
	fd_cache[i].fd = -1;
	fd_cache[i].name[0] = '\0';
    }
}


void requestsmooth (int pnum){
    cs_print_string (csocket.fd, "asksmooth %d",pnum);
}

/******************************************************************************
 *
 * Code related to face caching.
 *
 *****************************************************************************/

char facecachedir[MAX_BUF];

void requestface(int pnum, char *facename)
{
    face_info.cache_misses++;
    facetoname[pnum] = strdup_local(facename);
    cs_print_string(csocket.fd, "askface %d", pnum);
}



/* This is common for all the face commands (face2, face1, face).
 * For face1 and face commands, faceset should always be zero.
 * for face commands, has_sum and checksum will be zero.
 * pnum is the face number, while face is the name.
 * We actually don't care what the set it - it could be useful right now,
 * but in the current caching scheme, we look through all the facesets for
 * the image and if the checksum matches, we assume we have match.
 * This approach makes sure that we don't have to store the same image multiple
 * times simply because the set number may be different.
 */
void finish_face_cmd(int pnum, uint32 checksum, int has_sum, char *face, int faceset)
{
    int len;
    uint32 nx,ny;
    uint8 data[65536], *png_tmp;
    char filename[1024];
    uint32 newsum=0;
    Cache_Entry *ce=NULL;

#if 0
    fprintf(stderr,"finish_face_cmd, pnum=%d, checksum=%d, face=%s\n",
	    pnum, checksum, face);
#endif
    /* In the case of gfx, we don't care about checksum.  For public and
     * private areas, if we care about checksum, and the image doesn't match,
     * we go onto the next step.  If nothing found, we request it
     * from the server.
     */
    sprintf(filename,"%s/.crossfire/gfx/%s.png", getenv("HOME"), face);
    if (load_image(filename, data, &len, &newsum)==-1) {
	ce=image_find_cache_entry(face, checksum, has_sum);
	if (!ce) {
	    /* Not in our cache, so request it from the server */
		requestface(pnum, face);
		return;
	}
	else if (ce->image_data) {
	    /* If this has image_data, then it has already been rendered */
	    if (!associate_cache_entry(ce, pnum)) return;
	}
	if (ce->ispublic)
	    sprintf(filename,"%s/%s",
		    DATADIR, ce->filename);
	else
	    sprintf(filename,"%s/.crossfire/crossfire-images/%s",
		    getenv("HOME"), ce->filename);
	if (load_image(filename, data, &len, &newsum)==-1) {
	    LOG(LOG_WARNING,"common::finish_face_cmd","file %s listed in cache file, but unable to load", filename);
	    requestface(pnum, face);
	    return;
	}
    }

    /* If we got here, we found an image and the checksum is OK. */

    if (!(png_tmp = png_to_data(data, len, &nx, &ny))) {
	/* If the data is bad, remove it if it is in the players private cache */
	LOG(LOG_WARNING,"common::finish_face_cmd","Got error on png_to_data, image=%s",face);
	if (ce) {
	    if (!ce->ispublic) unlink(filename);
	    image_remove_hash(face,ce);
	}

	requestface(pnum, face);
    }

    /* create_and_rescale_image_from data is an external reference to a piece in
     * the gui section of the code.
     */
    if (create_and_rescale_image_from_data(ce, pnum, png_tmp,nx, ny)) {
	LOG(LOG_WARNING,"common::finish_face_cmd","Got error on create_and_rescale_image_from_data, file=%s",filename);
	requestface(pnum, face);
    }
    free(png_tmp);
}


/* We can now connect to different servers, so we need to clear out
 * any old images.  We try to free the data also to prevent memory
 * leaks.
 * Note that we don't touch our hashed entries - so that when we
 * connect to a new server, we still have all that information.
 */

void reset_image_cache_data()
{
    int i;

    if (want_config[CONFIG_CACHE]) for (i=1; i<MAXPIXMAPNUM; i++) {
	free(facetoname[i]);
	facetoname[i]=NULL;
    }
}


/* We only get here if the server believes we are caching images. */
/* We rely on the fact that the server will only send a face command for
 * a particular number once - at current time, we have no way of knowing
 * if we have already received a face for a particular number.
 */

void FaceCmd(unsigned char *data,  int len)
{
    int pnum;
    char *face;

    /* A quick sanity check, since if client isn't caching, all the data
     * structures may not be initialized.
     */
    if (!use_config[CONFIG_CACHE]) {
	LOG(LOG_WARNING,"common::FaceCmd","Received a 'face' command when we are not caching");
	return;
    }
    pnum = GetShort_String(data);
    face = (char*)data+2;
    data[len] = '\0';

    finish_face_cmd(pnum, 0, 0, face,0);

}

void Face1Cmd(unsigned char *data,  int len)
{
    int pnum;
    uint32  checksum;
    char *face;

    /* A quick sanity check, since if client isn't caching, all the data
     * structures may not be initialized.
     */
    if (!use_config[CONFIG_CACHE]) {
	LOG(LOG_WARNING,"common::Face1Cmd","Received a 'face' command when we are not caching");
	return;
    }
    pnum = GetShort_String(data);
    checksum = GetInt_String(data+2);
    face = (char*)data+6;
    data[len] = '\0';

    finish_face_cmd(pnum, checksum, 1, face,0);
}

void Face2Cmd(uint8 *data,  int len)
{
    int pnum;
    uint8 setnum;
    uint32  checksum;
    char *face;

    /* A quick sanity check, since if client isn't caching, all the data
     * structures may not be initialized.
     */
    if (!use_config[CONFIG_CACHE]) {
	LOG(LOG_WARNING,"common::Face2Cmd","Received a 'face' command when we are not caching");
	return;
    }
    pnum = GetShort_String(data);
    setnum = data[2];
    checksum = GetInt_String(data+3);
    face = (char*)data+7;
    data[len] = '\0';

    finish_face_cmd(pnum, checksum, 1, face,setnum);
}

void ImageCmd(uint8 *data,  int len)
{  
    int pnum,plen;

    pnum = GetInt_String(data);
    plen = GetInt_String(data+4);
    if (len<8 || (len-8)!=plen) {
	LOG(LOG_WARNING,"common::ImageCmd","Lengths don't compare (%d,%d)",
		(len-8),plen);
	return;
    }
    display_newpng(pnum,data+8,plen,0);
}


void Image2Cmd(uint8 *data,  int len)
{  
    int pnum,plen;
    uint8 setnum;

    pnum = GetInt_String(data);
    setnum = data[4];
    plen = GetInt_String(data+5);
    if (len<9 || (len-9)!=plen) {
	LOG(LOG_WARNING,"common::Image2Cmd","Lengths don't compare (%d,%d)",
		(len-9),plen);
	return;
    }
    display_newpng(pnum,data+9,plen, setnum);
}

/* 
 * This function is called when the server has sent us the actual
 * png data for an image.  If caching, we need to write this
 * data to disk.
 */
void display_newpng(int face, uint8 *buf, int buflen, int setnum)
{
    char    filename[MAX_BUF], basename[MAX_BUF];
    uint8   *pngtmp;
    FILE *tmpfile;
    uint32 width, height, csum, i;
    Cache_Entry *ce=NULL;

    if (use_config[CONFIG_CACHE]) {
	if (facetoname[face]==NULL) {
	    LOG(LOG_WARNING,"common::display_newpng","Caching images, but name for %ld not set", face);
	}
	/* Make necessary leading directories */
	sprintf(filename, "%s/.crossfire/crossfire-images",getenv("HOME"));
	if (access(filename, R_OK | W_OK | X_OK)== -1) 
#ifdef WIN32
	    mkdir(filename);
#else
	    mkdir(filename, 0755);
#endif

	sprintf(filename, "%s/.crossfire/crossfire-images/%c%c", getenv("HOME"),
		 facetoname[face][0], facetoname[face][1]);
	if (access(filename, R_OK | W_OK | X_OK)== -1) 
#ifdef WIN32
	    mkdir(filename);
#else
	    mkdir(filename,0755);
#endif

	/* If setnum is valid, and we have faceset information for it,
	 * put that prefix in.  This will make it easier later on to 
	 * allow the client to switch image sets on the fly, as it can
	 * determine what set the image belongs to.
	 * We also append the number to it - there could be several versions
	 * of 'face.base.111.x' if different servers have different image
	 * values.
	 */
	if (setnum >=0 && setnum < MAX_FACE_SETS && face_info.facesets[setnum].prefix) 
	    sprintf(basename,"%s.%s", facetoname[face], face_info.facesets[setnum].prefix);
	else
	    strcpy(basename, facetoname[face]);

	/* Decrease it by one since it will immediately get increased
	 * in the loop below.
	 */
	setnum--;
	do {
	    setnum++;
	    sprintf(filename, "%s/.crossfire/crossfire-images/%c%c/%s.%d",
		    getenv("HOME"), facetoname[face][0],
		    facetoname[face][1], basename, setnum);
	} while (access(filename, F_OK)==-0);

#ifdef WIN32
	if ((tmpfile = fopen(filename,"wb"))==NULL) {
#else
	if ((tmpfile = fopen(filename,"w"))==NULL) {
#endif
	    LOG(LOG_WARNING,"common::display_newpng","Can not open %s for writing", filename);
	}
	else {
	    /* found a file we can write to */

	    fwrite(buf, buflen, 1, tmpfile);
	    fclose(tmpfile);
	    csum=0;
	    for (i=0; (int)i<buflen; i++) {
		ROTATE_RIGHT(csum);
		csum += buf[i];
		csum &= 0xffffffff;
	    }
	    sprintf(filename, "%c%c/%s.%d", facetoname[face][0], facetoname[face][1],
		    basename, setnum);
	    ce = image_add_hash(facetoname[face], filename,  csum, 0);

	    /* It may very well be more efficient to try to store these up
	     * and then write them as a bunch instead of constantly opening the
	     * file for appending.  OTOH, hopefully people will be using the
	     * built image archives, so only a few faces actually need to get
	     * downloaded.
	     */
	    sprintf(filename,"%s/.crossfire/crossfire-images/bmaps.client", getenv("HOME"));
	    if ((tmpfile=fopen(filename, "a"))==NULL) {
		LOG(LOG_WARNING,"common::display_newpng","Can not open %s for appending", filename);
	    }
	    else {
		fprintf(tmpfile, "%s %u %c%c/%s.%d\n",
			facetoname[face], csum, facetoname[face][0],
			facetoname[face][1], basename, setnum);
		fclose(tmpfile);
	    }
	}
    }

    pngtmp = png_to_data(buf, buflen, &width, &height);
    if(create_and_rescale_image_from_data(ce, face, pngtmp, width, height)) {
	LOG(LOG_WARNING, "common::display_newpng", "create_and_rescale_image_from_data failed for face %ld", face);
    }

    if (use_config[CONFIG_CACHE]) {
	free(facetoname[face]);
	facetoname[face]=NULL;
    }
    free(pngtmp);
}


/* get_image_info takes the data from a replyinfo image_info
 * and breaks it down.  The info contained is the checkums,
 * number of images, and faceset information.  It stores this
 * data into the face_info structure.
 * Since we know data is null terminated, we can use the strchr
 * operations with safety.
 * In each block, we find the newline - if we find one, we presume
 * the data is good, and update the face_info accordingly.
 * if we don't find a newline, we return.
 */
void get_image_info(uint8 *data, int len)
{
    char *cp, *lp, *cps[7], buf[MAX_BUF];
    int onset=0, badline=0,i;

    replyinfo_status |= RI_IMAGE_INFO;

    lp = (char*)data;
    cp = strchr(lp, '\n');
    if (!cp || (cp - lp) > len) return;
    face_info.num_images = atoi(lp);

    lp = cp+1;
    cp = strchr(lp, '\n');
    if (!cp || (cp - lp) > len) return;
    face_info.bmaps_checksum = strtoul(lp, NULL, 10);	/* need unsigned, so no atoi */

    lp = cp+1; 
    cp = strchr(lp, '\n');
    while (cp && (cp - lp) <= len) {
	*cp++ = '\0';

	/* The code below is pretty much the same as the code from the server
	 * which loads the original faceset file.
	 */
        if (!(cps[0] = strtok(lp, ":"))) badline=1;
        for (i=1; i<7; i++) {
            if (!(cps[i] = strtok(NULL, ":"))) badline=1;
	}
        if (badline) {
            LOG(LOG_WARNING,"common::get_image_info","bad data, ignoring line:/%s/", lp);
	} else {
            onset = atoi(cps[0]);
            if (onset >=MAX_FACE_SETS) {
		LOG(LOG_WARNING,"common::get_image_info","setnum is too high: %d > %d",
                    onset, MAX_FACE_SETS);
	    }
            face_info.facesets[onset].prefix = strdup_local(cps[1]);
            face_info.facesets[onset].fullname = strdup_local(cps[2]);
            face_info.facesets[onset].fallback = atoi(cps[3]);
            face_info.facesets[onset].size = strdup_local(cps[4]);
            face_info.facesets[onset].extension = strdup_local(cps[5]);
            face_info.facesets[onset].comment = strdup_local(cps[6]);
	}
	lp = cp;
	cp = strchr(lp, '\n');
    }
    face_info.have_faceset_info = 1;
    /* if the user has requested a specific face set and that set
     * is not numeric, try to find a matching set and send the
     * relevent setup command.
     */
    if (face_info.want_faceset && atoi(face_info.want_faceset)==0) {
	for (onset=0; onset<MAX_FACE_SETS; onset++) {
	    if (face_info.facesets[onset].prefix && 
		!strcasecmp(face_info.facesets[onset].prefix, face_info.want_faceset)) break;
	    if (face_info.facesets[onset].fullname && 
		!strcasecmp(face_info.facesets[onset].fullname, face_info.want_faceset)) break;
	}
	if (onset < MAX_FACE_SETS) { /* We found a match */
	    face_info.faceset = onset;
	    cs_print_string(csocket.fd, "setup faceset %d", onset);
	} else {
	    sprintf(buf,"Unable to find match for faceset %s on the server", face_info.want_faceset);
	    draw_info(buf, NDI_RED);
	}
    }

}

/* This gets a block of checksums from the server.  This lets it 
 * prebuild the images or what not.  It would probably be 
 * nice to add a gui callback someplace that gives a little status
 * display (18% done or whatever) - that probably needs to be done
 * further up.
 *
 * The start and stop values are not meaningful - they are here
 * because the semantics of the requestinfo/replyinfo is that
 * replyinfo includes the same request data as the requestinfo
 * (thus, if the request failed for some reason, the client would
 * know which one failed and then try again).  Currently, we
 * don't have any logic in the function below to deal with failures.
 */

void get_image_sums(char *data, int len)
{
    int start, stop, imagenum, slen, faceset;
    uint32  checksum;
    char *cp, *lp;

    cp = strchr((char*)data, ' ');
    if (!cp || (cp - data) > len) return;
    start = atoi((char*)data);

    while (isspace(*cp)) cp++;
    lp = cp;
    cp = strchr(lp, ' ');
    if (!cp || (cp - data) > len) return;
    stop = atoi(lp);

    replyinfo_last_face = stop;

    /* Can't use isspace here, because it matches with tab, ascii code 
     * 9 - this results in advancing too many spaces because
     * starting at image 2304, the MSB of the image number will be
     * 9.  Using a check against space will work until we get up to
     * 8192 images.
     */
    while (*cp==' ') cp++;
    while ((cp - data) < len) {
	imagenum = GetShort_String((uint8*)cp); cp += 2;
	checksum = GetInt_String((uint8*)cp); cp += 4;
	faceset = *cp; cp++;
	slen = *cp; cp++;
	/* Note that as is, this can break horribly if the client is missing a large number
	 * of images - that is because it will request a whole bunch which will overflow
	 * the servers output buffer, causing it to close the connection.
	 * What probably should be done is for the client to just request this checksum
	 * information in small batches so that even if the client has no local
	 * images, requesting the entire batch won't overflow the sockets buffer - this
	 * probably amounts to about 100 images at a time
	 */
	finish_face_cmd(imagenum, checksum, 1, (char*)cp, faceset);
	if (imagenum > stop) 
	    LOG(LOG_WARNING,"common::get_image_sums","Received an image beyond our range? %d > %d", imagenum, stop);
	cp += slen;
    }
}

