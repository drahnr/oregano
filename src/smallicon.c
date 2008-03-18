/*
 * smallicon.c
 *
 * 
 * Author: 
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 *
 * 
 *  http://www.dtek.chalmers.se/~d4hult/oregano/ 
 * 
 * Copyright (C) 1999,2000  Richard Hult 
 * 
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the 
 * License, or (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <X11/Xlib.h>

#include "smallicon.h"

void
set_small_icon(GdkWindow *window,
	       GdkPixmap *pixmap,
	       GdkBitmap *mask)
{
#ifdef GNOME2_CONVERSION_COMPLETE
  GdkAtom icon_atom;
  long data[2];
 
  g_return_if_fail (window != NULL);
  g_return_if_fail (pixmap != NULL);

  data[0] = ((GdkPixmapPrivate *)pixmap)->xwindow;
  if (mask)
	  data[1] = ((GdkPixmapPrivate *)mask)->xwindow;
  else
	  data[1] = 0;

  icon_atom = gdk_atom_intern ("KWM_WIN_ICON", FALSE);
  gdk_property_change (window, icon_atom, icon_atom, 
		       32,GDK_PROP_MODE_REPLACE,
		       (guchar *)data, 2);

#endif
}
