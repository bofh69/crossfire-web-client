/*
 * static char *rcsid_xio_c =
 *   "$Id$";
 *
 * This contains varous 'support' functions.  These functions will probably
 * go mostly unaltered between different toolkits, as long as X11 is still
 * used.  This file is not compiled seperately, rather it is included by
 * x11.c, so all statics will still work fine.
 */

#include <X11/keysym.h>
#include "def-keys.h"

static char *colorname[] = {
"Black",                /* 0  */
"White",                /* 1  */
"Navy",                 /* 2  */
"Red",                  /* 3  */
"Orange",               /* 4  */
"DodgerBlue",           /* 5  */
"DarkOrange2",          /* 6  */
"SeaGreen",             /* 7  */
"DarkSeaGreen",         /* 8  */        /* Used for window background color */
"Grey50",               /* 9  */
"Sienna",               /* 10 */
"Gold",                 /* 11 */
"Khaki"                 /* 12 */
};

struct {
    char    *name;
    uint32  checksum;
#ifdef GDK_XUTIL
    GdkPixmap  *pixmap;
    GdkBitmap	*mask;
    uint8    *png_data;
#else
    Pixmap  pixmap, mask;
#endif
} private_cache[MAXPIXMAPNUM];

int use_private_cache=0, last_face_num=0;


/******************************************************************************
 *
 * Code related to face caching.
 *
 *****************************************************************************/

char facecachedir[MAX_BUF];

typedef struct Keys {
    uint8	flags;
    sint8	direction;
    KeySym	keysym;
    char	*command;
    struct Keys	*next;
} Key_Entry;

/***********************************************************************
 *
 * Key board input translations are handled here.  We don't deal with
 * the events, but rather KeyCodes and KeySyms.
 *
 * It would be nice to deal with only KeySyms, but many keyboards
 * have keys that do not correspond to a KeySym, so we do need to
 * support KeyCodes.
 *
 ***********************************************************************/


static KeyCode firekey[2], runkey[2], commandkey, *bind_keycode, prevkey, nextkey,
    completekey;
static KeySym firekeysym[2], runkeysym[2], commandkeysym,*bind_keysym,
    prevkeysym, nextkeysym, completekeysym;
static int bind_flags=0;
static char bind_buf[MAX_BUF];

#define KEYF_NORMAL	0x01	/* Used in normal mode */
#define KEYF_FIRE	0x02	/* Used in fire mode */
#define KEYF_RUN	0x04	/* Used in run mode */
#define KEYF_MODIFIERS	0x07	/* Mask for actual keyboard modifiers, */
				/* not action modifiers */
#define KEYF_EDIT	0x08	/* Line editor */
#define KEYF_STANDARD	0x10	/* For standard (built in) key definitions */

extern char *directions[9];

/* Key codes can only be from 8-255 (at least according to
 * the X11 manual.  This is easier than using a hash
 * table, quicker, and doesn't use much more space.
 */

#define MAX_KEYCODE 255
static Key_Entry *keys[256];

/* This holds the name we recieve with the 'face' command so we know what
 * to save it as when we actually get the face.
 */
char *facetoname[MAXPIXMAPNUM];

/* Can be set when user is moving to new machine type */
int updatekeycodes=FALSE, keepcache=FALSE;

#ifndef GDK_XUTIL
/* Initializes the data for image caching */
static void init_cache_data()
{
    int i;

#include "pixmaps/question.111"

    /* Currently, we can cache in all face modes currently supported,
     * so I removed the code that did checks on that.
     */

    pixmaps[0].mask=None;
    pixmaps[0].bitmap=XCreateBitmapFromData(display, 
	RootWindow(display, screen_num), (const char*)question_bits, image_size,image_size);

    /* In xpm mode, XCopyArea is used from this data, so we need to copy
     * the image into an pixmap of appropriate depth.
     * Note that while are image created is the image size, since we know
     * that are filler image is currently only 24x24, we only copy that much
     * data.
     */
    pixmaps[0].pixmap=XCreatePixmap(display, win_root, image_size, image_size, 
	DefaultDepth(display,DefaultScreen(display)));
    XCopyPlane(display, pixmaps[0].bitmap, pixmaps[0].pixmap, gc_game,
	       0,0,24,24,0,0,1);
		
    pixmaps[0].bg = 0;
    pixmaps[0].fg = 1;
    facetoname[0]=NULL;

    /* Initialize all the images to be of the same value. */
    for (i=1; i<MAXPIXMAPNUM; i++)  {
	pixmaps[i]=pixmaps[0];
	facetoname[i]=NULL;
    }

#ifdef IMAGECACHEDIR
    strcpy(facecachedir, IMAGECACHEDIR);
#else
    sprintf(facecachedir,"%s/.crossfire/images", getenv("HOME"));
#endif

    if (make_path_to_dir(facecachedir)==-1) {
	    fprintf(stderr,"Could not create directory %s, exiting\n", facecachedir);
	    exit(1);
    }

}
#endif

static void requestface(int pnum, char *facename, char *facepath)
{
    char buf[MAX_BUF];

    facetoname[pnum] = strdup_local(facepath);
    sprintf(buf,"askface %d", pnum);
    cs_write_string(csocket.fd, buf, strlen(buf));
    /* Need to make sure we have the directory */
    sprintf(buf,"%s/%c%c", facecachedir, facename[0], facename[1]);
    if (access(buf,R_OK)) make_path_to_dir(buf);
}
/* Rotate right from bsd sum. */
#define ROTATE_RIGHT(c) if ((c) & 01) (c) = ((c) >>1) + 0x80000000; else (c) >>= 1;

/*#define CHECKSUM_DEBUG*/

/* This is common for both face1 and face commands. */
void finish_face_cmd(int pnum, uint32 checksum, int has_sum, char *face)
{
    char buf[MAX_BUF];
    int fd,len;
    char data[65536];
    uint32 newsum=0;
#ifndef GDK_XUTIL
    Pixmap pixmap, mask;
#endif

    /* Check private cache first */
    sprintf(buf,"%s/.crossfire/gfx/%s", getenv("HOME"), face);
    if (display_mode == Xpm_Display)
	strcat(buf,".xpm");
    else if (display_mode == Png_Display)
	strcat(buf,".png");

    if ((fd=open(buf, O_RDONLY))!=-1) {
	len=read(fd, data, 65535);
	close(fd);
	has_sum=0;  /* Maybe not really true, but we want to use this image
		     * and not request a replacement.
		     */
    } else {

	/* Hmm.  Should we use this file first, or look in our home
	 * dir cache first?
	 */
	if (use_private_cache) {
	    len = find_face_in_private_cache(face, checksum);
	    if ( len > 0 ) {
#ifdef GDK_XUTIL
		pixmaps[pnum].gdkpixmap = private_cache[len].pixmap;
		pixmaps[pnum].gdkmask = private_cache[len].mask;
		pixmaps[pnum].png_data = private_cache[len].png_data;

#else
		pixmaps[pnum].pixmap = private_cache[len].pixmap;
		pixmaps[pnum].mask = private_cache[len].mask;
#endif
		/* we may want to find a better match */
		if (private_cache[len].checksum == checksum ||
		    !has_sum || keepcache) return;
	    }
	}


	/* To prevent having a directory with 2000 images, we do a simple
	 * split on the first 2 characters.
	 */
	sprintf(buf,"%s/%c%c/%s", facecachedir, face[0], face[1],face);
	if (display_mode == Xpm_Display)
	    strcat(buf,".xpm");
	else if (display_mode == Png_Display)
	    strcat(buf,".png");

	if ((fd=open(buf, O_RDONLY))==-1) {
	    requestface(pnum, face, buf);
	    return;
	}
	len=read(fd, data, 65535);
	close(fd);
    }

    if (has_sum && !keepcache) {
	for (fd=0; fd<len; fd++) {
	    ROTATE_RIGHT(newsum);
	    newsum += data[fd];
	    newsum &= 0xffffffff;
	}

	if (newsum != checksum) {
#ifdef CHECKSUM_DEBUG
	    fprintf(stderr,"finish_face_command: checksums differ: %s, %x != %x\n",
		    face, newsum, checksum);
#endif
	    requestface(pnum, face, buf);
#ifdef CHECKSUM_DEBUG
	} else {
	    fprintf(stderr,"finish_face_command: checksums match: %s, %x == %x\n",
		    face, newsum, checksum);
#endif
	}
    }
#ifdef GDK_XUTIL
    if (display_mode == Xpm_Display) {
    
	GtkStyle *style;
    
	style = gtk_widget_get_style(gtkwin_root);
	pixmaps[pnum].gdkpixmap = gdk_pixmap_create_from_xpm_d(gtkwin_root->window,
					 &pixmaps[pnum].gdkmask,
					 &style->bg[GTK_STATE_NORMAL],
					     (gchar **) data );
	    if (!pixmaps[pnum].gdkpixmap) {
		requestface(pnum, face, buf);
	    }
    }
    else if (display_mode == Png_Display) {
#ifdef HAVE_LIBPNG
	if (pngximage && !(pixmaps[pnum].png_data = png_to_data(data, (int)len))) {
	    fprintf(stderr,"Got error on png_to_data, file=%s\n",buf);
	    requestface(pnum, face, buf);
	}
#ifdef HAVE_SDL
	else if (sdlimage) {
	    SDL_RWops *ops= SDL_RWFromMem( data, len);
	    surfaces[pnum].surface= IMG_LoadTyped_RW( ops, 1, (char*)"PNG");
	
	    /*
	     * Not sure if this is an error condition or not...
	     * below it makes a call to requestface but if he does it,
	     * we don't have to.
	     */
	    /* Just leaving this code from Scott in place.  Maybe it will be used
	     * at some point? - MSW
	     */
	    if( surfaces[pnum].surface == NULL)
	    {
	    }
	    else 
	    {
	    }
	}
#endif /* HAVE_SDL */
	/* even if using pngximage or SDL we still need standard image for the inventory list */
	if (png_to_gdkpixmap(gtkwin_root->window, data, len, &pixmaps[pnum].gdkpixmap, 
		 &pixmaps[pnum].gdkmask,gtk_widget_get_colormap(gtkwin_root))) {
	    fprintf(stderr,"Got error on png_to_gdkpixmap, file=%s\n",buf);
	    requestface(pnum, face, buf);
	}
#endif
    }
#else /* Not GDK_XUTIL */
    if (display_mode==Xpm_Display) {
	XpmAttributes xpm_attr;

	xpm_attr.colormap = colormap;
	xpm_attr.valuemask = XpmColormap;
	if (XpmCreatePixmapFromBuffer(display, win_game, data,
                &pixmap, &mask, &xpm_attr)) {
	    requestface(pnum, face, buf);
	} else {
	    pixmaps[pnum].pixmap = pixmap;
	    pixmaps[pnum].mask = mask;
	}
    } else if (display_mode==Png_Display) {
#ifdef HAVE_LIBPNG
	unsigned long w,h;

	/* Fail on this read, we will request a new copy */
	if (png_to_xpixmap(display, win_game, data, len,
			   &pixmap, &mask, &colormap, &w, &h)) {
	    requestface(pnum, face, buf);
	} else {
	    pixmaps[pnum].pixmap = pixmap;
	    pixmaps[pnum].mask = mask;
	}

#if defined(GDK_XUTIL) && defined(HAVE_SDL)
	if (sdlimage) {
	    fprintf( stdout, "Getting PNG face %d data from memory\n", pnum);
	    SDL_RWops *ops= SDL_RWFromMem( data, len);
	    surfaces[pnum].surface= IMG_LoadTyped_RW( ops, 1, (char*)"PNG");
	
	    /*
	     * Not sure if this is an error condition or not...
	     * below it makes a call to requestface but if he does it,
	     * we don't have to.
	     */
	    if( surfaces[pnum].surface == NULL)
	    {
	    }
	    else 
	    {
	    }
	}
    }
#endif /* GDK_XUTIL and HAVE_SDL*/

#endif /* HAVE_LIBPNG */
    } else if (display_mode==Pix_Display) {
	pixmaps[pnum].bitmap = XCreateBitmapFromData(display,
		RootWindow(display,DefaultScreen(display)),
		(char*)data,24,24);
	pixmaps[pnum].fg = (data[24] << 24) + (data[25] << 16) + (data[26] << 8) +
	    data[27];
	pixmaps[pnum].bg = (data[28] << 24) + (data[29] << 16 )+ (data[30] << 8 )+
	    data[31];
    }
#endif
}


#ifndef GDK_XUTIL

static int allocate_colors(Display *disp, Window w, long screen_num,
        Colormap *colormap, XColor discolor[16])
{
  int i, tried = 0, depth=0, iscolor;
  Status status;
  Visual *vis;
  XColor exactcolor;

  iscolor = 1;
  vis = DefaultVisual(disp,screen_num);
  if (vis->class >= StaticColor) {
    *colormap = DefaultColormap(disp,screen_num);
    depth = DefaultDepth(disp,screen_num);
  }
  else {
    *colormap = DefaultColormap(disp,screen_num);
    printf("Switching to black and white.\n");
    printf("You have a black and white terminal.\n");
    return 0;
  }
try_private:
  if (depth > 3 && iscolor) {
    unsigned long pixels[13];
    for (i=0; i<13; i++){
      status = XLookupColor(disp,*colormap, colorname[i],&exactcolor,
                            &discolor[i]);
      if (!status){
        printf("Can't find colour %s.\n", colorname[i]);
        printf("Switching to black and white.\n");
        iscolor = 0;
        break;
      }
      status = XAllocColor(disp,*colormap,&discolor[i]);
      if (!status) {
        if (!tried) {
          printf( "Not enough colours. Trying a private colourmap.\n");
          XFreeColors(disp, *colormap, pixels, i-1, 0);
          *colormap = XCreateColormap(disp, w, vis, AllocNone);
          XSetWindowColormap(disp, w, *colormap);
          tried = 1;
          goto try_private;
        } else {
          printf( "Failed. Switching to black and white.\n");
          iscolor = 0;
          break;
        }
      }
      pixels[i] = discolor[i].pixel;
    }
  }
  return iscolor;
}

#endif /* GDK_XUTIL */




/* Updates the keys array with the keybinding that is passed.  All the
 * arguments are pretty self explanatory.  flags is the various state
 * that the keyboard is in.
 * This function is common to both gdk and x11 client
 */
static void insert_key(KeySym keysym, KeyCode keycode, int flags, char *command)
{

    Key_Entry *newkey;
    int i, direction=-1;

    if (keycode>MAX_KEYCODE) {
	fprintf(stderr,"Warning insert_key:keycode that is passed is greater than 255.\n");
	keycode=0;	/* hopefully the rest of the data is OK */
    }
    if (keys[keycode]==NULL) {
	keys[keycode]=malloc(sizeof(Key_Entry));
	keys[keycode]->command=NULL;
	keys[keycode]->next=NULL;
    }
    newkey=keys[keycode];

    /* Try to find out if the command is a direction command.  If so, we
     * then want to keep track of this fact, so in fire or run mode,
     * things work correctly.
     */
    for (i=0; i<9; i++)
	if (!strcmp(command, directions[i])) {
		direction=i;
		break;
	}

    if (keys[keycode]->command!=NULL) {
	/* if keys[keycode]->command is not null, then newkey is
	 * the same as keys[keycode]->command.
	 */
	while (newkey->next!=NULL)
	    newkey = newkey->next;
	newkey->next = malloc(sizeof(Key_Entry));
	newkey = newkey->next;
	/* This is the only initializing we need to do - the other fields
	 * will get filled in by the passed parameters
	 */
	newkey->next = NULL;
    }
    newkey->keysym = keysym;
    newkey->flags = flags;
    newkey->command = strdup_local(command);
    newkey->direction = direction;
}


#ifdef GDK_XUTIL
#define display GDK_DISPLAY()
#endif

/* This function is common to both gdk and x11 client */

static void parse_keybind_line(char *buf, int line, int standard)
{
    char *cp, *cpnext;
    KeySym keysym;
    KeyCode keycode;
    int flags;

    if (buf[0]=='#' || buf[0]=='\n') return;
    if ((cpnext = strchr(buf,' '))==NULL) {
	fprintf(stderr,"Line %d (%s) corrupted in keybinding file.\n", line,buf);
	return;
    }
    /* Special keybinding line */
    if (buf[0] == '!') {
	char *cp1;
	while (*cpnext == ' ') ++cpnext;
	cp = strchr(cpnext, ' ');
	if (!cp) {
	    fprintf(stderr,"Line %d (%s) corrupted in keybinding file.\n", line,buf);
	    return;
	}
	*cp++ = 0;  /* Null terminate it */
	cp1 = strchr(cp, ' ');
	if (!cp1) {
	    fprintf(stderr,"Line %d (%s) corrupted in keybinding file.\n", line,buf);
	    return;
	}
	*cp1 ++ = 0;/* Null terminate it */
	keycode = atoi(cp1);
	keysym = XStringToKeysym(cp);
	/* As of now, all these keys must have keysyms */
	if (keysym == NoSymbol) {
	    fprintf(stderr,"Could not convert %s into keysym\n", cp);
	    return;
	}
	if (!strcmp(cpnext,"commandkey")) {
	    commandkeysym = keysym;
	    commandkey = keycode;
	    return;
	}
	if (!strcmp(cpnext,"firekey0")) {
	    firekeysym[0] = keysym;
	    firekey[0] = keycode;
	    return;
	}
	if (!strcmp(cpnext,"firekey1")) {
	    firekeysym[1] = keysym;
	    firekey[1] = keycode;
	    return;
	}
	if (!strcmp(cpnext,"runkey0")) {
	    runkeysym[0] = keysym;
	    runkey[0] = keycode;
	    return;
	}
	if (!strcmp(cpnext,"runkey1")) {
	    runkeysym[1] = keysym;
	    runkey[1] = keycode;
	    return;
	}
	if (!strcmp(cpnext,"completekey")) {
	    completekeysym = keysym;
	    completekey = keycode;
	    return;
	}
	if (!strcmp(cpnext,"nextkey")) {
	    nextkeysym = keysym;
	    nextkey = keycode;
	    return;
	}
	if (!strcmp(cpnext,"prevkey")) {
	    prevkeysym = keysym;
	    prevkey = keycode;
	    return;
	}
    }
    if (standard) standard=KEYF_STANDARD;
    else standard=0;

    *cpnext++ = '\0';
    keysym = XStringToKeysym(buf);
    cp = cpnext;
    if ((cpnext = strchr(cp,' '))==NULL) {
	fprintf(stderr,"Line %d (%s) corrupted in keybinding file.\n", line, cp);
	return;
    }
    *cpnext++ = '\0';

    /* If we can, convert the keysym into a keycode.  */
    keycode = atoi(cp);
    cp = cpnext;
    if ((cpnext = strchr(cp,' '))==NULL) {
	fprintf(stderr,"Line %d (%s) corrupted in keybinding file.\n", line, cp);
	return;
    }
    *cpnext++ = '\0';
    flags = 0;
    while (*cp!='\0') {
        switch (*cp) {
	case 'A':
		flags |= KEYF_NORMAL | KEYF_FIRE | KEYF_RUN;
		break;
	case 'N':
		flags |= KEYF_NORMAL;
		break;
	case 'F':
		flags |= KEYF_FIRE;
		break;
	case 'R':
		flags |= KEYF_RUN;
		break;
	case 'E':
		flags |= KEYF_EDIT;
		break;
	case 'S':
		flags |= KEYF_STANDARD;
		break;
	default:
	    fprintf(stderr,"Warning:  Unknown flag (%c) line %d in key binding file\n",
		*cp, line);
        }
        cp++;
    }

    /* This gets tricky - if we are reading the keycodes from the built
     * in defaults and keycodes are specified there, we want to honor them.
     */
    if ((keysym!=NoSymbol) &&
	(( (keycode == 1) && standard) || (flags&KEYF_STANDARD) || updatekeycodes)) {

        keycode = XKeysymToKeycode(display, keysym);

        /* It is possible that we get a keysym that we can not convert
         * into a keycode (such a case might be binding the key on
         * one system, and later trying to run on another system that
         * doesn't have that key.
         * While the client will not be able to use it this invocation,
         * it may be able to use it in the future.  As such, don't throw
         * it away, but at least print a warning message.
         */
        if (keycode==0) {
	    fprintf(stderr,"Warning: could not convert keysym %s into keycode, ignoring\n",
		buf);
	}
    }
    /* Rest of the line is the actual command.  Lets kill the newline */
    cpnext[strlen(cpnext)-1]='\0';
    insert_key(keysym, keycode, flags | standard, cpnext);
}

/* This code is common to both x11 and gdk client */
static void init_default_keybindings()
{
  char buf[MAX_BUF];
  int i;

  for(i=0;i< sizeof(def_keys)/sizeof(char *);i++) {
    strcpy(buf,def_keys[i]);
    parse_keybind_line(buf,i,1);
  }
}


/* This reads in the keybindings, and initializes any special values.
 * called by init_windows.
 */
/* This function is common to both x11 and gdk client */

static void init_keys()
{
    int i, line=0;
    FILE *fp;
    char buf[BIG_BUF];

    commandkeysym = XK_apostrophe;
    commandkey =XKeysymToKeycode(display,XK_apostrophe);
    if (!commandkey) {
      commandkeysym =XK_acute;
      commandkey =XKeysymToKeycode(display, XK_acute);
    }
    firekeysym[0] =XK_Shift_L;
    firekey[0] =XKeysymToKeycode(display, XK_Shift_L);
    firekeysym[1] =XK_Shift_R;
    firekey[1] =XKeysymToKeycode(display, XK_Shift_R);
    runkeysym[0]  =XK_Control_L;
    runkey[0]  =XKeysymToKeycode(display, XK_Control_L);
    runkeysym[1]  =XK_Control_R;
    runkey[1]  =XKeysymToKeycode(display, XK_Control_R);

    completekeysym = XK_Tab;
    completekey = XKeysymToKeycode(display, XK_Tab);
    /* Don't set these to anything by default.  At least on sun
     * keyboards, the keysym for up on both the keypad and arrow
     * keys is the same, so player needs to rebind this so we get proper
     * keycode.  Very unfriendly to log in and not be able to move north/south.
     */
    nextkeysym = NoSymbol;
    nextkey = 0;
    prevkeysym = NoSymbol;
    prevkey = 0;

    for (i=0; i<=MAX_KEYCODE; i++) {
	keys[i] = NULL;
    }

    /* We now try to load the keybindings.  First place to look is the
     * users home directory, "~/.crossfire/keys".  Using a directory
     * seems like a good idea, in the future, additional stuff may be
     * stored.
     *
     * The format is described in the def_keys file.  Note that this file
     * is the same as what it was in the server distribution.  To convert
     * bindings in character files to this format, all that needs to be done
     * is remove the 'key ' at the start of each line.
     *
     * We need at least one of these keybinding files to exist - this is
     * where the various commands are defined.  In theory, we actually
     * don't need to have any of these defined -- the player could just
     * bind everything.  Probably not a good idea, however.
     */

    sprintf(buf,"%s/.crossfire/keys", getenv("HOME"));
    if ((fp=fopen(buf,"r"))==NULL) {
	fprintf(stderr,"Could not open ~/.crossfire/keys, trying to load global bindings\n");
	if (client_libdir==NULL) {
	    init_default_keybindings();
	    return;
	}
	sprintf(buf,"%s/def_keys", client_libdir);
	if ((fp=fopen(buf,"r"))==NULL) {
	    init_default_keybindings();
	    return;
	}
    }
    while (fgets(buf, BIG_BUF, fp)) {
	line++;
	parse_keybind_line(buf,line,0);
    }
    fclose(fp);
}

/* The only things we actually care about is the run and fire keys.
 * Other key releases are not important.
 * If it is the release of a run or fire key, we tell the client
 * to stop firing or running.  In some cases, it is possible that we
 * actually are not running or firing, and in such cases, the server
 * will just ignore the command.
 */

/* This code is used by gdk and x11 client, but has
 *  a fair number of #ifdefs to get the right
 * behavioiur
 */
static void parse_key_release(KeyCode kc, KeySym ks) {

    /* Only send stop firing/running commands if we are in actual
     * play mode.  Something smart does need to be done when the character
     * enters a non play mode with fire or run mode already set, however.
     */

    if (kc==firekey[0] || ks==firekeysym[0] || 
	kc==firekey[1] || ks==firekeysym[1]) {
		cpl.fire_on=0;
#ifdef GDK_XUTIL
		clear_fire();
		gtk_label_set (GTK_LABEL(fire_label),"    ");
#else	
		stop_fire();
		draw_message_window(0);
#endif
	}
    else if (kc==runkey[0] || ks==runkeysym[0] ||
	kc==runkey[1] || ks==runkeysym[1]) {
		cpl.run_on=0;
		if (cpl.echo_bindings) draw_info("stop run",NDI_BLACK);
#ifdef GDK_XUTIL
		clear_run();
		gtk_label_set (GTK_LABEL(run_label),"   ");
#else
		stop_run();
		draw_message_window(0);
#endif
	}
    /* Firing is handled on server side.  However, to keep more like the
     * old version, if you release the direction key, you want the firing
     * to stop.  This should do that.
     */
    else if (cpl.fire_on) 
#ifdef GDK_XUTIL
	clear_fire();
#else
	stop_fire();
#endif
}

/* This parses a keypress.  It should only be called when in Playing
 * mode.
 */
static void parse_key(char key, KeyCode keycode, KeySym keysym)
{
    Key_Entry *keyentry, *first_match=NULL;
    int present_flags=0;
    char buf[MAX_BUF];

    if (keycode == commandkey && keysym==commandkeysym) {
#ifdef GDK_XUTIL
      if (split_windows) {
	gtk_widget_grab_focus (GTK_WIDGET(gtkwin_info));
	gtk_widget_grab_focus (GTK_WIDGET(entrytext));
      } else {
	gtk_widget_grab_focus (GTK_WIDGET(entrytext));
      }
      gtk_entry_set_visibility(GTK_ENTRY(entrytext), 1);

#else
	draw_prompt(">");
#endif
	cpl.input_state = Command_Mode;
	cpl.no_echo=FALSE;
	return;
    }
    if (keycode == firekey[0] || keysym==firekeysym[0] ||
	keycode == firekey[1] || keysym==firekeysym[1]) {
		cpl.fire_on=1;
#ifdef GDK_XUTIL
		gtk_label_set (GTK_LABEL(fire_label),"Fire");
#else
		draw_message_window(0);
#endif
		return;
	}
    if (keycode == runkey[0] || keysym==runkeysym[0] ||
	keycode==runkey[1] || keysym==runkeysym[1]) {
		cpl.run_on=1;
#ifdef GDK_XUTIL
		gtk_label_set (GTK_LABEL(run_label),"Run");
#else
		draw_message_window(0);
#endif
		return;
	}

    if (cpl.run_on) present_flags |= KEYF_RUN;
    if (cpl.fire_on) present_flags |= KEYF_FIRE;
    if (present_flags ==0) present_flags = KEYF_NORMAL;

    keyentry = keys[keycode];
    while (keyentry!=NULL) {
	if ((keyentry->keysym!=NoSymbol && keyentry->keysym!=keysym) ||
	    (!(keyentry->flags & present_flags))) {
		keyentry=keyentry->next;
		continue;
	    }
	first_match = keyentry;
	/* Try to find a prefect match */
	if ((keyentry->flags & KEYF_MODIFIERS)!= present_flags) {
		keyentry=keyentry->next;
		continue;
	}
	else break;
    }
    if (first_match!=NULL) {
	char buf[MAX_BUF];

	if (first_match->flags & KEYF_EDIT) {
	    strcpy(cpl.input_text, first_match->command);
	    cpl.input_state = Command_Mode;
#ifdef GDK_XUTIL
	    sprintf(buf,"%s", cpl.input_text);
   	    gtk_entry_set_text(GTK_ENTRY(entrytext),buf);
	    gtk_widget_grab_focus (GTK_WIDGET(entrytext));
#else
	    sprintf(buf,">%s", cpl.input_text);
	    draw_prompt(buf);
#endif
	    return;
	}

	if (first_match->direction>=0) {
	    if (cpl.fire_on) {
		sprintf(buf,"fire %s", first_match->command);
		fire_dir(first_match->direction);
	    }
	    else if (cpl.run_on) {
		run_dir(first_match->direction);
		sprintf(buf,"run %s", first_match->command);
	    }
	    else {
		strcpy(buf,first_match->command);
		extended_command(first_match->command);
	    }
	    if (cpl.echo_bindings) draw_info(buf,NDI_BLACK);
	}
        else {
	    if (cpl.echo_bindings) draw_info(first_match->command,NDI_BLACK);
	    extended_command(first_match->command);
	}
	return;
    }
    if (key>='0' && key<='9') {
	cpl.count = cpl.count*10 + (key-'0');
	if (cpl.count>100000) cpl.count%=100000;
#ifdef GDK_XUTIL
        gtk_spin_button_set_value (GTK_SPIN_BUTTON(counttext), (float) cpl.count );
#endif
	return;
    }
    sprintf(buf, "Key unused (%s%s%s)",
          (cpl.fire_on? "Fire&": ""),
          (cpl.run_on ? "Run&" : ""),
          keysym==NoSymbol? "unknown": XKeysymToString(keysym));
    draw_info(buf,NDI_BLACK);
    cpl.count=0;
}


/* This returns a character string desribing the key. */
/* If save_mode is true, it means that the format used for saving
 * the information is used, instead of the usual format for displaying
 * the information in a friendly manner.
 */
static char * get_key_info(Key_Entry *key, KeyCode kc, int save_mode)
{
    static char buf[MAX_BUF];
    char buff[MAX_BUF];
    int bi=0;

    if ((key->flags & KEYF_MODIFIERS) == KEYF_MODIFIERS)
	buff[bi++] ='A';
    else {
	if (key->flags & KEYF_NORMAL)
	  buff[bi++] ='N';
	if (key->flags & KEYF_FIRE)
	  buff[bi++] ='F';
	if (key->flags & KEYF_RUN)
	  buff[bi++] ='R';
    }
    if (key->flags & KEYF_EDIT)
	buff[bi++] ='E';
    if (key->flags & KEYF_STANDARD)
	buff[bi++] ='S';

    buff[bi]='\0';
    if (save_mode) {
	if(key->keysym == NoSymbol) {
	  sprintf(buf, "(null) %i %s %s",
		kc,buff, key->command);
	}
	else {
	  sprintf(buf, "%s %i %s %s",
		    XKeysymToString(key->keysym), kc,
		    buff, key->command);
	}
    }
    else {
	if(key->keysym == NoSymbol) {
	  sprintf(buf, "key (null) (%i) %s %s",
		kc,buff, key->command);
	}
	else {
	  sprintf(buf, "key %s (%i) %s %s",
		    XKeysymToString(key->keysym), kc,
		    buff, key->command);
	}
    }
    return buf;
}

/* Shows all the keybindings.  allbindings me we also show the standard
 * (default) keybindings.
 */

static void show_keys(int allbindings)
{
  int i, count=1;
  Key_Entry *key;
  char buf[MAX_BUF];

  sprintf(buf, "Commandkey %s (%d)", 
	  commandkeysym==NoSymbol?"unknown":XKeysymToString(commandkeysym),
	  commandkey);
  draw_info(buf,NDI_BLACK);
  sprintf(buf, "Firekeys 1: %s (%d), 2: %s (%d)",
	  firekeysym[0]==NoSymbol?"unknown":XKeysymToString(firekeysym[0]), firekey[0],
	  firekeysym[1]==NoSymbol?"unknown":XKeysymToString(firekeysym[1]), firekey[1]);
  draw_info(buf,NDI_BLACK);
  sprintf(buf, "Runkeys 1: %s (%d), 2: %s (%d)",
	  runkeysym[0]==NoSymbol?"unknown":XKeysymToString(runkeysym[0]), runkey[0],
	  runkeysym[1]==NoSymbol?"unknown":XKeysymToString(runkeysym[1]), runkey[1]);
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Command Completion Key %s (%d)", 
	  completekeysym==NoSymbol?"unknown":XKeysymToString(completekeysym),
	  completekey);
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Next Command in History Key %s (%d)", 
	  nextkeysym==NoSymbol?"unknown":XKeysymToString(nextkeysym),
	  nextkey);
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Previous Command in History Key %s (%d)", 
	  prevkeysym==NoSymbol?"unknown":XKeysymToString(prevkeysym),
	  prevkey);
  draw_info(buf,NDI_BLACK);


  /* Perhaps we should start at 8, so that we only show 'active'
   * keybindings?
   */
  for (i=0; i<=MAX_KEYCODE; i++) {
    for (key=keys[i]; key!=NULL; key =key->next) {
	if (key->flags & KEYF_STANDARD && !allbindings) continue;

	sprintf(buf,"%3d %s",count,  get_key_info(key,i,0));
	draw_info(buf,NDI_BLACK);
	count++;
    }
  }
}




void bind_key(char *params)
{
  char buf[MAX_BUF];

  if (!params) {
    draw_info("Usage: bind [-nfre] {<commandline>/commandkey/firekey{1/2}/runkey{1/2}/",NDI_BLACK);
    draw_info("           completekey/nextkey/prevkey}",NDI_BLACK);
    return;
  }

  /* Skip over any spaces we may have */
  while (*params==' ') params++;

  if (!strcmp(params, "commandkey")) {
    bind_keycode = &commandkey;
    bind_keysym = &commandkeysym;
    draw_info("Push key to bind new commandkey.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }
  if (!strcmp(params, "firekey1")) {
    bind_keycode = &firekey[0];
    bind_keysym = & firekeysym[0];
    draw_info("Push key to bind new firekey 1.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }
  if (!strcmp(params, "firekey2")) {
    bind_keycode = &firekey[1];
    bind_keysym = & firekeysym[1];
    draw_info("Push key to bind new firekey 2.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }
  if (!strcmp(params, "runkey1")) {
    bind_keycode = &runkey[0];
    bind_keysym = &runkeysym[0];
    draw_info("Push key to bind new runkey 1.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }
  if (!strcmp(params, "runkey2")) {
    bind_keycode = &runkey[1];
    bind_keysym = &runkeysym[1];
    draw_info("Push key to bind new runkey 2.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }

  if (!strcmp(params, "completekey")) {
    bind_keycode = &completekey;
    bind_keysym = &completekeysym;
    draw_info("Push key to bind new command completeion key",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }

  if (!strcmp(params, "prevkey")) {
    bind_keycode = &prevkey;
    bind_keysym = &prevkeysym;
    draw_info("Push key to bind new previous command in history key.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }

  if (!strcmp(params, "nextkey")) {
    bind_keycode = &nextkey;
    bind_keysym = &nextkeysym;
    draw_info("Push key to bind new next command in history key.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }


  if (params[0] != '-')
    bind_flags =KEYF_MODIFIERS;
  else {
    bind_flags =0;
    bind_keysym=NULL;
    bind_keycode=NULL;
    for (params++; *params != ' '; params++)
      switch (*params) {
      case 'n':
	bind_flags |= KEYF_NORMAL;
	break;
      case 'f':
	bind_flags |= KEYF_FIRE;
	break;
      case 'r':
	bind_flags |= KEYF_RUN;
	break;
      case 'e':
	bind_flags |= KEYF_EDIT;
	break;
      case '\0':
	draw_info("Try unbind to remove bindings..",NDI_BLACK);
	return;
      default:
	sprintf(buf, "Unknown flag to bind: '%c'", *params);
	draw_info(buf,NDI_BLACK);
	return;
      }
    params++;
  }

  if (!(bind_flags & KEYF_MODIFIERS))
    bind_flags |= KEYF_MODIFIERS;

  if (!params[0]) {
    draw_info("Try unbind to remove bindings..",NDI_BLACK);
    return;
  }

  sprintf(buf, "Push key to bind '%s'.", params);
  draw_info(buf,NDI_BLACK);
  strcpy(bind_buf, params);
  bind_keycode=NULL;
  cpl.input_state = Configure_Keys;
  return;
}


/* This is a recursive function that saves all the entries for a particular
 * entry.  We save the first element first, and then go through
 * and save the rest of the elements.  In this way, the ordering of the key
 * entries in the
 * file remains the same.
 */

static void save_individual_key(FILE *fp, Key_Entry *key, KeyCode kc)
{
    if (key==NULL) return;
    fprintf(fp, "%s\n", get_key_info(key, kc, 1));
    save_individual_key(fp, key->next, kc);
}

static void save_keys()
{
    char buf[MAX_BUF], buf2[MAX_BUF];
    int i;
    FILE *fp;

    sprintf(buf,"%s/.crossfire/keys", getenv("HOME"));
    if (make_path_to_file(buf)==-1) {
	fprintf(stderr,"Could not create %s\n", buf);
	return;
    }
    if ((fp=fopen(buf,"w"))==NULL) {
	sprintf(buf2,"Could not open %s, key bindings not saved\n", buf);
	draw_info(buf2,NDI_BLACK);
	return;
    }
    if (commandkeysym != XK_apostrophe && commandkeysym != NoSymbol) {
	fprintf(fp, "! commandkey %s %d\n",
		XKeysymToString(commandkeysym), commandkey);
    }
    if (firekeysym[0] != XK_Shift_L && firekeysym[0] != NoSymbol) {
	fprintf(fp, "! firekey0 %s %d\n",
		XKeysymToString(firekeysym[0]), firekey[0]);
    }
    if (firekeysym[1] != XK_Shift_R && firekeysym[1] != NoSymbol) {
	fprintf(fp, "! firekey1 %s %d\n",
		XKeysymToString(firekeysym[1]), firekey[1]);
    }
    if (runkeysym[0] != XK_Control_L && runkeysym[0] != NoSymbol) {
	fprintf(fp, "! runkey0 %s %d\n",
		XKeysymToString(runkeysym[0]), runkey[0]);
    }
    if (runkeysym[1] != XK_Control_R && runkeysym[1] != NoSymbol) {
	fprintf(fp, "! runkey1 %s %d\n",
		XKeysymToString(runkeysym[1]), runkey[1]);
    }
    if (completekeysym != XK_Tab && completekeysym != NoSymbol) {
	fprintf(fp, "! completekey %s %d\n",
		XKeysymToString(completekeysym), completekey);
    }
    /* No defaults for these, so if it is set to anything, assume its valid */
    if (nextkeysym != NoSymbol) {
	fprintf(fp, "! nextkey %s %d\n",
		XKeysymToString(nextkeysym), nextkey);
    }
    if (prevkeysym != NoSymbol) {
	fprintf(fp, "! prevkey %s %d\n",
		XKeysymToString(prevkeysym), prevkey);
    }

    for (i=0; i<=MAX_KEYCODE; i++) {
	save_individual_key(fp, keys[i], i);
    }
    fclose(fp);
    /* Should probably check return value on all writes to be sure, but... */
    draw_info("key bindings successfully saved.",NDI_BLACK);
}

static void configure_keys(KeyCode k, KeySym keysym)
{
  char buf[MAX_BUF];

  if (bind_keycode==NULL) {
    if(k == firekey[0] || k == firekey[1]) {
	cpl.fire_on =1;
	draw_message_window(0);
	return;
    }
    if(k == runkey[0] || k == runkey[1]) {
	cpl.run_on =1;
	draw_message_window(0);
	return;
    }
  }
  cpl.input_state = Playing;
  /* Try to be clever - take into account shift/control keys being
   * held down when binding keys - in this way, player does not have to use
   * -f and -r flags to bind for many simple binds.
   */
	
  if ((cpl.fire_on || cpl.run_on) && (bind_flags & KEYF_MODIFIERS)==KEYF_MODIFIERS) {
	bind_flags &= ~KEYF_MODIFIERS;
	if (cpl.fire_on) bind_flags |= KEYF_FIRE;
	if (cpl.run_on) bind_flags |= KEYF_RUN;
  }

  if (bind_keycode!=NULL) {
	*bind_keycode = k;
	*bind_keysym=keysym;
  }
  else {
	insert_key(keysym, k, bind_flags, bind_buf);
  }

  sprintf(buf, "Binded to key '%s' (%i)", 
	  keysym==NoSymbol?"unknown":XKeysymToString(keysym), (int)k);
  draw_info(buf,NDI_BLACK);
  cpl.fire_on=0;
  cpl.run_on=0;
  draw_message_window(0);

  /* Do this each time a new key is bound.  This way, we are never actually
   * storing any information that needs to be saved when the connection
   * dies or the player quits.
   */
  save_keys();
  return;
}

static void unbind_usage()
{
    draw_info("Usage: unbind <entry_number> or",NDI_BLACK);
    draw_info("Usage: unbind [-a] [-g] to show existing bindings", NDI_BLACK);
    draw_info("    -a shows all (global) bindings", NDI_BLACK);
    draw_info("    -g unbinds a global binding", NDI_BLACK);
}

void unbind_key(char *params)
{
    int count=0, keyentry, onkey,global=0;
    Key_Entry *key, *tmp;
    char buf[MAX_BUF];

    if (params==NULL || params[0]=='\0') {
	show_keys(0);
	return;
    }

    /* Skip over any spaces we may have */
    while (*params==' ') params++;

    if (!strcmp(params,"-a")) {
	show_keys(1);
	return;
    }
    if (!strncmp(params,"-g",2)) {
	global=1;
	if (!(params=strchr(params,' ')))  {
	    unbind_usage();
	    return;
	}
    }
    if ((keyentry=atoi(params))==0) {
	unbind_usage();
	return;
    }

    for (onkey=0; onkey<=MAX_KEYCODE; onkey++) {
	for (key=keys[onkey]; key; key =key->next) {
	    if (global || !(key->flags&KEYF_STANDARD)) count++;
	    /* We found the key we want to unbind */
	    if (keyentry==count) {

		/* If it is the first entry, it is easy */
		if (key == keys[onkey]) {
		    keys[onkey] = key->next;
		    goto unbinded;
		}
		/* Otherwise, we need to figure out where in the link list
		 * the entry is.
		 */
		for (tmp=keys[onkey]; tmp->next!=NULL; tmp=tmp->next) {
		    if (tmp->next == key) {
			tmp->next =key->next;
			goto unbinded;
		    }
		}
		fprintf(stderr,"unbind_key - found number entry, but could not find actual key\n");
	    }
	}
    }
    /* Makes things look better to draw the blank line */
    draw_info("",NDI_BLACK);
    draw_info("No such entry. Try 'unbind' with no options to find entry.",NDI_BLACK);
    return;

    /*
     * Found. Now remove it.
     */

unbinded:

    sprintf(buf,"Removed binding: %3d %s", count, get_key_info(key,onkey,0));


    draw_info(buf,NDI_BLACK);
    if (key->command) free(key->command);
    free(key);
    save_keys();
}

#ifndef GDK_XUTIL

/* Gets a specified windows coordinates.  This function is pretty much
 * an exact copy out of the server.
 */
 
static void get_window_coord(Window win,
                 int *x,int *y,
                 int *wx,int *wy,
                 unsigned int *w,unsigned int *h)
{
  Window root,child;
  unsigned int tmp;

  XGetGeometry(display,win,&root,x,y,w,h,&tmp,&tmp);
  XTranslateCoordinates(display,win,root,0,0,wx,wy, &child);
}



void save_winpos()
{
    char savename[MAX_BUF],buf[MAX_BUF];
    FILE    *fp;
    int	    x,y,wx,wy;
    unsigned int w,h;

    if (!split_windows) {
	draw_info("You can only save window positions in split windows mode", NDI_BLUE);
	return;
    }
    sprintf(savename,"%s/.crossfire/winpos", getenv("HOME"));
    if (!(fp=fopen(savename,"w"))) {
	sprintf(buf,"Unable to open %s, window positions not saved",savename);
	draw_info(buf,NDI_BLUE);
	return;
    }
    /* This is a bit simpler than what the server was doing - it has
     * some code to handle goofy window managers which I am not sure
     * is still needed.
     */
    get_window_coord(win_game, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_game: %d %d %d %d\n", wx,wy, w, h);
    get_window_coord(win_stats, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_stats: %d %d %d %d\n", wx,wy, w, h);
    get_window_coord(infodata.win_info, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_info: %d %d %d %d\n", wx,wy, w, h);
    get_window_coord(inv_list.win, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_inv: %d %d %d %d\n", wx,wy, w, h);
    get_window_coord(look_list.win, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_look: %d %d %d %d\n", wx,wy, w, h);
    get_window_coord(win_message, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_message: %d %d %d %d\n", wx,wy, w, h);
    fclose(fp);
    sprintf(buf,"Window positions saved to %s",savename);
    draw_info(buf,NDI_BLUE);
}

/* Reads in the winpos file created by the above function and sets the
 * the window positions appropriately.
 */
void set_window_pos()
{
    unsigned int xwc_mask = CWX|CWY|CWWidth|CWHeight;
    XWindowChanges xwc;
    char buf[MAX_BUF],*cp;
    FILE *fp;

    if (!split_windows) return;

    sprintf(buf,"%s/.crossfire/winpos", getenv("HOME"));
    if (!(fp=fopen(buf,"r"))) return;

    while(fgets(buf,MAX_BUF-1, fp)!=NULL) {
	buf[MAX_BUF-1]='\0';
	if (!(cp=strchr(buf,' '))) continue;
	*cp++='\0';
	if (sscanf(cp,"%d %d %d %d",&xwc.x,&xwc.y,&xwc.width,&xwc.height)!=4)
	    continue;
	if (!strcmp(buf,"win_game:")) 
	    XConfigureWindow(display,win_game,xwc_mask, &xwc);
	if (!strcmp(buf,"win_stats:")) 
	    XConfigureWindow(display,win_stats,xwc_mask, &xwc);
	if (!strcmp(buf,"win_info:")) 
	    XConfigureWindow(display,infodata.win_info,xwc_mask, &xwc);
	if (!strcmp(buf,"win_inv:")) 
	    XConfigureWindow(display,inv_list.win,xwc_mask, &xwc);
	if (!strcmp(buf,"win_look:")) 
	    XConfigureWindow(display,look_list.win,xwc_mask, &xwc);
	if (!strcmp(buf,"win_message:")) 
	    XConfigureWindow(display,win_message,xwc_mask, &xwc);

    }
}

#endif

void load_defaults()
{
    char path[MAX_BUF],inbuf[MAX_BUF],*cp;
    FILE *fp;

#ifdef GDK_XUTIL
    sprintf(path,"%s/.crossfire/gdefaults", getenv("HOME"));
#else
    sprintf(path,"%s/.crossfire/defaults", getenv("HOME"));
#endif
    if ((fp=fopen(path,"r"))==NULL) return;
    while (fgets(inbuf, MAX_BUF-1, fp)) {
	inbuf[MAX_BUF-1]='\0';
	inbuf[strlen(inbuf)-1]='\0';	/* kill newline */

	if (inbuf[0]=='#') continue;
	/* IF no colon, then we certainly don't have a real value, so just skip */
	if (!(cp=strchr(inbuf,':'))) continue;
	*cp='\0';
	cp+=2;	    /* colon, space, then value */

	if (!strcmp(inbuf, "port")) {
	    port_num = atoi(cp);
	    continue;
	}
	if (!strcmp(inbuf, "server")) {
	    server = strdup_local(cp);	/* memory leak ! */
	    continue;
	}
	if (!strcmp(inbuf,"display")) {
	    if (!strcmp(cp,"xpm")) 
		display_mode=Xpm_Display;
	    if (!strcmp(cp,"png")) 
		display_mode=Png_Display;
#ifndef GDK_XUTIL	/* Gdk doesn't support pixmap */
	    else if (!strcmp(cp,"pixmap"))
		display_mode = Pix_Display;
#endif
	    else fprintf(stderr,"Unknown display specication in %s, %s",
			   path, cp);
	    continue;
	}
	if (!strcmp(inbuf,"cacheimages")) {
	    if (!strcmp(cp,"True")) cache_images=TRUE;
	    else cache_images=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"split")) {
	    if (!strcmp(cp,"True")) split_windows=TRUE;
	    else split_windows=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"showicon")) {
	    if (!strcmp(cp,"True")) inv_list.show_icon=TRUE;
	    else inv_list.show_icon=FALSE;
	    continue;
	}
#ifndef GDK_XUTIL
	if (!strcmp(inbuf,"scrolllines")) {
	    infodata.maxlines = atoi(cp);
	    continue;
	}
	if (!strcmp(inbuf,"scrollinfo")) {
	    if (!strcmp(cp,"True")) infodata.scroll_info_window=TRUE;
	    else infodata.scroll_info_window=FALSE;
	    continue;
	}
#endif
	if (!strcmp(inbuf,"sound")) {
	    if (!strcmp(cp,"True")) nosound=FALSE;
	    else nosound=TRUE;
	    continue;
	}
	if (!strcmp(inbuf,"command_window")) {
	    cpl.command_window = atoi(cp);
	    if (cpl.command_window<1 || cpl.command_window>127)
		cpl.command_window=COMMAND_WINDOW;
	    continue;
	}
	if (!strcmp(inbuf,"foodbeep")) {
	    if (!strcmp(cp,"True")) cpl.food_beep=TRUE;
	    else cpl.food_beep=FALSE;
	    continue;
	}
#ifdef GDK_XUTIL
	if (!strcmp(inbuf,"colorinv")) {
	    if (!strcmp(cp,"True")) color_inv=TRUE;
	    else color_inv=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"colortext")) {
	    if (!strcmp(cp,"True")) color_text=TRUE;
	    else color_text=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"tooltips")) {
	  if (!strcmp(cp,"True")) tool_tips=TRUE;
	  else tool_tips=FALSE;
	  continue;
	}  
	if (!strcmp(inbuf,"splitinfo")) {
	  if (!strcmp(cp,"True")) splitinfo=TRUE;
	  else splitinfo=FALSE;
	  continue;
	}  
	if (!strcmp(inbuf,"nopopups")) {
	  if (!strcmp(cp,"True")) nopopups=TRUE;
	  else nopopups=FALSE;
	  continue;
	}  
	/* Only SDL actually uses these values, but we can still preserve
	 * them even if they are not being used.
	 */
	if( !strcmp( inbuf,"Lighting")) {
	  if( !strcmp( cp, "per_pixel")) {
	    per_pixel_lighting= 1;
	    per_tile_lighting= 0;
	  } else if( !strcmp( cp, "per_tile")) {
	    per_pixel_lighting= 0;
	    per_tile_lighting= 1;
	  }
	  continue;
	}
	if( !strcmp( inbuf,"show_grid")) {
	  if( !strcmp( cp, "True")) show_grid = TRUE;
	  else show_grid = FALSE;
	  continue;
	}
#endif	/* GDK_XUTIL */
	fprintf(stderr,"Got line we did not understand: %s: %s\n", inbuf, cp);
    }
    fclose(fp);
}

void save_defaults()
{
    char path[MAX_BUF],buf[MAX_BUF];
    FILE *fp;

#ifdef GDK_XUTIL
    sprintf(path,"%s/.crossfire/gdefaults", getenv("HOME"));
#else
    sprintf(path,"%s/.crossfire/defaults", getenv("HOME"));
#endif
    if (make_path_to_file(path)==-1) {
	fprintf(stderr,"Could not create %s\n", path);
	return;
    }
    if ((fp=fopen(path,"w"))==NULL) {
	fprintf(stderr,"Could not open %s\n", path);
	return;
    }
    fprintf(fp,"# This file is generated automatically by cfclient.\n");
    fprintf(fp,"# Manually editing is allowed, however cfclient may be a bit finicky about\n");
    fprintf(fp,"# some of the matching it does.  all comparissons are case sensitive.\n");
    fprintf(fp,"# 'True' and 'False' are the proper cases for those two values");

    fprintf(fp,"port: %d\n", port_num);
    fprintf(fp,"server: %s\n", server);
    if (display_mode==Xpm_Display) {
	fprintf(fp,"display: xpm\n");
    } else if (display_mode==Pix_Display) {
	fprintf(fp,"display: pixmap\n");
    } else if (display_mode==Png_Display) {
	fprintf(fp,"display: png\n");
    }
    fprintf(fp,"cacheimages: %s\n", cache_images?"True":"False");
    fprintf(fp,"split: %s\n", split_windows?"True":"False");
    fprintf(fp,"showicon: %s\n", inv_list.show_icon?"True":"False");
#ifndef GDK_XUTIL
    fprintf(fp,"scrolllines: %d\n", infodata.maxlines);
    fprintf(fp,"scrollinfo: %s\n", infodata.scroll_info_window?"True":"False");
#endif
    fprintf(fp,"sound: %s\n", nosound?"False":"True");
    fprintf(fp,"command_window: %d\n", cpl.command_window);
    fprintf(fp,"foodbeep: %s\n", cpl.food_beep?"True":"False");
#ifdef GDK_XUTIL
    fprintf(fp,"colorinv: %s\n", color_inv?"True":"False");
    fprintf(fp,"colortext: %s\n", color_text?"True":"False");
    fprintf(fp,"tooltips: %s\n", color_text?"True":"False");
    fprintf(fp,"splitinfo: %s\n", splitinfo?"True":"False");
    fprintf(fp,"nopopups: %s\n", nopopups?"True":"False");
    if( per_pixel_lighting)
      fprintf( fp, "Lighting: per_pixel\n");
    else
      fprintf( fp, "Lighting: per_tile\n");
    fprintf( fp,"show_grid: %s\n", show_grid?"True":"False");
#endif /* GDK_XUTIL */

    fclose(fp);
    sprintf(buf,"Defaults saved to %s",path);
    draw_info(buf,NDI_BLUE);
}

#ifndef GDK_XUTIL
/* determine what we show in the inventory window.  This is a slightly
 * more complicated version than the server side, since we use a bitmask
 * which means we could show things like magical and cursed, or unpaid
 * and magical, etc.  Current time, we don't really support setting it
 * all that well.
 *
 */

void command_show (char *params)
{
    if(!params) {
	if (inv_list.show_what==show_all) inv_list.show_what = show_applied;
	else { /* rotate the bit.  If no valid bits are set, start over */
	    inv_list.show_what = inv_list.show_what << 1;
	    if (!(inv_list.show_what & show_mask))
		inv_list.show_what = show_all;
	}
	inv_list.env->inv_updated =1;
	return;
    }

    if (!strncmp(params, "all", strlen(params)))
        inv_list.show_what = show_all;
    else if (!strncmp(params, "applied", strlen(params)))
        inv_list.show_what = show_applied;
    else if (!strncmp(params, "unapplied", strlen(params)))
        inv_list.show_what = show_unapplied;
    else if (!strncmp(params, "unpaid", strlen(params)))
        inv_list.show_what = show_unpaid;
    else if (!strncmp(params, "cursed", strlen(params)))
        inv_list.show_what = show_cursed;
    else if (!strncmp(params, "magical", strlen(params)))
        inv_list.show_what = show_magical;
    else if (!strncmp(params, "nonmagical", strlen(params)))
        inv_list.show_what = show_nonmagical;
    else if (!strncmp(params, "locked", strlen(params)))
        inv_list.show_what = show_locked;
    else if (!strncmp(params, "unlocked", strlen(params)))
        inv_list.show_what = show_unlocked;

    inv_list.env->inv_updated =1;
}
#endif

/* This code is somewhat from the crossedit/xutil.c.
 * What we do is create a private copy of all the images
 * for ourselves.  Then, if we get a request to display
 * a new image, we see if we have it in this cache.
 *
 * This is only supported for PNG images.  I see now reason
 * to support the older image formats since they will be 
 * going away.
 */

int ReadImages() {

    int		len,i,num ;
    FILE	*infile;
    char	*cp, databuf[10000], *last_cp=NULL;
    unsigned long  x;
#ifndef GDK_XUTIL
    unsigned long y;
#endif

    if ((display_mode != Png_Display) || (image_file[0] == 0)) return 0;

    if (!cache_images) {
	cache_images=1;	    /* we want face commands from server */
	keepcache=TRUE;	    /* Reduce requests for new image */
    }

    if ((infile = fopen(image_file,"r"))==NULL) {
        fprintf(stderr,"Unable to open %s\n", image_file);
	return 0;
    }
    for (i=0; i<MAXPIXMAPNUM; i++)
	private_cache[0].name = NULL;

    i=0;
    while (fgets(databuf,MAX_BUF,infile)) {

	/* First, verify that that image header line is OK */
        if(strncmp(databuf,"IMAGE ",6)!=0) {
	    fprintf(stderr,"ReadImages:Bad image line - not IMAGE, instead\n%s",databuf);
	    return 0;
	}
        num = atoi(databuf+6);
        if (num<0 || num > MAXPIXMAPNUM) {
            fprintf(stderr,"Pixmap number less than zero: %d, %s\n",num, databuf);
            return 0;
	}
	/* Skip accross the number data */
	for (cp=databuf+6; *cp!=' '; cp++) ;
	len = atoi(cp);
	if (len==0 || len>10000) {
	    fprintf(stderr,"ReadImages: length not valid: %d\n%s",
                    len,databuf);
                return 0;
	}
	/* We need the name so that when an FaceCmd comes in, we can look for
	 the matching name.
	 */
	while (*cp!=' ' && *cp!='\n') cp++; /* skip over len */

	/* We only want the last component of the name - not the full path */
	while (*cp != '\n') {
	    if (*cp == '/') last_cp = cp+1; /* don't want the slah either */
	    cp++;
	}
	*cp = '\0';	/* Clear newline */

	private_cache[num].name = strdup_local(last_cp);

	if (fread(databuf, 1, len, infile)!=len) {
           fprintf(stderr,"read_client_images: Did not read desired amount of data, wanted %d\n%s",
                    len, databuf);
                    return 0;
	}
	private_cache[num].checksum=0;
	for (x=0; x<len; x++) {
	    ROTATE_RIGHT(private_cache[num].checksum);
	    private_cache[num].checksum += databuf[x];
	    private_cache[num].checksum &= 0xffffffff;
	}
	if (num > last_face_num) last_face_num = num;
#ifdef HAVE_LIBPNG
#ifdef GDK_XUTIL
	if (pngximage && !(private_cache[num].png_data = png_to_data(databuf, (int)len))) {
	    fprintf(stderr,"Got error on png_to_data\n");
	}
	/* even if using pngximage, we standard image for the inventory list */
	if (png_to_gdkpixmap(gtkwin_root->window, databuf, len, 
		   &private_cache[num].pixmap, &private_cache[num].mask,
			 gtk_widget_get_colormap(gtkwin_root))) {

		fprintf(stderr,"Error loading png file.\n");
	}
#else
	if (png_to_xpixmap(display, win_game, databuf, len, 
		   &private_cache[num].pixmap, &private_cache[num].mask,
		       &colormap, &x, &y)) {

		fprintf(stderr,"Error loading png file.\n");
	}
#endif
#endif
    }
    fclose(infile);
    use_private_cache=1;
    return 0;
}

/* try to find a face in our private cache.  We return the face
 * number if we find one, -1 for no face match
 */
int  find_face_in_private_cache(char *face, int checksum)
{
    int i;

    for (i=1; i<=last_face_num; i++)
	if (!strcmp(face, private_cache[i].name)) {
	    return i;
	}
    return -1;
}

/* Start of map handling code.
 * For the most part, this actually is not window system specific,
 * but certainly how the client wants to store this may vary.
 */

#define MAXFACES 5
#define MAXPIXMAPNUM 10000
struct MapCell {
  short faces[MAXFACES];
  int count;
  uint8 darkness;
  uint8 need_update:1;
  uint8 have_darkness:1;
  uint8 cleared:1; /* Used for fog of war code only */
};

#if 0
struct Map {
  struct MapCell cells[MAP_MAX_SIZE][MAP_MAX_SIZE];
};

static struct Map the_map;
#endif

struct Map {
  struct MapCell **cells;
  /* Store size of map so we know if map_size has changed
   * since the last time we allocated this;
   */
  int x;
  int y;
};

static struct Map the_map;

/*
 * Added for fog of war. Current size of the map structure in memory.
 * We assume a rectangular map so this is the length of one side.
 * command.c needs to know about this so not static 
 * FIX ME: Don't assume rectangle
 */

int map_size= 0;
PlayerPosition pl_pos;


/*
 * Takes three args, first is a return value that is a pointer
 * we should put map info into. Next two are map dimensions.
 * This function supports non rectangular maps but the client
 * pretty much doesn't. The caller is responsible for freeing
 * the data. I have the caller pass in a map struct instead of
 * returning a pointer because I didn't want to change all the
 * the_map.cells to the_map->cells...
 * The returned map memory is zero'ed.
 */
void allocate_map( struct Map* new_map, int ax, int ay)
{

  int i= 0;

  if( new_map == NULL)
    return;

  if( ax < 1 || ay < 1) {
    new_map->cells= NULL;
    return;
  }

  new_map->cells= (struct MapCell**)calloc( sizeof( struct MapCell*) * ay 
					    + sizeof( struct MapCell) *
					    map_size * map_size, 1);
  if( new_map->cells == NULL)
    return;

  /* Skip past the first row of pointers to rows and assign the start of
   * the actual map data
   */
  new_map->cells[0]= (struct MapCell*)((char*)new_map->cells + 
				       (sizeof( struct MapCell*) * ay));

  /* Finish assigning the beginning of each row relative to the first row
   * assigned above
   */
  for( i= 0; i < ay; i++) 
    {
      new_map->cells[i]= new_map->cells[0] + ( i * ax);
    }

  new_map->x= ax;
  new_map->y= ay;

  return;
}

/*
 * Clears out all the cells in the current view (which is 
 * the whole map if not using fog_of_war, and request
 * a map update from the server 
 */
void reset_map()
{
    if( fog_of_war == TRUE)
    {
	int x= 0;
	int y= 0;
	pl_pos.x= the_map.x/2;
	pl_pos.y= the_map.y/2;
	memset( the_map.cells[0], 0, 
		sizeof( struct MapCell) * the_map.x * the_map.y);
	for( x= pl_pos.x; x < (pl_pos.x + mapx); x++) 
	{
	    for( y= pl_pos.y; y < (pl_pos.y + mapy); y++)
	    {
		the_map.cells[x][y].need_update= 1;
	    }
	}
    }
    else
    {
	int x= 0;
	int y= 0;
	memset( the_map.cells[0], 0, 
		sizeof( struct MapCell) * the_map.x * the_map.y);
	for( x= 0; x < mapx; x++)
	{
	    for( y= 0; y < mapy; y++)
	    {
		the_map.cells[x][y].need_update= 1;
	    }
	}
    }
    
    cs_write_string( csocket.fd, "mapredraw", 9);
    
    return;
}

void display_map_clearcell(long x,long y)
{
    if( fog_of_war == TRUE)
    {
	/* we don't want to clear out the values yet. We will do that
	 * next time we try to write some data to this tile. For now
	 * we just mark that it has been cleared. Also mark it for
	 * update so we can draw the proper fog cell over it
	 */
	x+= pl_pos.x;
	y+= pl_pos.y;
	the_map.cells[x][y].cleared= 1;
	the_map.cells[x][y].need_update= 1;
    }
    else 
    {
	int i;
	the_map.cells[x][y].count = 0;
	the_map.cells[x][y].darkness = 0;
	the_map.cells[x][y].need_update = 1;
	the_map.cells[x][y].have_darkness = 0;
	the_map.cells[x][y].cleared= 0;
	for (i=0; i<MAXFACES; i++)
	    the_map.cells[x][y].faces[i] = -1;  /* empty/blank face */
    }

    return;
}

void print_darkness()
{

    int x= 0;
    int y= 0;

    for( y= 0; y < mapy; y++)
    {
	for( x= 0; x < mapx; x++)
	{
	    if( the_map.cells[x][y].count== 0)
		fprintf( stderr, "[ - ]");
	    else
		fprintf( stderr, "[%3d]", the_map.cells[x][y].darkness);
	}
	fprintf( stderr, "\n");
    }
}

void print_map()
{
    int x= 0;
    int y= 0;
    int z= 0;

    int local_mapx;
    int local_mapy;

    if( fog_of_war == TRUE)
    {
	local_mapx= pl_pos.x + mapx;
	local_mapy= pl_pos.y + mapy;
	printf( " Current X pos: %d -- Current Y pos: %d\n", 
		pl_pos.x, pl_pos.y);
    }
    else 
    {
	local_mapx= mapx;
	local_mapy= mapy;
    }

    fprintf( stderr, "-----------------------\n");
    for( y= (fog_of_war == TRUE ? pl_pos.y : 0); y < local_mapy; y++)
    {
	for( z= 0; z < MAXFACES; z++)
	{
	    for( x= (fog_of_war == TRUE ? pl_pos.x : 0); x < local_mapx; x++)
	    {
		if( the_map.cells[x][y].count == 0)
		    fprintf( stderr, "[ -- ]");
		else 
		    fprintf( stderr, "[%4d]", the_map.cells[x][y].faces[z]);
	    }
	    fprintf( stderr, "\n");
	}
	fprintf( stderr, "\n");
    }
    fprintf( stderr, "-----------------------\n");
    return;
}

void set_map_darkness(int x, int y, uint8 darkness)
{
  if( fog_of_war == TRUE)
  {
      x+= pl_pos.x;
      y+= pl_pos.y;
  }

    the_map.cells[x][y].have_darkness = 1;
    if (darkness != (255 - the_map.cells[x][y].darkness )) {
	the_map.cells[x][y].darkness = 255 - darkness;
	the_map.cells[x][y].need_update = 1;
#ifdef GDK_XUTIL
	/* pretty ugly - since the light code with pngximage uses
	 * neighboring spaces to adjust the darkness, we now need to
	 * let the neighbors know they should update their darkness
	 * now.
	 */
	if (pngximage || sdlimage) {
	    if (x-1>0) the_map.cells[x-1][y].need_update = 1;
	    if (y-1>0) the_map.cells[x][y-1].need_update = 1;
	    if (x+1<mapx) the_map.cells[x+1][y].need_update = 1;
	    if (y+1<mapy) the_map.cells[x][y+1].need_update = 1;
	}
#endif /* GDK_XUTIL */
    }
}

/* sets the face at layer to some value.  We just can't
 * restact arbitrarily, as the server now sends faces only
 * for layers that change, and not the entire space.
 */
void set_map_face(int x, int y, int layer, int face)
{
  if( fog_of_war == TRUE)
  {
      x+= pl_pos.x;
      y+= pl_pos.y;
  }

  if( (fog_of_war == TRUE) && (the_map.cells[x][y].cleared == 1) )
  {
      /* This cell has been cleared previously but now we are 
       * writing new data to do. So we have to clear it for real now 
       */
      int i= 0;
      the_map.cells[x][y].count= 0;
      the_map.cells[x][y].darkness= 0;
      the_map.cells[x][y].need_update= 1;
      the_map.cells[x][y].have_darkness= 0;
      the_map.cells[x][y].cleared= 0;
      for (i=0; i<MAXFACES; i++)
	  the_map.cells[x][y].faces[i]= -1;  /* empty/blank face */
  }

  the_map.cells[x][y].faces[layer] = face;
  if ((layer+1) > the_map.cells[x][y].count)
    the_map.cells[x][y].count = layer+1;
  the_map.cells[x][y].need_update = 1;
  the_map.cells[x][y].have_darkness = 1;
}


void display_map_addbelow(long x,long y,long face)
{

    if( fog_of_war == TRUE) 
    {
	x+= pl_pos.x;
	y+= pl_pos.y;
    }
    
    if( (fog_of_war == TRUE) && (the_map.cells[x][y].cleared == 1) )
    {
	/* This cell has been cleared previously but now we are 
	 * writing new data to do. So we have to clear it for real now 
	 */
	int i= 0;
	the_map.cells[x][y].count= 0;
	the_map.cells[x][y].darkness= 0;
	the_map.cells[x][y].need_update= 1;
	the_map.cells[x][y].have_darkness= 0;
	the_map.cells[x][y].cleared= 0;
	for (i=0; i<MAXFACES; i++)
	    the_map.cells[x][y].faces[i]= -1;  /* empty/blank face */
    }

    the_map.cells[x][y].faces[the_map.cells[x][y].count] = face&0xFFFF;
    the_map.cells[x][y].count ++;
    the_map.cells[x][y].need_update = 1;
}

/* 
 * Returns true if virtual view is about to butt up against 
 * the side of the virtual map on the next scroll
 * Only used for fog of war code
 */
int need_recenter_map( int dx, int dy)
{
    
    if( pl_pos.x + dx + mapx >= the_map.x ||
	pl_pos.y + dx + mapy >= the_map.y ||
	pl_pos.x + dx <= 0                ||
	pl_pos.y + dy <= 0 )
    {
	return TRUE;
    }
    
    return FALSE;
}

/*
 * Only used in fog of war code.
 * Will recenter the virtual coordinates of the player view 
 * to the center of the map and try to keep as much current
 * state in memory as possible
 * If view is already close to center it won't move it
 */
void recenter_virtual_map_view( struct Map *map)
{
    static struct Map tmpmap;
    struct MapCell **tmpcells;
    int y_shift= 0;
    int x_shift= 0;
    int x= 0, y= 0;

    if( map == NULL)
	return;


    if( tmpmap.cells == NULL)
    {
	allocate_map( &tmpmap, map->x, map->y);
    }

    /* 
     * If mapsize changed, allocate a new map
     */
    if( tmpmap.x != map->x || tmpmap.y != map->y)
    {
	if( tmpmap.cells)
	    free( tmpmap.cells);

	allocate_map( &tmpmap, map->x, map->y);
    }


    /*
     * If we are less then 1/4 away from either edge of the virtual map
     * or the next move would push us up against the edge (for small
     * virtual maps with large views this could happen before our 0,0 view
     * coordinate is within 1/4 of the edge) we shift to the center.
     */
    if( pl_pos.x <= (map->x/4) || pl_pos.x >= (map->x*3/4) ||
	pl_pos.x + mapx + 1 >= map->x )
    {
	x_shift= map->x/2 - pl_pos.x;
    }
    if( pl_pos.y <= (map->y/4) || pl_pos.y >= (map->y*3/4) ||
	pl_pos.y + mapy + 1 >= map->y )
    {
	y_shift= map->y/2 - pl_pos.y;
    }


    if( x_shift == 0 && y_shift == 0)
	return;

    for( x= 0; x < map->x; x++)
    {
	if( x + x_shift >= map->x || x + x_shift < 0)
	    continue;

	for( y= 0; y < map->y; y++)
	{
	    if( y + y_shift >= map->y || y + y_shift < 0)
		continue;

	    memcpy( (char*)&tmpmap.cells[x + x_shift][y + y_shift],
		    (char*)&map->cells[x][y],
		    sizeof( struct MapCell) );
	}
    }


    pl_pos.x+= x_shift;
    pl_pos.y+= y_shift;


    /*
     * Swap cell arrays then zero out the old cells to avoid another big memcopy
     */
    tmpcells= map->cells;
    map->cells= tmpmap.cells;
    tmpmap.cells= tmpcells;

    memset( (char*)&tmpmap.cells[0][0], 0,
	    sizeof( struct MapCell) * tmpmap.x * tmpmap.y);

    return;
}

  
void display_mapscroll(int dx,int dy)
{
    int x,y;
    static struct Map newmap;
    int local_mapx= 0, local_mapy= 0;

    if( fog_of_war == TRUE) 
    {
	/* We don't need to memcopy any of this stuff around cause 
	 * we are keeping it in memory. We do need to update our
	 * virtual position though
	 */
	
	if( need_recenter_map( dx, dy) == TRUE) 
	{
	    recenter_virtual_map_view( &the_map);
	}
	
	pl_pos.x+= dx;
	pl_pos.y+= dy;
	local_mapx= pl_pos.x + mapx;
	local_mapy= pl_pos.y + mapy;
	
	/*
	 * For cells about to enter the view, mark them as
	 * needing an update. Cells that are already in 
	 * view don't need to be updated since we just memcpy
	 * the image data around. This is needed for proper 
	 * drawing of blank or black tiles coming into view
	 */
	for( x= pl_pos.x; x < pl_pos.x + mapx; x++) {
	    for( y= pl_pos.y; y < pl_pos.y + mapy; y++) {
		if( (x + dx) < pl_pos.x || (x + dx) >= (mapx + pl_pos.x) ||
		    (y + dy) < pl_pos.y || (y + dy) >= (mapy + pl_pos.y) ) 
		{
		    if( x < 0 || y < 0 || x >= the_map.x ||
			y >= the_map.y)
		    {
			continue;
		    }
		    
		    the_map.cells[x][y].need_update= 1;
		}
	    } /* for y */
	} /* for x */
    }
    else 
    {
	local_mapx= mapx;
	local_mapy= mapy;
    }

    if( newmap.cells == NULL)
	allocate_map( &newmap, map_size, map_size);

    /* Check to see if map_size changed since we allocated newmap */
    if( newmap.x != map_size) 
    {
	if( newmap.cells)
	    free( newmap.cells);
	
	allocate_map( &newmap, map_size, map_size);
    }
    
    if( fog_of_war == FALSE) {
      for(x=0;x<mapx;x++) {
	for(y=0;y<mapy;y++) {
	  /* In case its own of range, set the count to zero */
	  if (x+dx < 0 || x+dx >= mapx ||y+dy < 0 || y+dy >= mapy) {
	    memset((char*)&newmap.cells[x][y], 0, sizeof(struct MapCell));
#ifdef GDK_XUTIL
	    /* basically, if using pngximage, don't want to update it, 
	     * since the scrolling below will effectively take care of 
	     * our redraw
	     *
	     * Changed my smacfiggen 6/20/2001 -- When new cells come onto
	     * the map and we aren't using the new map command we want to 
	     * mark these as updated or else blank tiles get blitted with 
	     * old info.
	     *
	     */
	    if ( !map1cmd)
	      newmap.cells[x][y].need_update=1;
#else
	    newmap.cells[x][y].need_update=1;
#endif
	    
	  } else {
	    memcpy((char*)&(newmap.cells[x][y]), (char*)&(the_map.cells[x+dx][y+dy]),
		   sizeof(struct MapCell));
#ifdef GDK_XUTIL
	    /* if using pngximage, we will instead set the map_did_scroll
	     * to 1 - we don't want to regen the backing image
	     */
	    if (!pngximage) {
#ifdef HAVE_SDL
	      if( !sdlimage)
#endif
		newmap.cells[x][y].need_update=1;
	    }
#else
	    newmap.cells[x][y].need_update=1;	/* new space needs to be redrawn */
#endif
	  }
	}
      }
      memcpy((char*)the_map.cells[0],(char*)newmap.cells[0],
	     sizeof(struct MapCell)*newmap.x*newmap.y );
    }
#ifdef GDK_XUTIL
    if (pngximage ) {
	/* move the screen data around - this is more efficient than re-calculating it all 
	 * memmove does support moving around overlapping data, so this is safe.
	 * 
	 */
	if (dy<0) {
	    int offset = -dy * mapx * image_size * image_size * BPP;
	    memmove(screen +offset, screen, mapx * (mapy + dy)* image_size * image_size * BPP);
	} else if (dy>0) {
	    int offset = dy * mapx * image_size * image_size * BPP;
	    memmove(screen, screen + offset, mapx * (mapy + dy)* image_size * image_size * BPP);
	}
	if (dx) {
	    int y;

	    for (y=0; y < mapy * image_size; y++) {
		if (dx<0) 
		    /* -dx because dx is already negative, so this effective adds */
		    memmove(screen + y * mapx * image_size * BPP - dx * image_size * BPP,
			screen + y * mapx * image_size * BPP,
			(mapx + dx) * image_size * BPP);
		else /* dx is positive */
		    memmove(screen + y * mapx * image_size * BPP,
			screen + y * mapx * image_size * BPP + dx * image_size * BPP,
			(mapx - dx) * image_size * BPP);
	    }
	}
	map_did_scroll=1;
    }
#ifdef HAVE_SDL
    if( sdlimage) 
      {
	/* a copy of what pngximage does except sdl specfic
	 * mapsurface->pitch is the length of a scanline in bytes 
	 * including alignment padding
	 */

	SDL_LockSurface( mapsurface);
	if( dy < 0)
	  {
	    int offset= mapsurface->pitch * (-dy*image_size);
	    memmove( mapsurface->pixels + offset, mapsurface->pixels, 
		     mapsurface->pitch * (mapsurface->h + dy*image_size) );
	  }
	else if( dy > 0)
	  {
	    int offset= mapsurface->pitch * (dy*image_size);
	    memmove( mapsurface->pixels,  mapsurface->pixels + offset,
		     mapsurface->pitch * (mapsurface->h - dy*image_size) );
	  }

	if( dx)
	  {
	    int y;
	    for( y= 0; y < mapsurface->h; y++)
	      {
		if( dx < 0)
		  {
		    char* start_of_row= mapsurface->pixels + mapsurface->pitch * y;
		    int offset= ((mapsurface->pitch / mapy) * -dx);
		    memmove( start_of_row + offset, start_of_row,
			     mapsurface->pitch - offset);
		  }
		else 
		  {
		    char* start_of_row= mapsurface->pixels + mapsurface->pitch * y; 
		    int offset= ((mapsurface->pitch / mapy) * dx);
		    memmove( start_of_row, start_of_row + offset,
			     mapsurface->pitch - offset);
		  }
	      }
	  }
	SDL_UnlockSurface( mapsurface);

	map_did_scroll= 1;
      }

#endif /* HAVE_SDL */
	    
/*    fprintf(stderr,"scroll command: %d %d\n", dx, dy);*/
#endif
}


