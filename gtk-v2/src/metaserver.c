char *rcsid_gtk2_metaserver_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2005 Mark Wedel & Crossfire Development Team

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

    The author can be reached via e-mail to crossfire@metalforge.org
*/


char *get_metaserver()
{
    cpl.input_state = Metaserver_Select;


    while(cpl.input_state==Metaserver_Select) {
        /* 
         * This gtk_main will be quit inside of event_callback
         * when the user enters data into the input_text box
         * at which point the input_state will change.
         */
        gtk_main();
	usleep(10*1000);    /* 10 milliseconds */
    }
    return cpl.input_text;
}
