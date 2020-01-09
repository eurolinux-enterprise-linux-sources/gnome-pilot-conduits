/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- *//* 
 * Copyright (C) 1998-2000 Free Software Foundation
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
 * Authors: Vadim Strizhevsky
 *          Eskil Heyn Olsen
 */

#ifndef __MEMO_FILE_CONDUIT_H__
#define __MEMO_FILE_CONDUIT_H__

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <pi-appinfo.h>
#include <pi-memo.h>
#include <glib.h>
#include <gnome.h>
#include <errno.h>

#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard-abs.h>

#define OBJ_DATA_CONDUIT "conduit_data"
#define OBJ_DATA_CONFIG  "conduit_config"
#define OBJ_DATA_OLDCONFIG  "conduit_oldconfig"
#define OBJ_DATA_CONFIG_WINDOW  "config_window"
#define CONFIG_PREFIX    "/gnome-pilot.d/memo_file-conduit/Pilot_%u/"


typedef struct _MemoLocalRecord MemoLocalRecord;
struct _MemoLocalRecord {
  LocalRecord local;

  gboolean    ignore;
  MemoLocalRecord *next;

  time_t mtime;
  gint category;

  gint length;
  gchar *record;
  gchar *filename;
};

typedef struct _ConduitData ConduitData;

struct _ConduitData {
  struct MemoAppInfo ai;
  GList *records;
  GnomePilotDBInfo *dbi;
};

#define GET_CONDUIT_CFG(s) ((ConduitCfg*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_CONFIG))
#define GET_CONDUIT_OLDCFG(s) ((ConduitCfg*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_OLDCONFIG))
#define GET_CONDUIT_DATA(s) ((ConduitData*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_CONDUIT))
#define GET_CONDUIT_WINDOW(s) ((GtkWidget*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_CONFIG_WINDOW))

typedef struct IterateData {
  int flag;
  int archived;
  MemoLocalRecord *prev;
  MemoLocalRecord *first;
} IterateData;

typedef struct LoadInfo {
  recordid_t id;
  gint secret;
  time_t mtime;
} LoadInfo;


typedef struct _ConduitCfg ConduitCfg;

struct _ConduitCfg 
{
	GnomePilotConduitSyncType  sync_type;   /* only used by capplet */
	mode_t   file_mode;
	mode_t   dir_mode;
	guint32  pilotId;
	gchar   *dir;
	gchar   *ignore_start;
	gchar   *ignore_end;

	gboolean open_secret;
	mode_t   secret_mode;
};
#endif
