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

/******************************************************************************
 *
 * Code related to face caching.
 *
 *****************************************************************************/

char facecachedir[MAX_BUF];

/* This holds the name we recieve with the 'face' command so we know what
 * to save it as when we actually get the face.
 */
char *facetoname[MAXPIXMAPNUM];

/* Can be set when user is moving to new machine type */
int updatekeycodes=FALSE;

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
	RootWindow(display, screen_num), (const char*)question_bits, 24,24);

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

/* We only get here if the server believes we are caching images. */
/* We rely on the fact that the server will only send a face command for
 * a particular number once - at current time, we have no way of knowing
 * if we have already received a face for a particular number.
 */

void FaceCmd(unsigned char *data,  int len)
{
    int pnum;
    char *face,buf[MAX_BUF];

    /* A quick sanity check, since if client isn't caching, all the data
     * structures may not be initialized.
     */
    if (!cache_images) {
	fprintf(stderr,"Received a 'face' command when we are not caching\n");
	return;
    }
    pnum = GetShort_String(data);
    face = (char*)data+2;
    data[len] = '\0';
    /* To prevent having a directory with 2000 images, we do a simple
     * split on the first 2 characters.
     */
    sprintf(buf,"%s/%c%c/%s", facecachedir, face[0], face[1],face);
    if (display_mode == Xpm_Display)
	strcat(buf,".xpm");
    else if (display_mode == Png_Display)
	strcat(buf,".png");

    /* check to see if we already have the file.  IF not, we need to request
     * it from the server.
     */
    if (access(buf,R_OK)) {
	requestface(pnum, face, buf);
    } else if (display_mode==Xpm_Display) {
	XpmAttributes xpm_attr;
	Pixmap pixmap, mask;

	xpm_attr.colormap = colormap;
	xpm_attr.valuemask = XpmColormap;
	/* Fail on this read, we will request a new copy */
	if (XpmReadFileToPixmap(display,win_game,buf,
                &pixmap,&mask,&xpm_attr)!=XpmSuccess) {
	    requestface(pnum, face, buf);
	} else {
	    pixmaps[pnum].pixmap = pixmap;
	    pixmaps[pnum].mask = mask;
/*	    fprintf(stderr,"Successfully loaded %s (%d) from cache\n", buf, pnum);*/
	}
    } else if (display_mode==Png_Display) {
#ifdef HAVE_IMLIB_H
	Pixmap pixmap, mask;

	/* Fail on this read, we will request a new copy */
	if (Imlib_load_file_to_pixmap(id, buf, &pixmap, &mask)==0) { 
	    requestface(pnum, face, buf);
	} else {
	    pixmaps[pnum].pixmap = pixmap;
	    pixmaps[pnum].mask = mask;
	}
#endif
    } else if (display_mode==Pix_Display) {
	FILE *bitmap, data[MAX_BUF];

	if ((bitmap=fopen(buf,"r"))==NULL) {
		fprintf(stderr,"Unable to open %s when we should have been able to\n",buf);
		requestface(pnum, face, buf);
	} else if (fread(data, 1, 72, bitmap)!=72)  {
		fprintf(stderr,"Read incorrect number of bytes from %s\n",buf);
		requestface(pnum, face, buf);
		fclose(bitmap);
	}
	else {
	    pixmaps[pnum].bitmap = XCreateBitmapFromData(display,
                               RootWindow(display,DefaultScreen(display)),
                               (char*)data,24,24);
	    fread(&pixmaps[pnum].fg, 1, 4, bitmap);
	    fread(&pixmaps[pnum].bg, 1, 4, bitmap);
	    fclose(bitmap);
/*	    fprintf(stderr,"Successfully loaded %s (%d) from cache\n", buf, pnum);*/
	}
    }
}


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

/*
 * This function adds the path to the fontpath of the given display.
 * It's mostly copied from the X11R5 distribution.
 */

static void set_font_path(Display *dpy, char *path) {
  char **currentList = NULL; int ncurrent = 0;
  char **directoryList = NULL; int ndirs = 0;

  currentList = XGetFontPath (dpy, &ncurrent);
  if(currentList==NULL) {
    fprintf(stderr,"Unable to get old font path.\n");
    return;
  }
  {
    register char *cp = path;
    ndirs=1;
    while((cp=strchr(cp, ','))!=NULL)
      ndirs++,cp++;
    directoryList=(char **) malloc(ndirs*sizeof(char *));
    if(!directoryList) {
      fprintf(stderr,"Unable to allocate memory for font path directory.\n");
      return;
    }
  }
  {
    int i=0;
    char *cp = path;
    directoryList[i++]=cp;
    while((cp=strchr(cp, ','))!=NULL)
      directoryList[i++]=cp+1,
      *cp++='\0';
    if(i!=ndirs) {
      fprintf(stderr,"Internal error, only parsed %d of %d dirs.\n",i,ndirs);
      return;
    }
  }
  {
    int nnew=ndirs+ncurrent;
    char **newList = (char **) malloc (nnew * sizeof(char *));

    if(!newList) {
      fprintf(stderr,"Couldn't get memory for new fontpath.\n");
      return;
    }
    memcpy((void *)newList,(void *)directoryList,
           (unsigned) (ndirs*sizeof (char *)));
    memcpy((void *) (newList + ndirs), (void *) currentList,
           (unsigned) (ncurrent*sizeof (char *)));
    XSetFontPath(dpy,newList, nnew);
    free((char *)newList);
  }
  if (directoryList)
    free((char *) directoryList);
  if (currentList)
    XFreeFontPath (currentList);
}

/*
 * Checks if "crossfire" is present somewhere in the fontpath of
 * the given display.
 */

static int check_font_path(Display *dpy) {
  int count;
  char **list;

  list = XListFonts(dpy, font_graphic, 1, &count);
  fprintf(stderr, "Matching fonts to %s: %d (%s)\n",
      font_graphic,count,count?*list:"");
  XFreeFontNames(list);
  return count;
}

/*
 * Uses check_font_path() and set_font_path() to check and, if needed
 * fix the fontpath for the player.
 * Function changed around to make it useful for the client.
 * Passing the player struct to this is not required - xio.c
 * can use the return value to set the use_pixmaps value in the
 * player struct.  name is only passed to give better error
 * messages.
 */

static int fixfontpath(Display *disp, char *name) {

  if (check_font_path(disp))
    return 0;

  fprintf(stderr,"Trying to fix fontpath for display %s.\n",name);
  set_font_path(disp,FONTDIR);
  if(check_font_path(disp))
     return 0;
  fprintf(stderr,"Failed, switching to pixmaps (this might take a while).\n");
  return 1;
}



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


static KeyCode firekey[2], runkey[2], commandkey, *bind_keycode;
static KeySym firekeysym[2], runkeysym[2], commandkeysym,*bind_keysym;
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


typedef struct Keys {
    uint8	flags;
    sint8	direction;
    KeySym	keysym;
    char	*command;
    struct Keys	*next;
} Key_Entry;

/* Key codes can only be from 8-255 (at least according to
 * the X11 manual.  This is easier than using a hash
 * table, quicker, and doesn't use much more space.
 */

#define MAX_KEYCODE 255
static Key_Entry *keys[256];




/* Updates the keys array with the keybinding that is passed.  All the
 * arguments are pretty self explanatory.  flags is the various state
 * that the keyboard is in.
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


static void parse_key_release(KeyCode kc, KeySym ks) {

    /* Only send stop firing/running commands if we are in actual
     * play mode.  Something smart does need to be done when the character
     * enters a non play mode with fire or run mode already set, however.
     */
#if 0	/* I think this causes more problems than it solves */
    if (cpl.input_state != Playing) return;
#endif

    if (kc==firekey[0] || ks==firekeysym[0] || 
	kc==firekey[1] || ks==firekeysym[1]) {
#if 0	/* Nice idea, but unfortunately prints too many false results */
		if (cpl.echo_bindings) draw_info("stop fire",NDI_BLACK);
#endif
		cpl.fire_on=0;
		stop_fire();
		draw_message_window(0);
	}
    else if (kc==runkey[0] || ks==runkeysym[0] ||
	kc==runkey[1] || ks==runkeysym[1]) {
		cpl.run_on=0;
		if (cpl.echo_bindings) draw_info("stop run",NDI_BLACK);
		stop_run();
		draw_message_window(0);
	}
    /* Firing is handled on server side.  However, to keep more like the
     * old version, if you release the direction key, you want the firing
     * to stop.  This should do that.
     */
    else if (cpl.fire_on) stop_fire();
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
	draw_prompt(">");
	cpl.input_state = Command_Mode;
	cpl.no_echo=FALSE;
	return;
    }
    if (keycode == firekey[0] || keysym==firekeysym[0] ||
	keycode == firekey[1] || keysym==firekeysym[1]) {
		cpl.fire_on=1;
		draw_message_window(0);
		return;
	}
    if (keycode == runkey[0] || keysym==runkeysym[0] ||
	keycode==runkey[1] || keysym==runkeysym[1]) {
		cpl.run_on=1;
		draw_message_window(0);
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
	    sprintf(buf,">%s", cpl.input_text);
	    draw_prompt(buf);
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


  /* Perhaps we should start at 8, so0 that we only show 'active'
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
    draw_info("Usage: bind [-nfre] {<commandline>/commandkey/firekey{1/2}/runkey{1/2}}",NDI_BLACK);
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
	    draw_info("Usage: unbind <entry_number> or",NDI_BLACK);
	    draw_info("Usage: unbind [-a] to show existing bindings (-a shows all bindings)",NDI_BLACK);
	    return;
	}
    }
    if ((keyentry=atoi(params))==0) {
	draw_info("Usage: unbind <entry_number> or",NDI_BLACK);
	draw_info("Usage: unbind [-a] to show existing bindings (-a shows all bindings)",NDI_BLACK);
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


void load_defaults()
{
    char path[MAX_BUF],inbuf[MAX_BUF],*cp;
    FILE *fp;

    sprintf(path,"%s/.crossfire/defaults", getenv("HOME"));
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
	    else if (!strcmp(cp,"pixmap"))
		display_mode = Pix_Display;
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
	if (!strcmp(inbuf,"scrolllines")) {
	    infodata.maxlines = atoi(cp);
	    continue;
	}
	if (!strcmp(inbuf,"scrollinfo")) {
	    if (!strcmp(cp,"True")) infodata.scroll_info_window=TRUE;
	    else infodata.scroll_info_window=FALSE;
	    continue;
	}
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

	fprintf(stderr,"Got line we did not understand: %s: %s", inbuf, cp);
    }
    fclose(fp);
}

void save_defaults()
{
    char path[MAX_BUF],buf[MAX_BUF];
    FILE *fp;

    sprintf(path,"%s/.crossfire/defaults", getenv("HOME"));
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
    }
    fprintf(fp,"cacheimages: %s\n", cache_images?"True":"False");
    fprintf(fp,"split: %s\n", split_windows?"True":"False");
    fprintf(fp,"showicon: %s\n", inv_list.show_icon?"True":"False");
    fprintf(fp,"scrolllines: %d\n", infodata.maxlines);
    fprintf(fp,"scrollinfo: %s\n", infodata.scroll_info_window?"True":"False");
    fprintf(fp,"sound: %s\n", nosound?"False":"True");
    fprintf(fp,"command_window: %d\n", cpl.command_window);
    fprintf(fp,"foodbeep: %s\n", cpl.food_beep?"True":"False");
    fclose(fp);
    sprintf(buf,"Defaults saved to %s",path);
    draw_info(buf,NDI_BLUE);
}

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

    inv_list.env->inv_updated =1;
}

