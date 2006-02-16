/*
 * A replacement of strdup(), since it's not defined at some
 * unix variants.
 */

extern int init_audio(void);

#define MAX_SOUNDS 1024
char *buffers=NULL;

void play_sound(int soundnum, int soundtype, int x, int y);

char *strdup_local(char *str) {
  char *c=(char *)malloc(sizeof(char)*strlen(str)+1);
  strcpy(c,str);
  return c;
}


typedef struct Sound_Info {
    char *filename;
    char *symbolic;
    unsigned char volume;
    int size;
    unsigned char *data;
} Sound_Info;

Sound_Info normal_sounds[MAX_SOUNDS], spell_sounds[MAX_SOUNDS],
default_normal, default_spell;
/* parses a line from the sound file.  This is a little uglier because
 * we store some static values in the function so we know what we are doing -
 * however, it is somewhat necessary so that we can use this same function
 * to parse both files and the compiled in data.
 * 
 * Note that this function will modify the data in line.  lineno is just
 * for error tracking purposes.
 */

static void parse_sound_line(char *line, int lineno) {
    static int readtype=0, lastnum=0;
    int newnum, len;
    char *cp,*volume,*symbolic,*cp1,filename[512];

    if (line[0]=='#' || line[0]=='\n') return;

    if (!strcmp(line,"Standard Sounds:\n")) {
	lastnum=0; 
	readtype=1;
	return;
    }
    if (!strcmp(line,"Spell Sounds:\n")) {
	lastnum=0; 
	readtype=2;
	return;
    }
    if (!readtype) {
#ifdef SOUND_DEBUG
	fprintf(stderr,"Got input without finding section header yet:\n%d:%s\n",
		lineno, line);
#endif
	return;
    }

    if (line[strlen(line)-1]=='\n') line[strlen(line)-1]='\0';

    len=strcspn(line, " \t");
    line[len]='\0';
    cp = line+len+1;


    /* Skip all whitespace for the next field */
    while (*cp!='\0' && (*cp==' ' || *cp=='\t'))
	cp++;
    
    volume=cp;

    /* No symbolic name or number - that is ok */
    cp1=cp;
    if (!(cp=strchr(cp1,' ')) && !(cp=strchr(cp1,'\t'))) {
	newnum=lastnum+1;
	symbolic=NULL;
    } else {	/* We think we have a symbolic name */
	/* Don't need to nulterm the volume, since we atoi it anyways */
	while (*cp!='\0' && (*cp==' ' || *cp=='\t')) 
	    cp++;

	symbolic=cp;
	/* Some symbolc names are double quote protected.  If, do some
	 * special processing.  We strip off the quotes.
	 */
	if (*symbolic=='"') {
	    symbolic++;
	    for (cp=symbolic; *cp!='\0' && *cp!='"'; cp++) ;
	    *cp='\0';
	    cp++;
	}
	/* Lets try to find the sound number now */
	cp1 = cp;
	if (!(cp=strchr(cp1,' '))  && !(cp=strchr(cp1,'\t')))
	    newnum=lastnum+1;
	else {
	    *cp++='\0';
	    while (*cp!='\0' && (*cp==' ' || *cp=='\t')) 
		cp++;
	    if (isdigit(*cp))
		newnum=atoi(cp);
	    else newnum=lastnum+1;
	}
    }
    if (newnum < 0 || newnum>MAX_SOUNDS) {
	fprintf(stderr,"Invalid sound number %d, line %d, buf %s\n",
		newnum, lineno, line);
	return;
    }

    /* Compatibility processing for older files - if the file ends in
     * .au, convert to .raw.  A bit of a hack, but probably better than
     * trying to play an au file.
     */
    strcpy(filename, line);
    cp = filename + strlen(filename)-3;
    if (!strcmp(cp, ".au")) 
	strcpy(cp, ".raw");

    if (symbolic && !strcmp(symbolic,"DEFAULT")) {
	if (readtype==1) {
	    default_normal.filename=strdup_local(filename);
	    default_normal.volume=atoi(volume);
	} else if (readtype==2) {
	    default_spell.filename=strdup_local(filename);
	    default_spell.volume=atoi(volume);
	}
	return;
    }
    else {
	if (readtype==1) {
	    normal_sounds[newnum].filename = strdup_local(filename);
	    normal_sounds[newnum].volume = atoi(volume);
	    if (symbolic) normal_sounds[newnum].symbolic=strdup_local(symbolic);
	    else normal_sounds[newnum].symbolic=NULL;
	} else if (readtype==2) {
	    spell_sounds[newnum].filename = strdup_local(filename);
	    spell_sounds[newnum].volume = atoi(volume);
	    if (symbolic) spell_sounds[newnum].symbolic=strdup_local(symbolic);
	    else spell_sounds[newnum].symbolic=NULL;
	}
	lastnum=newnum;
    }
}

/* init_sounds open the audio device, and reads any configuration files
 * that need to be.  It returns 0 on success.  On failure, the calling
 * function will likely disable sound support/requests from the server.
 */

int init_sounds(void)
{
    int i;
    FILE *fp;
    char path[256], buf[512];
 
#ifdef SOUND_DEBUG    
    fprintf( stderr,"Settings: bits: %i, ",settings.bit8?8:16);
    fprintf( stderr,"%s, ",settings.sign?"signed":"unsigned");
    fprintf( stderr,"%s, ",settings.stereo?"stereo":"mono");
    fprintf( stderr,"frequency: %i, ",settings.frequency);
    fprintf( stderr,"device: %s\n",settings.audiodev);
#endif

    buffers = (char *)malloc( settings.buffers * settings.buflen );
    if ( !buffers ) return -1;
    sounds_in_buffer = (int *)calloc( settings.buffers,sizeof(int) );
    if ( !sounds_in_buffer ) return -1;

    if (init_audio()) return -1;

    memset(buffers,zerolevel,settings.buflen*settings.buffers);
    
#ifdef SOUND_DEBUG    
    fprintf( stderr,"bits: %i, ",settings.bit8?8:16);
    fprintf( stderr,"%s, ",sign?"signed":"unsigned");
    fprintf( stderr,"%s, ",stereo?"stereo":"mono");
    fprintf( stderr,"freq: %i, ",frequency);
    fprintf( stderr,"smpl_size: %i, ",sample_size);
    fprintf( stderr,"0level: %i\n",zerolevel);
#endif
     
    for (i=0; i<MAX_SOUNDS; i++) {
	normal_sounds[i].filename=NULL;
	spell_sounds[i].filename=NULL;
	normal_sounds[i].size=-1;
	spell_sounds[i].size=-1;
    }
    default_normal.filename=NULL;
    default_spell.filename=NULL;

    sprintf(path,"%s/.crossfire/sounds", getenv("HOME"));
    i=0;
    if (!(fp=fopen(path,"r"))) {
	fprintf(stderr,"Unable to open %s - will use built in defaults\n", path);
	for (; i<sizeof(def_sounds)/sizeof(char*); i++) {
	    strcpy(buf, def_sounds[i]);
	    parse_sound_line(buf,i);
	}
    } else while (fgets(buf, 511, fp)!=NULL) {
	buf[511]='\0';
	parse_sound_line(buf, ++i);
    }
    /* Note in both cases below, we leave the symbolic name untouched. */
    for (i=0; i<MAX_SOUNDS; i++) {
	if (!normal_sounds[i].filename) {
	    normal_sounds[i].filename=default_normal.filename;
	    normal_sounds[i].volume=default_normal.volume;
	}
	if (!spell_sounds[i].filename) {
	    spell_sounds[i].filename=default_spell.filename;
	    spell_sounds[i].volume=default_spell.volume;
	}
	normal_sounds[i].data=NULL;
	spell_sounds[i].data=NULL;
    }
    return 0;
}

int SoundCmd(unsigned char *data,  int len)
{  
    int x, y, num, type;
    int i;
    
    i=sscanf((char *)data,"%x %x %x %x",&num,&type,&x,&y);
    if (i!=4){
        fprintf(stderr,"Wrong input!\n");
	return -1;
    }
#ifdef SOUND_DEBUG    
    fprintf(stderr,"Playing sound %d (type %d), offset %d, %d\n",
	    num, type, x ,y);
#endif	    
    play_sound(num, type, x, y);
    return 0;
}

int write_settings(void) {
FILE *f;
char *home;
char *path;

  if ( (home=getenv("HOME")) == NULL ) return -1;
  path=(char *)malloc(strlen(home)+strlen(CONFIG_FILE)+1);
  if (!path) return -1;
  strcpy(path,home);
  strcat(path,CONFIG_FILE);
  f=fopen(path,"w");
  if (!f) return -1;
  fprintf(f,"# Crossfire sound server settings\n");
  fprintf(f,"# Please note, that not everything will work\n\n");
  fprintf(f,"stereo: %i\n",settings.stereo);
  fprintf(f,"bits: %i\n",settings.bit8?8:16);
  fprintf(f,"signed: %i\n",settings.sign);
  fprintf(f,"frequency: %i\n",settings.frequency);
  fprintf(f,"buffers: %i\n",settings.buffers);
  fprintf(f,"buflen: %i\n",settings.buflen);
  fprintf(f,"simultaneously: %i\n",settings.simultaneously);
/*  fprintf(f,"device: %s\n",settings.audiodev);*/
  fclose(f);
  return 0;
}

int read_settings(void) {
    FILE *f;
    char *home;
    char *path;
    char linebuf[1024];
    if ( (home=getenv("HOME")) == NULL ) return 0;

    path=(char *)malloc(strlen(home)+strlen(CONFIG_FILE)+1);
    if (!path) return 0;

    strcpy(path,home);
    strcat(path,CONFIG_FILE);

    f=fopen(path,"r");
    if (!f) return -1;
  
    while(fgets(linebuf,1023,f)!=NULL) {
	linebuf[1023]=0;
	/* Strip off the newline */
	linebuf[strlen(linebuf)-1]=0;

	if (strncmp(linebuf,"stereo:",strlen("stereo:"))==0)
	    settings.stereo=atoi(linebuf+strlen("stereo:"))?1:0;
	else if (strncmp(linebuf,"bits:",strlen("bits:"))==0)
	    settings.bit8=(atoi(linebuf+strlen("bits:"))==8)?1:0;
	else if (strncmp(linebuf,"signed:",strlen("signed:"))==0)
	    settings.sign=atoi(linebuf+strlen("signed:"))?1:0;
	else if (strncmp(linebuf,"buffers:",strlen("buffers:"))==0)
	    settings.buffers=atoi(linebuf+strlen("buffers:"));
	else if (strncmp(linebuf,"buflen:",strlen("buflen:"))==0)
	    settings.buflen=atoi(linebuf+strlen("buflen:"));
	else if (strncmp(linebuf,"frequency:",strlen("frequency:"))==0)
	    settings.frequency=atoi(linebuf+strlen("frequency:"));
	else if (strncmp(linebuf,"simultaneously:",strlen("simultaneously:"))==0)
	    settings.simultaneously=atoi(linebuf+strlen("simultaneously:"));
#if 0
	else if (strncmp(linebuf,"device: ",strlen("device: "))==0)
		settings.audiodev=strdup_local(linebuf+strlen("device: "));
#endif
    }
    fclose(f);
    return 0;
}
