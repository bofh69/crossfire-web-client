/**
 * @file
 * Metaserver settings, structures, and prototypes.
 */

/* Arbitrary size.  At some point, we would need to cut this off simply
 * for display/selection reasons.
 */
#define MAX_METASERVER 100

/* Various constants we use in the structure */
#define MS_SMALL_BUF	60
#define MS_LARGE_BUF	512

/**
 * @struct Meta_Info
 * Information about individual servers from the metaserver.
 */
typedef struct {
    char    hostname[MS_LARGE_BUF];
    int     port;
    char    html_comment[MS_LARGE_BUF];
    char    text_comment[MS_LARGE_BUF]; /* all comments are text */
    char    archbase[MS_SMALL_BUF];
    char    mapbase[MS_SMALL_BUF];
    char    codebase[MS_SMALL_BUF];
    char    flags[MS_SMALL_BUF];
    int     num_players;
    guint32 in_bytes;
    guint32 out_bytes;
    int     idle_time; /* calculated from last_update value */
    int     uptime;
    char    version[MS_SMALL_BUF];
    int     sc_version;
    int     cs_version;
} Meta_Info;

extern Meta_Info *meta_servers;

/* Before accessing the metaservers structure,
 * a lock against this is needed
 */
extern pthread_mutex_t ms2_info_mutex;

/* Needs to be here because gtk2 client needs to resort for example */
extern int meta_sort(Meta_Info *m1, Meta_Info *m2);

extern int meta_numservers;

extern int cached_servers_num;

#define CACHED_SERVERS_MAX  10
extern char* cached_servers_name[ CACHED_SERVERS_MAX ];
extern char* cached_servers_ip[ CACHED_SERVERS_MAX ];

/* Used by GTK-V2 client to maintain servers.cache */
extern
void metaserver_update_cache(const char *server_name, const char *server_ip);

int metaserver_check_status(void);
int metaserver_check_version(int entry);
int metaserver_get(void);
void metaserver_init(void);