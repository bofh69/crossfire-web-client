int main(int argc, char *argv[])
{
    int sound,got_one=0;

    /* This needs to be done first.  In addition to being quite quick,
     * it also sets up some paths (client_libdir) that are needed by
     * the other functions.
     */

    init_client_vars();
    
    /* Call this very early.  It should parse all command
     * line arguments and set the pertinent ones up in
     * globals.  Also call it early so that if it can't set up
     * the windowing system, we get an error before trying to
     * to connect to the server.  And command line options will
     * likely change on the server we connect to.
     */
    if (init_windows(argc, argv)) {	/* x11.c */
	fprintf(stderr,"Failure to init windows.\n");
	exit(1);
    }
    csocket.inbuf.buf=malloc(MAXSOCKBUF);

#ifdef HAVE_SYSCONF
    maxfd = sysconf(_SC_OPEN_MAX);
#else
    maxfd = getdtablesize();
#endif

    sound = init_sounds();

    /* Loop to connect to server/metaserver and play the game */
    while (1) {
	reset_client_vars();
	csocket.inbuf.len=0;
	csocket.cs_version=0;

	/* Perhaps not the best assumption, but we are taking it that
	 * if the player has not specified a server (ie, server
	 * matches compiled in default), we use the meta server.
	 * otherwise, use the server provided, bypassing metaserver.
	 * Also, if the player has already played on a server once (defined
	 * by got_one), go to the metaserver.  That gives them the oppurtunity
	 * to quit the client or select another server.  We should really add
	 * an entry for the last server there also.
	 */

	if (!strcmp(server, SERVER) || got_one) {
	    char *ms;
	    metaserver_get_info(meta_server, meta_port);
	    metaserver_show(TRUE);
	    do {
		ms=get_metaserver();
	    } while (metaserver_select(ms));
	    negotiate_connection(sound);
	} else {
	    csocket.fd=init_connection(server, port_num);
	    if (csocket.fd == -1) { /* specified server no longer valid */
		server = SERVER;
		continue;
	    }
	    negotiate_connection(sound);
	}

	got_one=1;
	event_loop();
	/* if event_loop has exited, we most of lost our connection, so we
	 * loop again to establish a new one.
	 */

	/* Need to reset the images so they match up properly and prevent
	 * memory leaks.
	 */
	reset_image_data();
	remove_item_inventory(cpl.ob);
	/* We know the following is the private map structure in
	 * item.c.  But we don't have direct access to it, so
	 * we still use locate.
	 */
	remove_item_inventory(locate_item(0));
	reset_map_data();
    }
    exit(0);	/* never reached */
}
