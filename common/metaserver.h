/**
 * @file
 * Metaserver functions and data structures
 */

typedef void (*ms_callback)(char *, int, int, char *, char *, bool);

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

extern void ms_init(void);
extern void ms_set_callback(ms_callback function);
extern void ms_fetch(void);
extern bool ms_fetch_server(const char *metaserver2);