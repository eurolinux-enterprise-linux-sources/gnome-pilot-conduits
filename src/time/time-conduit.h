/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- *//* 
 * Copyright (C) 2001 Free Software Foundation
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
 *
 * Authors: Eskil Heyn Olsen
 */

#ifndef __TIME_CONDUIT_H__
#define __TIME_CONDUIT_H__

#include <time.h>
#include <gpilotd/gnome-pilot-conduit-standard.h>

#define OBJ_DATA_CONDUIT "conduit_data"
#define OBJ_DATA_CONFIG  "conduit_config"
#define OBJ_DATA_OLDCONFIG  "conduit_oldconfig"
#define OBJ_DATA_CONFIG_WINDOW  "config_window"
#define CONFIG_PREFIX    "/gnome-pilot.d/time-conduit/Pilot_%u/"

#define GET_CONDUIT_CFG(s) ((ConduitCfg*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_CONFIG))
#define GET_CONDUIT_OLDCFG(s) ((ConduitCfg*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_OLDCONFIG))
#define GET_CONDUIT_DATA(s) ((ConduitData*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_CONDUIT))
#define GET_CONDUIT_WINDOW(s) ((GtkWidget*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_CONFIG_WINDOW))

typedef struct ConduitCfg {
	GnomePilotConduitSyncType  sync_type;   /* only used by capplet */
	guint32 pilotId;
	time_t t;
} ConduitCfg;

#endif
