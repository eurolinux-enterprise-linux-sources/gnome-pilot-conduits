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
 * Authors: Eskil Heyn Olsen
 *          Vadim Strizhevsky
 */

#include <glib.h>
#include <gnome.h>

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-file.h>
#include <pi-dlp.h>
#include <pi-version.h>

#include <gpilotd/gnome-pilot-conduit-standard.h>

#include "time-conduit.h"

#define TC_DEBUG 1

#ifdef TC_DEBUG
#define LOG(format,args...) g_log (G_LOG_DOMAIN, \
                            G_LOG_LEVEL_MESSAGE, \
                            "time_file: " format, ##args)
#else
#define LOG(args...)
#endif

GnomePilotConduit *conduit_load_gpilot_conduit (GPilotPilot *pilot);
void conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit);

#define CONDUIT_VERSION "0.1"

static void 
load_configuration(ConduitCfg **c,guint32 pilotId) 
{
	gchar *prefix;
	gchar *buf;
	g_return_if_fail(c!=NULL);
	
	prefix = g_strdup_printf (CONFIG_PREFIX, pilotId);
 
	*c = g_new0(ConduitCfg,1);
	gnome_config_push_prefix(prefix);

	(*c)->sync_type = GnomePilotConduitSyncTypeCustom; /* this will be reset by capplet */
	gnome_config_pop_prefix();

	(*c)->pilotId = pilotId;
	
	g_free (prefix);
}

static void 
copy_configuration(ConduitCfg *d, ConduitCfg *c)
{
	g_return_if_fail(c!=NULL);
	g_return_if_fail(d!=NULL);
	d->pilotId = c->pilotId;
}

static ConduitCfg*
dupe_configuration(ConduitCfg *c) 
{
	ConduitCfg *d;
	g_return_val_if_fail(c!=NULL,NULL);
	d = g_new0(ConduitCfg,1);
	copy_configuration(d,c);
	return d;
}

static void 
destroy_configuration(ConduitCfg **c) 
{
	g_return_if_fail(c!=NULL);
	g_free(*c);
	*c = NULL;
}

static void 
save_configuration(ConduitCfg *c) 
{
	gchar *prefix;

	g_return_if_fail(c!=NULL);
	prefix = g_strdup_printf (CONFIG_PREFIX, c->pilotId);

	gnome_config_push_prefix(prefix);
	gnome_config_pop_prefix();
	gnome_config_sync();
	gnome_config_drop_all();
	g_free (prefix);
}

static GtkWidget
*createCfgWindow(GnomePilotConduit* conduit, ConduitCfg *cfg)
{
	GtkWidget *vbox;
	GtkWidget *option;
	GtkMenu *menu;
	GtkWidget *m1, *m2;
	GtkWidget *label;

	vbox = gtk_vbox_new (FALSE, GNOME_PAD);	
	
	label = gtk_label_new (_("Please note, that PalmOS 3.3 can not properly set the time."));
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, GNOME_PAD);

	return vbox;
}


static void
setOptionsCfg(GtkWidget *pilotcfg, ConduitCfg *state)
{
}


static void
readOptionsCfg(GtkWidget *pilotcfg, ConduitCfg *state)
{
}


static gint
create_settings_window (GnomePilotConduit *conduit, GtkWidget *parent, gpointer data)
{
	GtkWidget *cfgWindow;
	LOG("create_settings_window");

	cfgWindow = createCfgWindow(conduit, GET_CONDUIT_CFG(conduit));

	gtk_container_add(GTK_CONTAINER(parent),cfgWindow);
	gtk_widget_show_all(cfgWindow);

	gtk_object_set_data(GTK_OBJECT(conduit),OBJ_DATA_CONFIG_WINDOW,cfgWindow);
	setOptionsCfg(GET_CONDUIT_WINDOW(conduit),GET_CONDUIT_CFG(conduit));
	return 0;
}
static void
display_settings (GnomePilotConduit *conduit, gpointer data)
{
	LOG("display_settings");
	setOptionsCfg(GET_CONDUIT_WINDOW(conduit),GET_CONDUIT_CFG(conduit));
}

static void
save_settings    (GnomePilotConduit *conduit, gpointer data)
{
	LOG("save_settings");
	readOptionsCfg(GET_CONDUIT_WINDOW(conduit),GET_CONDUIT_CFG(conduit));
	save_configuration(GET_CONDUIT_CFG(conduit));
}

static void
revert_settings  (GnomePilotConduit *conduit, gpointer data)
{
	ConduitCfg *cfg,*cfg2;
	LOG("revert_settings");
	cfg2= GET_CONDUIT_OLDCFG(conduit);
	cfg = GET_CONDUIT_CFG(conduit);
	save_configuration(cfg2);
	copy_configuration(cfg,cfg2);
	setOptionsCfg(GET_CONDUIT_WINDOW(conduit),cfg);
}

static gint
synchronize (GnomePilotConduit *c,
	     GnomePilotDBInfo *dbi)
{
	struct  SysInfo s;
	time_t t;
	int err;

	err = dlp_ReadSysInfo(dbi->pilot_socket, &s);
	if (err < 0)
		return err;

	if ((s.romVersion) == 0x03303000) {
		gnome_pilot_conduit_send_warning (c, _("Unable to set time due to PalmOS 3.3"));
	} else {
		t = time (NULL);
		err = dlp_SetSysDateTime (dbi->pilot_socket, t);
		LOG ("synchronization to PDA = %d", err);
	}
	
	return err;
}

GnomePilotConduit *
conduit_load_gpilot_conduit (GPilotPilot *pilot)
{
	GtkObject *retval;
	ConduitCfg *cfg, *cfg2;
	
	retval = gnome_pilot_conduit_standard_new ("Unsaved Preferences", (guint32)0x70737973, pilot);
	g_assert (retval != NULL);
	
	LOG("creating time conduit");

	load_configuration (&cfg, pilot->pilot_id);
	cfg2 = dupe_configuration (cfg);

	gtk_object_set_data(GTK_OBJECT(retval),OBJ_DATA_CONFIG,cfg);
	gtk_object_set_data(GTK_OBJECT(retval),OBJ_DATA_OLDCONFIG,cfg2);

	gtk_signal_connect (retval, "synchronize", (GtkSignalFunc)synchronize, cfg);
	gtk_signal_connect (retval, "create_settings_window", (GtkSignalFunc)create_settings_window, NULL);
	gtk_signal_connect (retval, "display_settings", (GtkSignalFunc)display_settings, NULL);
	gtk_signal_connect (retval, "save_settings", (GtkSignalFunc)save_settings, NULL);
	gtk_signal_connect (retval, "revert_settings", (GtkSignalFunc)revert_settings, NULL);

	return GNOME_PILOT_CONDUIT (retval);
}

void
conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit)
{
	ConduitCfg  *cfg=GET_CONDUIT_CFG(conduit);
	ConduitCfg  *cfg2=GET_CONDUIT_OLDCFG(conduit);
	LOG("destroying time conduit");
	
	destroy_configuration(&cfg);
	if(cfg2) destroy_configuration(&cfg2);
	gtk_object_destroy (GTK_OBJECT (conduit));
}


