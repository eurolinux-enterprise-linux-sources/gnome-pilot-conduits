/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/* expense conduit, based on read-expense */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gnome.h>

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-version.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <time.h>

#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard.h>
#include "expense_conduit.h"

#define CONDUIT_VERSION "0.3"

GnomePilotConduit *conduit_get_gpilot_conduit( guint32 pilotId );
void conduit_destroy_gpilot_conduit( GnomePilotConduit *c );

/* Following depend on the ordering in pi-expense.h ! */
static gchar *ExpenseTypeName[] = { "Airfare", "Breakfast", "Bus", "BusinessMeals", "CarRental", 
                                    "Dinner", "Entertainment", "Fax", "Gas", "Gifts", "Hotel", 
                                    "Incidentals","Laundry", "Limo", "Lodging", "Lunch", "Mileage", 
                                    "Other", "Parking", "Postage", "Snack", "Subway", "Supplies", 
                                    "Taxi", "Telephone", "Tips", "Tolls", "Train" };

static gchar *ExpensePaymentName[] = { "AmEx", "Cash", "Check", "CreditCard", "MasterCard", 
                                       "PrePaid", "VISA", "Unfiled" };

/* these values are hardcoded in the palm expense appl. does it differ for non-us palms??? */
static gchar *ExpenseCurrencyName[] = { "AU$", "S", "BF", "R$", "$CN", "DKK", "Mk", "FRF", "DM", 
                                        "HK$", "ISK", "IEP", "L.", "Y", "Flux", "MXP", "NLG", 
                                        "$NZ", "NOK", "Pts", "SEK", "CHF", "GBP", "$", "EU" };

/* #define EC_DEBUG */
#ifdef EC_DEBUG
#define LOG(format,args...) g_log (G_LOG_DOMAIN, \
                                   G_LOG_LEVEL_MESSAGE, \
                                   "expense: "##format, ##args)
#else
#define LOG(format,args...)
#endif

static void 
load_configuration(ConduitCfg **c,guint32 pilotId) 
{
	gchar *prefix;
	gchar *tempbuf;

	g_assert(c!=NULL);
	*c = g_new0(ConduitCfg,1);
	(*c)->child = -1;

	prefix = g_strdup_printf(CONFIG_PREFIX,pilotId);
  
	gnome_config_push_prefix(prefix);
	(*c)->dir = gnome_config_get_string( "dir");
	(*c)->dateFormat = gnome_config_get_string( "date_format=%x");
	(*c)->outputFormat = gnome_config_get_int("output_format=0");
	tempbuf = gnome_config_get_string("dir mode=0700");
	(*c)->dirMode =(mode_t)strtol(tempbuf,NULL,0);
	g_free(tempbuf);
	tempbuf = gnome_config_get_string("file mode=0600");
	(*c)->fileMode =(mode_t)strtol(tempbuf,NULL,0);
	g_free(tempbuf);

	gnome_config_pop_prefix();

	(*c)->pilotId = pilotId;
	g_free(prefix);
}

static void 
save_configuration(ConduitCfg *c) 
{
	gchar *prefix;
        char buf[20];

	g_assert(c!=NULL);

	prefix = g_strdup_printf("/gnome-pilot.d/expense-conduit/Pilot_%u/",c->pilotId);

	gnome_config_push_prefix(prefix);
	gnome_config_set_string("dir", c->dir);
	gnome_config_set_string("date_format", c->dateFormat);
	gnome_config_set_int("output_format", c->outputFormat);
	g_snprintf(buf,sizeof(buf),"0%o", c->dirMode);
	gnome_config_set_string("dir mode", buf);
	g_snprintf(buf,sizeof(buf),"0%o", c->fileMode);
	gnome_config_set_string("file mode", buf);

	gnome_config_pop_prefix();

	gnome_config_sync();
	gnome_config_drop_all();
	g_free(prefix);
}
 
static void 
copy_configuration(ConduitCfg *d, ConduitCfg *c)
{
        g_return_if_fail(c!=NULL);
        g_return_if_fail(d!=NULL);

	d->dir = g_strdup(c->dir);
	d->dateFormat = g_strdup(c->dateFormat);
	d->outputFormat = c->outputFormat;
	d->dirMode = c->dirMode;
	d->fileMode = c->fileMode;

	d->pilotId = c->pilotId;
}

static ConduitCfg*
dupe_configuration(ConduitCfg *c) 
{
	ConduitCfg *retval;

	g_assert(c!=NULL);

	retval = g_new0(ConduitCfg,1);
        copy_configuration(retval,c);
        
	return retval;
}

/** this method frees all data from the conduit config */
static void 
destroy_configuration(ConduitCfg **c) 
{
	g_assert(c!=NULL);
	g_assert(*c!=NULL);
	g_free( (*c)->dir);
	g_free( (*c)->dateFormat);
	g_free(*c);
	*c = NULL;
}


/* from pilot-xfer */
static void protect_name(char *d, char *s) 
{
	while(*s) {
		switch(*s) {
		case '/': *(d++) = '='; *(d++) = '2'; *(d++) = 'F'; break;
		case '=': *(d++) = '='; *(d++) = '3'; *(d++) = 'D'; break;
		case '\x0A': *(d++) = '='; *(d++) = '0'; *(d++) = 'A'; break;
		case '\x0D': *(d++) = '='; *(d++) = '0'; *(d++) = 'D'; break;
			/*case ' ': *(d++) = '='; *(d++) = '2'; *(d++) = '0'; break;*/
		default: 
			if(*s < ' ') {
				gchar tmp[6];
				g_snprintf(tmp,5,"=%2X",(unsigned char)*s);
				*(d++) = tmp[0]; *(d++) = tmp[1]; *(d++) = tmp[2];
			} else
				*(d++) = *s;
			break;
		}
		++s;
	}
	*d = '\0';
}

/** 
    generates a pathname for a category 
 */
static gchar *category_path(int category, GnomePilotConduit *c) 
{
	static gchar filename[FILENAME_MAX];
	gchar buf[64];
	
	if(category < 0 || category >= 16)
		strcpy(buf,"BogusCategory");
	else
		protect_name(buf,GET_CONDUIT_DATA(c)->ai.category.name[category]);
  
	g_snprintf(filename,FILENAME_MAX-1,"%s/%s",
		   GET_CONDUIT_CFG(c)->dir,
		   buf);
	     
	return filename;
}

static void writeout_record(int fd, struct Expense *record, GnomePilotConduit *c)
{
        char entry[0xffff];
    
        const int kDateStrSize = 30;
        char DateStr[kDateStrSize];
        char *Currency;

        strftime(DateStr, kDateStrSize, GET_CONDUIT_CFG(c)->dateFormat, &record->date);

        if(record->currency < 24)
                Currency = ExpenseCurrencyName[record->currency];
        /* handle the euro*/ 
        else if(record->currency == 133)
                Currency = ExpenseCurrencyName[24];
        /* handle the custom currencies */
        else if(record->currency >= 128 && record->currency < 133) 
                Currency = GET_CONDUIT_DATA(c)->ai.currencies[record->currency-128].symbol;
        else {
                g_warning(_("Unknown Currency Symbol"));
                Currency = "";
        }

        switch(GET_CONDUIT_CFG(c)->outputFormat) {
        case eSimpleFormat:
                sprintf(entry, "%s, %s, %s, %s, %s\n", DateStr, ExpenseTypeName[record->type], ExpensePaymentName[record->payment], Currency, record->amount ? record->amount: "<None>");
                break;
        case eComplexFormat:
        default:
                sprintf(entry, "Date: %s, Type: %s, Payment: %s, Currency: %s, Amount: '%s', Vendor: '%s', City: '%s', Attendees: '%s', Note: '%s'\n", DateStr, ExpenseTypeName[record->type], ExpensePaymentName[record->payment], Currency, record->amount ? record->amount: "<None>", record->vendor ? record->vendor: "<None>", record->city ? record->city: "<None>", record->attendees ? record->attendees: "<None>", record->note ? record->note: "<None>");
        }

#ifdef EC_DEBUG
        fprintf(stderr, "%s\n", entry);
#endif
        if(write(fd, entry, strlen(entry)) == -1) {
                perror("writeout_record");
        }
}

/* Parts of this routine is shamelessly stolen from read-expenses.c from the pilot-link 
package, Copyright (c) 1997, Kenneth Albanowski
*/
static gint copy_from_pilot( GnomePilotConduit *c, GnomePilotDBInfo *dbi )
{
        int dbHandle;
#ifdef PILOT_LINK_0_12
        pi_buffer_t *pi_buf;
#else
        guchar buffer[0xffff];
#endif

        struct ExpenseAppInfo *tai;
        struct ExpensePref *tp;

        int i;
        int ret;
        const int numhandles = 16; /* number of categories. See pi-appinfo.h  */
        int filehandle[numhandles];
        int result = 0;

        for (i = 0; i < numhandles; i++)
                filehandle[i] = -1;

        if(GET_CONDUIT_CFG(c)->dir==NULL || strlen(GET_CONDUIT_CFG(c)->dir)==0) {
		g_warning(_("No dir specified. Please run expense conduit capplet first."));
		gnome_pilot_conduit_send_error(c,
                                               _("No dir specified. Please run expense conduit capplet first."));
        }

        tai = &(GET_CONDUIT_DATA(c)->ai);
        tp = &(GET_CONDUIT_DATA(c)->pref);

        g_message ("Expense Conduit v.%s", CONDUIT_VERSION);

        if(dlp_OpenDB(dbi->pilot_socket, 0, 0x80|0x40, "ExpenseDB", &dbHandle) < 0) {
                g_warning("Unable to open ExpenseDB");
                return -1;
        }
    
#ifdef PILOT_LINK_0_12
        pi_buf = pi_buffer_new (0xffff);     
        unpack_ExpensePref(tp, pi_buf->data, 0xffff);        
#else
        unpack_ExpensePref(tp, buffer, 0xffff);
#endif

#ifdef EC_DEBUG
        fprintf(stderr, "Orig prefs, %d bytes:\n", ret);
        fprintf(stderr, "Expense prefs, current category %d, default category %d\n",
                tp->currentCategory, tp->defaultCategory);
        fprintf(stderr, "  Note font %d, Show all categories %d, Show currency %d, Save backup %d\n",
                tp->noteFont, tp->showAllCategories, tp->showCurrency, tp->saveBackup);
        fprintf(stderr, "  Allow quickfill %d, Distance unit %d, Currencies:\n",
                tp->allowQuickFill, tp->unitOfDistance);
        for(i = 0; i < 7; i++) {
                fprintf(stderr, " %d", tp->currencies[i]);
        }
        fprintf(stderr, "\n");
#endif /* EC_DEBUG */

#ifdef PILOT_LINK_0_12
        ret = dlp_ReadAppBlock(dbi->pilot_socket, dbHandle, 0, 0xffff, pi_buf);
        unpack_ExpenseAppInfo(tai, pi_buf->data, 0xffff);
#else
        ret = dlp_ReadAppBlock(dbi->pilot_socket, dbHandle, 0, buffer, 0xffff);
        unpack_ExpenseAppInfo(tai, buffer, 0xffff);
#endif

#ifdef EC_DEBUG
        fprintf(stderr, "Orig length %d, new length %d, orig data:\n", ret, i);
        fprintf(stderr, "New data:\n");
    
        fprintf(stderr, "Expense app info, sort order %d\n", tai->sortOrder);
        for(i = 0; i < 4; i++)
                fprintf(stderr, " Currency %d, name '%s', symbol '%s', rate '%s'\n", i, 
                        tai->currencies[i].name, tai->currencies[i].symbol, tai->currencies[i].rate);
#endif /* EC_DEBUG */

        /* make the directory */
        if(mkdir(GET_CONDUIT_CFG(c)->dir, GET_CONDUIT_CFG(c)->dirMode) < 0) {
                if(errno != EEXIST) {
                        g_warning ("Unable to create directory:\n\t%s\n\t%s\n", 
                                   GET_CONDUIT_CFG(c)->dir,
                                   strerror (errno));
                        goto error;
                }
        }
        
        /* open one file for every category in Expense */
        for(i = 0; i < numhandles; i++) {
                /* skip unused categories */
                if(*tai->category.name[i] != '\0') {
                        LOG("Opening for cat %d: %s\n", i, category_path(i, c));
                        if((filehandle[i] = creat(category_path(i, c), GET_CONDUIT_CFG(c)->fileMode))== -1) {
                                LOG("copy_from_pilot: error in opening %s", category_path(i, c));
                                perror("");
                                goto error;
                        }
                } else {
                        filehandle[i] = -1;
                }
        }

        /* loop through all the records */
        for (i = 0; ; i++) {
                struct Expense t;
                int attr, category, len;

#ifdef PILOT_LINK_0_12
                len = dlp_ReadRecordByIndex(dbi->pilot_socket, dbHandle, i, pi_buf, 0, &attr, &category);
#else
                len = dlp_ReadRecordByIndex(dbi->pilot_socket, dbHandle, i, buffer, 0, 0, &attr, &category);
#endif
                
                /* at the end of all the records? */
                if(len < 0)
                        break;
                /* Skip deleted records */
                if((attr & dlpRecAttrDeleted) || (attr & dlpRecAttrArchived))
                        continue;

#ifdef PILOT_LINK_0_12
                unpack_Expense(&t, pi_buf->data, len);
#else
                unpack_Expense(&t, buffer, len);                
#endif
                /* PalmOS should give us a 4-bit category, and that's all that
                 * pi-appinfo.h can cope with, but lets be cautious */
                if (category < 0 || category >= 16) {
                        g_warning ("Out-of-range category ID from device: %d\n", category);
                        goto error;
                }
                if (filehandle[category] == -1) {
                        g_warning ("Unexpected category ID from device: %d\n", category);
                        goto error;
                }
                writeout_record(filehandle[category], &t, c);
                free_Expense(&t);
        }

        goto exit;
 error:
        result = -1;
 exit:
        /* close all the opened filehandles */
        for(i = 0; i < numhandles; i++)
                if(filehandle[i] != -1)
                        close(filehandle[i]);

        /* Close the database */
        dlp_CloseDB(dbi->pilot_socket, dbHandle);
        
#ifdef PILOT_LINK_0_12
        if (pi_buf) {
                pi_buffer_free (pi_buf);
        }
#endif
         
        return( result );
}

static gint synchronize( GnomePilotConduit *c, GnomePilotDBInfo *dbi ) {
        return copy_from_pilot( c, dbi );
}

/*
 * Gui Configuration Code
 */
static void
insert_ignore_space_cb (GtkEditable    *editable, const gchar    *text,
                        gint len, gint *position, void *data)
{
        gint i;
        const gchar *curname;

        curname = gtk_entry_get_text(GTK_ENTRY(editable));
        if (*curname == '\0' && len > 0) {
                if (isspace(text[0])) {
                        gtk_signal_emit_stop_by_name(GTK_OBJECT(editable), "insert_text");
                        return;
                }
        } else {
                for (i=0; i<len; i++) {
                        if (isspace(text[i])) {
                                gtk_signal_emit_stop_by_name(GTK_OBJECT(editable), 
                                                             "insert_text");
                                return;
                        }
                }
        }
}

static void
insert_numeric_cb(GtkEditable    *editable, const gchar    *text,
                  gint len, gint *position, void *data)
{
	gint i;

	for (i=0; i<len; i++) {
		if (!isdigit(text[i])) {
			gtk_signal_emit_stop_by_name(GTK_OBJECT(editable), "insert_text");
			return;
		}
	}
}


#define DATE_OPTIONS_COUNT 4
typedef struct {
        gchar *name;
        char *format;
} DateSetting_t;

static DateSetting_t date_options[] = { { N_("Day/Month/Year"), "%d/%m/%Y"}, 
                                       { N_("Month/Day/Year"), "%m/%d/%Y"}, 
                                       { N_("Since 1970-01-01 (in sec)"), "%s"}, 
                                       { N_("Local format"), "%x"}
};

#define WRITEOUT_OPTIONS_COUNT 2
typedef struct {
        gchar *name;
        enum ExpenseOutputFormat format;
} WriteoutSetting_t;

static WriteoutSetting_t writeout_options[] = { { N_("Simple"), eSimpleFormat},
                                                { N_("Complex"), eComplexFormat} };

typedef struct _FieldInfo FieldInfo;
struct _FieldInfo
{
	gchar    *name;
	gchar    *label_data;
	gchar    *obj_data;
	gpointer  insert_func;
};


static FieldInfo fields[] = { { N_("Expense Directory:"), NULL, "ExpenseDir", insert_ignore_space_cb},
                       { N_("Directory Mode:"), NULL, "DirMode", insert_numeric_cb},
                       { N_("File Mode:"), NULL, "FileMode", insert_numeric_cb}, 
                       { NULL, NULL, NULL, NULL}
};

static GtkWidget
*createCfgWindow(void)
{
	GtkWidget *vbox, *table;
	GtkWidget *entry, *label;
        GtkWidget *menuItem, *optionMenu;
        GtkMenu   *menu;

        int i, count=0, widget_offset;

	vbox = gtk_vbox_new(FALSE, GNOME_PAD);

	table = gtk_table_new(2, 5, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, GNOME_PAD);

        /* set the date format */
        label = gtk_label_new(_("Date Format:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);

        menu = GTK_MENU(gtk_menu_new());
        for (i = 0; i < DATE_OPTIONS_COUNT; i++) {
                menuItem = gtk_menu_item_new_with_label(_(date_options[i].name));
                gtk_widget_show(menuItem);
                gtk_object_set_data(GTK_OBJECT(menuItem), "format",
                                    date_options[i].format);
                gtk_menu_append(menu, menuItem);
        }

        optionMenu = gtk_option_menu_new(); 
        gtk_option_menu_set_menu(GTK_OPTION_MENU(optionMenu),GTK_WIDGET(menu));

        gtk_table_attach_defaults(GTK_TABLE(table), optionMenu, 1, 2, 1, 2);
        gtk_object_set_data(GTK_OBJECT(vbox), "DateFormat", optionMenu);

        /* set the writeout format */
        label = gtk_label_new(_("Output Format:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);

        menu = GTK_MENU(gtk_menu_new());
        for (i = 0; i < WRITEOUT_OPTIONS_COUNT; i++) {
                menuItem = gtk_menu_item_new_with_label(_(writeout_options[i].name));
                gtk_widget_show(menuItem);
                gtk_object_set_data (GTK_OBJECT (menuItem), "format", 
                                     &writeout_options[i].format);
                gtk_menu_append(menu, menuItem);
        }

        optionMenu = gtk_option_menu_new(); 
        gtk_option_menu_set_menu(GTK_OPTION_MENU(optionMenu),GTK_WIDGET(menu));

        gtk_table_attach_defaults(GTK_TABLE(table), optionMenu, 1, 2, 2, 3);
        gtk_object_set_data(GTK_OBJECT(vbox), "OutputFormat", optionMenu);

        /* ugh, so we have an asymmetry here: above is done in paste&copy fashion 
           and below, we do it nicely with structs and stuff */  

        /* do the dir & modes */

	/* how many fields do we have */
	while(fields[count].name!=0) count++;

        widget_offset = 3;
	for(i = 0; i < count; i++) {
		label = gtk_label_new(_(fields[i].name));
                gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1,
                	widget_offset+i, widget_offset+i+1);
		if(fields[i].label_data!=NULL) {
			gtk_object_set_data(GTK_OBJECT(vbox), fields[i].label_data, label);
		}
		entry = gtk_entry_new_with_max_length(128);
		gtk_object_set_data(GTK_OBJECT(vbox), fields[i].obj_data, entry);
		gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 
                                 widget_offset+i, widget_offset+i+1, 0,0,0,0);
		gtk_signal_connect(GTK_OBJECT(entry), "insert_text",
				   GTK_SIGNAL_FUNC(fields[i].insert_func),
				   NULL);
	}
	

	return vbox;
}

static void
setOptionsCfg(GtkWidget *pilotcfg, ConduitCfg *state)
{
	GtkWidget *DateFormat, *OutputFormat, *ExpenseDir, *DirMode, *FileMode;
        gchar buf[8];

        int i;

	DateFormat = gtk_object_get_data(GTK_OBJECT(pilotcfg), "DateFormat");
	OutputFormat = gtk_object_get_data(GTK_OBJECT(pilotcfg), "OutputFormat");
	ExpenseDir = gtk_object_get_data(GTK_OBJECT(pilotcfg), "ExpenseDir");
	DirMode = gtk_object_get_data(GTK_OBJECT(pilotcfg), "DirMode");
	FileMode = gtk_object_get_data(GTK_OBJECT(pilotcfg), "FileMode");

	g_assert(DateFormat != NULL);
	g_assert(OutputFormat != NULL);
	g_assert(ExpenseDir != NULL);
	g_assert(DirMode != NULL);
	g_assert(FileMode != NULL);
                
	gtk_entry_set_text(GTK_ENTRY(ExpenseDir), state->dir);
	g_snprintf(buf, 7, "0%o", state->dirMode);
	gtk_entry_set_text(GTK_ENTRY(DirMode),buf);
	g_snprintf(buf, 7, "0%o", state->fileMode);
	gtk_entry_set_text(GTK_ENTRY(FileMode),buf);

        /* find the entry in the option menu. if not found, default to the last */
        for(i = 0; i < DATE_OPTIONS_COUNT && g_strncasecmp(state->dateFormat, date_options[i].format, 20) != 0; i++);
        gtk_option_menu_set_history(GTK_OPTION_MENU(DateFormat), i);

        for(i = 0; i < WRITEOUT_OPTIONS_COUNT && state->outputFormat != writeout_options[i].format; i++);
        gtk_option_menu_set_history(GTK_OPTION_MENU(OutputFormat), i);
}

static void
readOptionsCfg(GtkWidget *pilotcfg, ConduitCfg *state)
{
	GtkWidget *ExpenseDir, *DirMode, *FileMode;
        GtkWidget *option_menu, *menu, *menu_item;
        
	ExpenseDir = gtk_object_get_data(GTK_OBJECT(pilotcfg), "ExpenseDir");
	DirMode = gtk_object_get_data(GTK_OBJECT(pilotcfg), "DirMode");
	FileMode = gtk_object_get_data(GTK_OBJECT(pilotcfg), "FileMode");

        state->dir = g_strdup(gtk_entry_get_text(GTK_ENTRY(ExpenseDir)));
        state->dirMode = strtol(gtk_entry_get_text(GTK_ENTRY(DirMode)), NULL, 0);
        state->fileMode = strtol(gtk_entry_get_text(GTK_ENTRY(FileMode)), NULL, 0);

        option_menu = gtk_object_get_data(GTK_OBJECT(pilotcfg), "DateFormat");
        menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(option_menu));
        menu_item = gtk_menu_get_active (GTK_MENU (menu));
        state->dateFormat = g_strdup((gchar*)gtk_object_get_data(GTK_OBJECT(menu_item),"format"));

        option_menu = gtk_object_get_data(GTK_OBJECT(pilotcfg), "OutputFormat");
        menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(option_menu));
        menu_item = gtk_menu_get_active (GTK_MENU (menu));
        state->outputFormat = *(enum ExpenseOutputFormat*)gtk_object_get_data(GTK_OBJECT(menu_item),"format");
        
}

static gint
create_settings_window (GnomePilotConduit *conduit, GtkWidget *parent, gpointer data)
{
	GtkWidget *cfgWindow;

	cfgWindow = createCfgWindow();
	gtk_container_add(GTK_CONTAINER(parent),cfgWindow);
	gtk_widget_show_all(cfgWindow);

	gtk_object_set_data(GTK_OBJECT(conduit),OBJ_DATA_CONFIG_WINDOW,cfgWindow);
	setOptionsCfg(GET_CONDUIT_WINDOW(conduit),GET_CONDUIT_CFG(conduit));

	return 0;
}

static void
display_settings (GnomePilotConduit *conduit, gpointer data)
{
	setOptionsCfg(GET_CONDUIT_WINDOW(conduit),GET_CONDUIT_CFG(conduit));
}

static void
save_settings    (GnomePilotConduit *conduit, gpointer data)
{
	readOptionsCfg(GET_CONDUIT_WINDOW(conduit),GET_CONDUIT_CFG(conduit));
	save_configuration(GET_CONDUIT_CFG(conduit));
}

static void
revert_settings  (GnomePilotConduit *conduit, gpointer data)
{
	ConduitCfg *cfg,*cfg2;

	cfg2= GET_CONDUIT_OLDCFG(conduit);
	cfg = GET_CONDUIT_CFG(conduit);
	save_configuration(cfg2);
	copy_configuration(cfg,cfg2);
	setOptionsCfg(GET_CONDUIT_WINDOW(conduit),cfg);
}

GnomePilotConduit *conduit_get_gpilot_conduit( guint32 pilotId ) 
{
        GtkObject *retval;
        ConduitCfg *cfg, *cfg2;
        ConduitData *cd = g_new0(ConduitData, 1);

        retval = gnome_pilot_conduit_standard_new("ExpenseDB", Expense_Creator, NULL);
        g_assert(retval != NULL);

        gtk_signal_connect(retval, "copy_from_pilot", (GtkSignalFunc)copy_from_pilot ,NULL);
        /*
          gtk_signal_connect(retval, "copy_to_pilot", (GtkSignalFunc) ,NULL);
          gtk_signal_connect(retval, "merge_to_pilot", (GtkSignalFunc) ,NULL);
          gtk_signal_connect(retval, "merge_from_pilot", (GtkSignalFunc) ,NULL);
        */
        gtk_signal_connect(retval, "synchronize", (GtkSignalFunc)synchronize ,NULL);
	gtk_signal_connect (retval, "create_settings_window", (GtkSignalFunc) create_settings_window, NULL);
	gtk_signal_connect (retval, "display_settings", (GtkSignalFunc) display_settings, NULL);
	gtk_signal_connect (retval, "save_settings", (GtkSignalFunc) save_settings, NULL);
	gtk_signal_connect (retval, "revert_settings", (GtkSignalFunc) revert_settings, NULL);

	load_configuration(&cfg,pilotId);
	cfg2 = dupe_configuration(cfg);
	gtk_object_set_data(GTK_OBJECT(retval),OBJ_DATA_CONFIG,cfg);
	gtk_object_set_data(GTK_OBJECT(retval),OBJ_DATA_OLDCONFIG,cfg2);
        gtk_object_set_data(retval,OBJ_DATA_CONDUIT,(gpointer)cd);
        
        return GNOME_PILOT_CONDUIT(retval); 
}

void conduit_destroy_gpilot_conduit( GnomePilotConduit *c ) 
{
        ConduitCfg *cc;
        ConduitData  *cd;
  
        cc = GET_CONDUIT_CFG(c);
        cd = GET_CONDUIT_DATA(c);

        destroy_configuration( &cc );
        gtk_object_destroy(GTK_OBJECT(c));
}

