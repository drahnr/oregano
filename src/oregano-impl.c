/*
 * oregano-impl.c
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
#include "oregano.h"

/*** App-specific servant structures ***/
typedef struct
{
   POA_GNOME_Oregano_Sheet servant;
   PortableServer_POA poa;
} impl_POA_GNOME_Oregano_Sheet;

/*** Implementation stub prototypes ***/
static void impl_GNOME_Oregano_Sheet__destroy(impl_POA_GNOME_Oregano_Sheet *
	servant, CORBA_Environment * ev);

static void impl_GNOME_Oregano_Sheet_select_all(impl_POA_GNOME_Oregano_Sheet *
	servant, CORBA_Environment * ev);

static CORBA_boolean
impl_GNOME_Oregano_Sheet_is_all_selected(impl_POA_GNOME_Oregano_Sheet *
	servant, CORBA_Environment * ev);

static GNOME_Oregano_Sheet
impl_GNOME_Oregano_Sheet_sheet_load(impl_POA_GNOME_Oregano_Sheet * servant,
	CORBA_char * name, CORBA_Environment * ev);

/*** epv structures ***/
static PortableServer_ServantBase__epv impl_GNOME_Oregano_Sheet_base_epv = {
	NULL,			/* _private data */
	NULL,			/* finalize routine */
	NULL,			/* default_POA routine */
};

static POA_GNOME_Oregano_Sheet__epv impl_GNOME_Oregano_Sheet_epv = {
	NULL,			/* _private */
	(gpointer) & impl_GNOME_Oregano_Sheet_select_all,

	(gpointer) & impl_GNOME_Oregano_Sheet_is_all_selected,

	(gpointer) & impl_GNOME_Oregano_Sheet_sheet_load,

};

/*** vepv structures ***/
static POA_GNOME_Oregano_Sheet__vepv impl_GNOME_Oregano_Sheet_vepv = {
	&impl_GNOME_Oregano_Sheet_base_epv,
	&impl_GNOME_Oregano_Sheet_epv,
};

/*** Stub implementations ***/
GNOME_Oregano_Sheet
impl_GNOME_Oregano_Sheet__create(PortableServer_POA poa, CORBA_Environment * ev)
{
	GNOME_Oregano_Sheet retval;
	impl_POA_GNOME_Oregano_Sheet *newservant;
	PortableServer_ObjectId *objid;

	newservant = g_new0(impl_POA_GNOME_Oregano_Sheet, 1);
	newservant->servant.vepv = &impl_GNOME_Oregano_Sheet_vepv;
	newservant->poa = poa;
	POA_GNOME_Oregano_Sheet__init((PortableServer_Servant) newservant, ev);
	objid = PortableServer_POA_activate_object(poa, newservant, ev);
	CORBA_free(objid);
	retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

	return retval;
}

static void
impl_GNOME_Oregano_Sheet__destroy(impl_POA_GNOME_Oregano_Sheet * servant,
	CORBA_Environment * ev)
{
	PortableServer_ObjectId *objid;

	objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
	PortableServer_POA_deactivate_object(servant->poa, objid, ev);
	CORBA_free(objid);

	POA_GNOME_Oregano_Sheet__fini((PortableServer_Servant) servant, ev);
	g_free(servant);
}

static void
impl_GNOME_Oregano_Sheet_select_all(impl_POA_GNOME_Oregano_Sheet * servant,
	CORBA_Environment * ev)
{
	g_print ("select all\n");
}

static CORBA_boolean
impl_GNOME_Oregano_Sheet_is_all_selected(impl_POA_GNOME_Oregano_Sheet *
	servant, CORBA_Environment * ev)
{
	CORBA_boolean retval;

	g_print ("is all selected\n");
	retval = FALSE;

	return retval;
}

static GNOME_Oregano_Sheet
impl_GNOME_Oregano_Sheet_sheet_load(impl_POA_GNOME_Oregano_Sheet * servant,
	CORBA_char * name, CORBA_Environment * ev)
{
	GNOME_Oregano_Sheet retval;

	g_print ("sheet load\n");
	retval = TRUE;

	return retval;
}

