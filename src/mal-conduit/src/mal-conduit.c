/* $Id: mal-conduit.c 390 2006-08-07 12:24:33Z mcdavey $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gnome.h>

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-mail.h>
#include <pi-dlp.h>
#include <pi-version.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>

#include <malsync.h>

#include "mal-conduit.h"

#define MAL_CONDUIT_VERSION "0.9-2.0.4"

#define OBJ_DATA_CONFIG  "conduit_config"
#define OBJ_DATA_OLDCONFIG  "conduit_oldconfig"
#define OBJ_DATA_CONFIG_WINDOW  "config_window"

#define GET_CONDUIT_CFG(s) ((ConduitCfg*)gtk_object_get_data (GTK_OBJECT (s),OBJ_DATA_CONFIG))
#define GET_CONDUIT_OLDCFG(s) ((ConduitCfg*)gtk_object_get_data (GTK_OBJECT (s),OBJ_DATA_OLDCONFIG))
#define GET_CONDUIT_WINDOW(s) ((GtkWidget*)gtk_object_get_data (GTK_OBJECT (s),OBJ_DATA_CONFIG_WINDOW))

#define CONFIG_PREFIX "/gnome-pilot.d/mal-conduit/Pilot_%u/"

typedef struct  {
	gboolean once_a_day;
	time_t last_sync_date;

	gboolean ok_to_sync;
	guint32 pilot_id;

	gchar *proxy_address, *proxy_username, *proxy_password;
	gint proxy_port;
	gchar *socks_proxy_address;
	gint socks_proxy_port;
} ConduitCfg;

typedef sword (*netInitFunc)(AGNetCtx *ctx);
typedef sword (*netCloseFunc)(AGNetCtx *ctx);
typedef int32 (*netCtxSizeFunc)(void);
typedef void  (*netPreSyncHook) (AGNetCtx *ctx, 
                                AGServerConfig *sc,
                                AGLocationConfig *lc,
                                AGSyncProcessor *sp,
                                AGBool connectSecure);
typedef void  (*netPostSyncHook) (AGNetCtx *ctx, 
                                 AGServerConfig *sc,
                                 AGLocationConfig *lc,
                                 AGSyncProcessor *sp,
                                 AGBool connectSecure);

/* prototype to get rid of warnings */
extern PalmSyncInfo* syncInfoNew ();
extern AGUserConfig* getUserConfig(uint32*);

extern netInitFunc     secnetinit;
extern netCloseFunc    secnetclose;
extern netCtxSizeFunc  secctxsize;
extern netPreSyncHook  secnetpresync;
extern netPostSyncHook  secnetpostsync;

extern char *httpProxy;
extern int   httpProxyPort;
extern char *socksProxy;
extern int   socksProxyPort;
extern char *proxyUsername;
extern char *proxyPassword;

extern int sd;
static int daemon_mode = 0;

/* I need this as a global, since I can't pass data
   to malsync callbacks */
GnomePilotConduit *the_conduit;

static void
bonk_sync_date (ConduitCfg *cfg)
{
	char *prefix;

	time (&cfg->last_sync_date);
	prefix = g_strdup_printf (CONFIG_PREFIX, cfg->pilot_id);
	gnome_config_push_prefix (prefix);
	gnome_config_set_int ("last_sync", cfg->last_sync_date);
	gnome_config_pop_prefix ();
	g_free (prefix);
}

#if 1
/* This is used in mal/client/unix/malsync.c:doStartServer(...) */
int32
cmdTASK (void *out, int32 err_code, char *task, AGBool bufferable) {
	if (task) {
		g_message ("TASK : %s", task);
	}
	gnome_pilot_conduit_send_message (the_conduit, task);
	return AGCLIENT_CONTINUE;
}

/* This is used in mal/client/unix/malsync.c:doStartServer(...) */
int32 
cmdITEM (void *out, int32 *returnErrorCode, int32 currentItemNumber,
	 int32 totalItemCount, char *currentItem)
{
	g_message ("%d/%d (%s)", currentItemNumber, totalItemCount, currentItem);
	if (currentItemNumber == totalItemCount)
		g_message ("done");
	
	if (totalItemCount > 0) {
		gnome_pilot_conduit_send_progress (the_conduit, totalItemCount, currentItemNumber);
	}

	return AGCLIENT_CONTINUE;
}
#endif 

static int 
synchronize (GnomePilotConduitStandard *c, 
	     GnomePilotDBInfo *dbi,
	     gpointer unused) 
{
	PalmSyncInfo *pInfo;
	uint32 pilotID;
	AGNetCtx *ctx;
	ConduitCfg *cfg;

	g_message ("MALconduit %s", MAL_CONDUIT_VERSION);
	
	cfg = GET_CONDUIT_CFG (c);
	if (cfg->once_a_day && !cfg->ok_to_sync) {
		gnome_pilot_conduit_send_message (GNOME_PILOT_CONDUIT (c), 
						 _("Already synchronized today"));
		g_message ("already synchronized today");
		return -1;
	}

	bonk_sync_date (cfg);
	cfg->ok_to_sync = FALSE;
	pilotID = cfg->pilot_id;
	sd = dbi->pilot_socket;

	pInfo = syncInfoNew ();
	pInfo->conduit = c;
	if (!loadSecLib (&ctx)) {
	  ctx = (AGNetCtx *)malloc (sizeof (AGNetCtx));
	  AGNetInit (ctx);
	} 

	if (setupPlatformCalls (pInfo)) {
		return -1;
	}

	if (cfg->proxy_address || cfg->socks_proxy_address) {
		g_message ("trying to set proxy stuff...");
		httpProxy = cfg->proxy_address;
		httpProxyPort = cfg->proxy_port;
		socksProxy = cfg->socks_proxy_address;
		socksProxyPort = cfg->socks_proxy_port;
		proxyUsername = cfg->proxy_username;
		proxyPassword = cfg->proxy_password;
	}

	pInfo->userConfig = getUserConfig (&pilotID);
	if (doClientProcessorLoop (pInfo, ctx) == TRUE) {
		bonk_sync_date (cfg);
	}
	storeDeviceUserConfig (pInfo->userConfig, pilotID);

        if (secnetclose) {
            (*secnetclose)(ctx);
        } else {
            AGNetClose (ctx);
        }
        
        syncInfoFree (pInfo);
        free (ctx);
	
	return 0;
}

static void
load_config (ConduitCfg **cfg, GPilotPilot *pilot)
{
	char *prefix;
	struct tm *today;
	struct tm *last_sync;
	int last_day;
	time_t t;

	(*cfg) = g_new0 (ConduitCfg, 1);
	prefix = g_strdup_printf (CONFIG_PREFIX, pilot->pilot_id);
	gnome_config_push_prefix (prefix);
	(*cfg)->once_a_day = gnome_config_get_bool ("once_a_day=TRUE");
	(*cfg)->last_sync_date = (time_t)gnome_config_get_int ("last_sync=1");
	(*cfg)->proxy_address = gnome_config_get_string ("proxy_address");
	(*cfg)->proxy_username = gnome_config_get_string ("proxy_username");
	(*cfg)->proxy_password = gnome_config_get_string ("proxy_password");
	(*cfg)->proxy_port = gnome_config_get_int ("proxy_port");
	(*cfg)->socks_proxy_address = gnome_config_get_string ("socks_proxy_address");
	(*cfg)->socks_proxy_port = gnome_config_get_int ("socks_proxy_port");
	last_sync = gmtime (&((*cfg)->last_sync_date));	
	(*cfg)->pilot_id = pilot->pilot_id;

	last_day = last_sync->tm_yday;
	time (&t);
	today = gmtime (&t);

	g_message ("Last sync was day %d, today is %d", 
		   last_day,
		   today->tm_yday);

	if (today->tm_yday != last_day) {
		(*cfg)->ok_to_sync = TRUE;
	} else {
		(*cfg)->ok_to_sync = FALSE;
	}	

	gnome_config_pop_prefix ();
	g_free (prefix);
}

static void
save_config (ConduitCfg *cfg) 
{
	char *prefix;

	prefix = g_strdup_printf (CONFIG_PREFIX, cfg->pilot_id);
	gnome_config_push_prefix (prefix);
	gnome_config_set_bool ("once_a_day", cfg->once_a_day);
	gnome_config_set_int ("last_sync", cfg->last_sync_date);

	if (cfg->proxy_address) {
		gnome_config_set_string ("proxy_address", cfg->proxy_address);		
		gnome_config_set_string ("proxy_username", cfg->proxy_username);
		gnome_config_set_string ("proxy_password", cfg->proxy_password);
		gnome_config_set_int ("proxy_port", cfg->proxy_port);
	}
	if (cfg->socks_proxy_address) {
		gnome_config_set_string ("socks_proxy_address", cfg->socks_proxy_address);
		gnome_config_set_int ("socks_proxy_port", cfg->socks_proxy_port);
	}

	gnome_config_pop_prefix ();
	g_free (prefix);
}

static ConduitCfg*
dupe_config (ConduitCfg *cfg)
{
	ConduitCfg *result;

	result = g_new0 (ConduitCfg, 1);
	result->once_a_day = cfg->once_a_day;
	result->last_sync_date = cfg->last_sync_date;
	result->ok_to_sync = cfg->ok_to_sync;
	result->pilot_id = cfg->pilot_id;

	return result;
}

static void
copy_config (ConduitCfg *to, ConduitCfg *from)
{
	to->once_a_day = from->once_a_day;
	to->last_sync_date = from->last_sync_date;
	to->ok_to_sync = from->ok_to_sync ;
	to->pilot_id = from->pilot_id; 
}

static GtkWidget
*createCfgWindow (GnomePilotConduit* conduit)
{
	GtkWidget *vbox, *table;
	GtkWidget *label;
	GtkWidget *button;	

	vbox = gtk_vbox_new (FALSE, GNOME_PAD);

	table = gtk_table_new (2, 1, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table), 10);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, GNOME_PAD);

	label = gtk_label_new (_("Only sync MAL once a day"));
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

	button = gtk_check_button_new ();
	gtk_object_set_data (GTK_OBJECT (vbox), "only_once_a_day", button);
	gtk_table_attach_defaults (GTK_TABLE (table), button, 1, 2, 0, 1);

	return vbox;
}

static void
setOptionsCfg (GtkWidget *pilotcfg, ConduitCfg *state)
{
	GtkWidget *once_a_day;
	GtkObject *adj;

	once_a_day = gtk_object_get_data (GTK_OBJECT (pilotcfg), "only_once_a_day");

	g_assert (once_a_day != NULL);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (once_a_day), state->once_a_day);
}


static void
readOptionsCfg (GtkWidget *pilotcfg, ConduitCfg *state)
{
	GtkWidget *once_a_day;
	GtkObject *adj;

	once_a_day = gtk_object_get_data (GTK_OBJECT (pilotcfg), "only_once_a_day");

	g_assert (once_a_day != NULL);

	state->once_a_day = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (once_a_day));
}

static gint
create_settings_window (GnomePilotConduit *conduit, GtkWidget *parent, gpointer data)
{
	GtkWidget *cfgWindow;
	cfgWindow = createCfgWindow (conduit);
	gtk_container_add (GTK_CONTAINER (parent), cfgWindow);
	gtk_widget_show_all (cfgWindow);

	gtk_object_set_data (GTK_OBJECT (conduit), OBJ_DATA_CONFIG_WINDOW, cfgWindow);
	setOptionsCfg (GET_CONDUIT_WINDOW (conduit), GET_CONDUIT_CFG (conduit));

	return 0;
}

static void
display_settings (GnomePilotConduit *conduit, 
		  gpointer data)
{
	setOptionsCfg (GET_CONDUIT_WINDOW (conduit), GET_CONDUIT_CFG (conduit));
}

static void
save_settings (GnomePilotConduit *conduit, 
	       gpointer data)
{
	readOptionsCfg (GET_CONDUIT_WINDOW (conduit), GET_CONDUIT_CFG (conduit));
	save_config (GET_CONDUIT_CFG (conduit));
}

static void
revert_settings  (GnomePilotConduit *conduit, gpointer data)
{
	ConduitCfg *cfg,*cfg2;
	cfg2= GET_CONDUIT_OLDCFG (conduit);
	cfg = GET_CONDUIT_CFG (conduit);
	save_config (cfg2);
	copy_config (cfg,cfg2);
	setOptionsCfg (GET_CONDUIT_WINDOW (conduit),cfg);
}

GnomePilotConduit *conduit_load_gpilot_conduit (GPilotPilot *pilot) 
{
  GtkObject *retval;
  ConduitCfg *cfg, *cfg2;

  retval = gnome_pilot_conduit_standard_new ("AvantGo",0x4176476f, NULL);
  g_assert (retval != NULL);
  the_conduit = GNOME_PILOT_CONDUIT (retval);

  load_config (&cfg, pilot);
  cfg2 = dupe_config (cfg);
  
  gtk_object_set_data (GTK_OBJECT (retval), OBJ_DATA_CONFIG, cfg);
  gtk_object_set_data (GTK_OBJECT (retval), OBJ_DATA_OLDCONFIG, cfg2);

  
  gtk_signal_connect (retval, "synchronize", (GtkSignalFunc)synchronize, NULL);

  gtk_signal_connect (retval, "create_settings_window", (GtkSignalFunc) create_settings_window, NULL);
  gtk_signal_connect (retval, "display_settings", (GtkSignalFunc) display_settings, NULL);
  gtk_signal_connect (retval, "save_settings", (GtkSignalFunc) save_settings, NULL);
  gtk_signal_connect (retval, "revert_settings", (GtkSignalFunc) revert_settings, NULL);

  return GNOME_PILOT_CONDUIT (retval); 
}


void conduit_destroy_gpilot_conduit ( GnomePilotConduit *c ) 
{
#if 0
  ConduitCfg *cc;
  
  cc = GET_CONFIG (c);
  destroy_configuration ( &cc );
#endif
  gtk_object_destroy (GTK_OBJECT (c));
}

