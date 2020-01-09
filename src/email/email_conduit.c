/* $Id: email_conduit.c 390 2006-08-07 12:24:33Z mcdavey $ */

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
#include <fcntl.h>
#include <utime.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>

#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard.h>
#include "email_conduit.h"

#define CONDUIT_VERSION "0.10"

/*#define EC_DEBUG */
#ifdef EC_DEBUG
#define LOG(format,args...) g_log (G_LOG_DOMAIN, \
                                   G_LOG_LEVEL_MESSAGE, \
                                   "email: " format, ##args)
#else
#define LOG(format,args...)
#endif

GnomePilotConduit *conduit_get_gpilot_conduit( guint32 pilotId ) ;
void conduit_destroy_gpilot_conduit( GnomePilotConduit *c );

static void 
load_configuration(ConduitCfg **c,guint32 pilotId) 
{
	gchar *prefix;

	g_assert(c!=NULL);
	*c = g_new0(ConduitCfg,1);
	(*c)->child = -1;

	prefix = g_strdup_printf("/gnome-pilot.d/email-conduit/Pilot_%u/",pilotId);
  
	gnome_config_push_prefix(prefix);
	(*c)->sendmail = gnome_config_get_string( "sendmail=/usr/lib/sendmail -t -i");
	(*c)->fromAddr = gnome_config_get_string( "from_address" );
	(*c)->sendAction = gnome_config_get_string( "send_action=file");
	(*c)->mhDirectory = gnome_config_get_string( "mh_directory" );
	(*c)->mboxFile = gnome_config_get_string ( "mbox_file" );
	(*c)->receiveAction = gnome_config_get_string( "receive_action=copy" );
	gnome_config_pop_prefix();

	(*c)->pilotId = pilotId;
	g_free(prefix);
}

static void
save_configuration(ConduitCfg *c)
{
	gchar *prefix;

	g_assert(c!=NULL);

	prefix = g_strdup_printf("/gnome-pilot.d/email-conduit/Pilot_%u/",c->pilotId);

	gnome_config_push_prefix(prefix);
	gnome_config_set_string("sendmail", c->sendmail);
	gnome_config_set_string("from_address", c->fromAddr);
	gnome_config_set_string("send_action", c->sendAction);
	gnome_config_set_string("mh_directory", c->mhDirectory);
	gnome_config_set_string("mbox_file", c->mboxFile);
	gnome_config_set_string("receive_action", c->receiveAction);
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

	/* it is always safe to free NULL pointers with [g_]free */
	g_free(d->sendmail);
	g_free(d->fromAddr);
	g_free(d->sendAction);
	g_free(d->mhDirectory);
	g_free(d->mboxFile);
	g_free(d->receiveAction);

	d->sendmail      = g_strdup(c->sendmail);
	d->fromAddr      = g_strdup(c->fromAddr);
	d->sendAction    = g_strdup(c->sendAction);
	d->mhDirectory   = g_strdup(c->mhDirectory);
	d->mboxFile      = g_strdup(c->mboxFile);
	d->receiveAction = g_strdup(c->receiveAction);

	d->pilotId       = c->pilotId;
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

/** this method frees all data from the conduit config */
static void 
destroy_configuration(ConduitCfg **c) 
{
	g_assert(c!=NULL);
	g_assert(*c!=NULL);
	g_free( (*c)->sendmail );
	g_free( (*c)->fromAddr );
	g_free( (*c)->sendAction );
	g_free( (*c)->mhDirectory );
	g_free( (*c)->mboxFile );
	g_free( (*c)->receiveAction );
	g_free(*c);
	*c = NULL;
}

void markline( char *msg ) 
{
    while( (*msg) != '\n' && (*msg) != 0 ) {
        msg++; 
    }
    (*msg) = 0;
}

int openmhmsg( char *dir, int num ) 
{ 
    char filename[1000];
    
    sprintf( filename, "%s/%d", dir, num ); 
    return( open( filename, O_RDONLY ) );
}

char *skipspace( char *c ) 
{
    while ( c && ((*c == ' ') || (*c == '\t')) ) { 
        c++;
    }
    return( c );
}

void header( struct Mail *m, char *t )
{
    static char holding[4096];

    if ( t && strlen(t) && ( t[strlen(t)-1] == '\n' ) ) {
        t[strlen(t)-1] = 0;
    }
         
    if ( t && ((t[0] == ' ') || (t[0] == '\t')) ) { 
        if ( (strlen(t) + strlen(holding)) > 4096 ) { 
            return; /* Just discard approximate overflow */
        }
        strcat( holding, t+1 ); 
        return; 
    }
           
    /* Decide on what we do with m->sendTo */
           
    if ( strncmp( holding, "From:", 5 ) == 0 ) { 
        m->from = strdup( skipspace( holding + 5 ) ); 
    } else if ( strncmp( holding, "To:", 3 ) == 0 ) { 
        m->to = strdup( skipspace( holding + 3 ) ); 
    } else if ( strncmp( holding, "Subject:", 8 ) == 0 ) { 
        m->subject = strdup( skipspace( holding + 8 ) ); 
    } else if ( strncmp( holding, "Cc:", 3 ) == 0 ) {
        m->cc = strdup( skipspace( holding + 3 ) ); 
    } else if ( strncmp( holding, "Bcc:", 4 ) == 0 ) { 
        m->bcc = strdup( skipspace( holding + 4 ) ); 
    } else if ( strncmp( holding, "Reply-To:", 9 ) == 0 ) { 
        m->replyTo = strdup( skipspace( holding + 9 ) ); 
    } else if ( strncmp( holding, "Date:", 4 ) == 0 ) { 
        time_t d = parsedate(skipspace(holding+5)); 
        
        if ( d != -1 ) { 
            struct tm * d2; 
            
            m->dated = 1; 
            d2 = localtime( &d ); 
            m->date = *d2; 
        } 
    } 
    
    holding[0] = 0; 
    
    if ( t ) { 
        strcpy( holding, t );
    }
}

/* helper function to identify identical e-mails */
static gint match_mail(gconstpointer a, gconstpointer b)
{
    MailDBRecord *c = (MailDBRecord *) a;
    MailDBRecord *d = (MailDBRecord *) b;

    LOG("matching records [%d vs. %d]", c->size, d->size);
    if (c->size != d->size) {
	return 1;
    }

    return memcmp(c->buffer, d->buffer, c->size);
}

static gboolean
write_message_to_pilot (GnomePilotConduit *c, GnomePilotDBInfo *dbi, 
			int dbHandle, char *buffer, int msg_num)
{
    char *msg;
    int h;
    struct Mail t;
    int len;
    GList *inbox_list;
    GList *field;
    MailDBRecord needle;
    
    t.to = NULL;
    t.from = NULL;
    t.cc = NULL;
    t.bcc = NULL;
    t.subject = NULL;
    t.replyTo = NULL;
    t.sentTo = NULL;
    t.body = NULL;
    t.dated = 0;

    /* initialise these to something */
    t.read = 0;
    t.signature = 0;
    t.confirmRead = 0;
    t.confirmDelivery = 0;
    t.priority = 0;
    t.addressing = 0;

    msg = (char *)buffer;
    h = 1;
    
    while ( h == 1 ) {
	markline( msg );
	
	if ( ( msg[0] == 0 ) && ( msg[1] == 0 ) ) {
	    break;
	}
	
	if ( msg[0] == 0 ) {
	    h = 0;
	    header( &t, 0 );
	} else {
	    header( &t, msg );
	}
	msg += strlen(msg)+1;
    }
    
    if ( (*msg) == 0 ) {
	h = 1;
    }
    
    if ( h ) {
	fprintf( stderr, "Incomplete message %d\n", msg_num );
	free_Mail( &t );
	return FALSE;
    }
    
    t.body = strdup( msg );
    
    len = pack_Mail( &t, buffer, 0xffff );

    /* if this mail already exists in the Palms inbox then skip this mail */
    needle.size = len;
    needle.buffer = buffer;
    inbox_list = (GList*) gtk_object_get_data(GTK_OBJECT(c), "inbox_list");
    field = g_list_find_custom(inbox_list, &needle, match_mail);
    if (field) {
    	/* remove the mail from the list as we've already seen it */
	inbox_list = g_list_remove_link(inbox_list, field);
	gtk_object_set_data(GTK_OBJECT(c),"inbox_list",(gpointer)inbox_list);
	free(field->data);
	g_list_free_1(field);
	LOG("Skipping message (already on Palm device)");
	return TRUE;
    }
    
    if ( dlp_WriteRecord( dbi->pilot_socket, dbHandle, 0, 0, 0, buffer,
			  len, 0 ) > 0 ) {
	return TRUE;
    } else {
	fprintf( stderr, "Error writing message to Pilot\n" );
	return FALSE;
    }
}

static gint synchronize( GnomePilotConduit *c, GnomePilotDBInfo *dbi ) 
{
    int dbHandle;
#ifdef PILOT_LINK_0_12
    pi_buffer_t *pi_buf;
#else
    guchar buffer[0xffff];
#endif
    struct MailAppInfo tai;
    struct MailSyncPref pref;
    struct MailSignaturePref sig;
    int i;
    int rec;
    int dupe;
    GList *inbox_list;
   
    g_message ("SendMail Conduit v %s",CONDUIT_VERSION);

    memset( &tai, '\0', sizeof( struct MailAppInfo ) );

    if ( dlp_OpenDB( dbi->pilot_socket, 0, 0x80|0x40, "MailDB", 
                     &dbHandle ) < 0 ) {
        fprintf( stderr, "Unable to open mail database\n" );
        return( -1 );
    }

#ifdef PILOT_LINK_0_12
    pi_buf = pi_buffer_new (0xffff);    
    dlp_ReadAppBlock( dbi->pilot_socket, dbHandle, 0, 0xffff, pi_buf);
    unpack_MailAppInfo( &tai, pi_buf->data, 0xffff );
#else
    dlp_ReadAppBlock( dbi->pilot_socket, dbHandle, 0, buffer, 0xffff );
    unpack_MailAppInfo( &tai, buffer, 0xffff );
#endif
   
    pref.syncType = 0;
    pref.getHigh = 0;
    pref.getContaining = 0;
    pref.truncate = 8 * 1024;
    pref.filterTo = 0;
    pref.filterFrom = 0;
    pref.filterSubject = 0;

#ifdef PILOT_LINK_0_12
     if ( pi_version( dbi->pilot_socket ) > 0x0100 ) {
         if ( dlp_ReadAppPreference( dbi->pilot_socket, makelong("mail"), 1, 1, 
                                    pi_buf->allocated, pi_buf->data, 0, 0 ) >= 0 ) {
            unpack_MailSyncPref( &pref, pi_buf->data, pi_buf->allocated );
         } else { 
             if ( dlp_ReadAppPreference( dbi->pilot_socket, makelong("mail"), 1,
                                        1, pi_buf->allocated, pi_buf->data, 0, 0 ) >= 0 ) { 
                unpack_MailSyncPref( &pref, pi_buf->data, pi_buf->allocated); 
             } else {
 	      LOG("Couldn't get any mail preferences.\n",0);
             }
         } 
 
          if ( dlp_ReadAppPreference( dbi->pilot_socket, makelong("mail"), 3, 1, 
                                    pi_buf->allocated, pi_buf->data, 0, 0 ) > 0 ) {
            unpack_MailSignaturePref( &sig, pi_buf->data, pi_buf->allocated);
         } 
     }
#else
    if ( pi_version( dbi->pilot_socket ) > 0x0100 ) {
        if ( dlp_ReadAppPreference( dbi->pilot_socket, makelong("mail"), 1, 1, 
                                    0xffff, buffer, 0, 0 ) >= 0 ) {
            unpack_MailSyncPref( &pref, buffer, 0xffff );
        } else { 
            if ( dlp_ReadAppPreference( dbi->pilot_socket, makelong("mail"), 1,
                                        1, 0xffff, buffer, 0, 0 ) >= 0 ) { 
                unpack_MailSyncPref( &pref, buffer, 0xffff ); 
            } else {
	      LOG("Couldn't get any mail preferences.\n",0);
            }
        } 

        if ( dlp_ReadAppPreference( dbi->pilot_socket, makelong("mail"), 3, 1, 
                                    0xffff, buffer, 0, 0 ) > 0 ) {
            unpack_MailSignaturePref( &sig, buffer, 0xffff );
        } 
    }
#endif

    for ( i = 0; ; i++ ) {
        struct Mail t;
        int attr;
        int size=0;
        recordid_t recID;
        int length;
        FILE * sendf;

#ifdef PILOT_LINK_0_12
         length = dlp_ReadNextRecInCategory( dbi->pilot_socket, dbHandle, 1,
                                            pi_buf, &recID, 0, &attr );	
#else
        length = dlp_ReadNextRecInCategory( dbi->pilot_socket, dbHandle, 1,
                                            buffer, &recID, 0, &size, &attr );	
#endif
        if ( length < 0 ) {
            break;
        }
        
        if ( ( attr & dlpRecAttrDeleted ) || ( attr & dlpRecAttrArchived ) ) {
            continue;
        }

#ifdef PILOT_LINK_0_12
	unpack_Mail( &t, pi_buf->data, pi_buf->used);
#else
        unpack_Mail( &t, buffer, length );
#endif
 
        sendf = popen( GET_CONFIG(c)->sendmail, "w" );
        if ( sendf == NULL ) {
            fprintf( stderr, "Unable to create sendmail process\n" );
            break;
        }

        if ( GET_CONFIG(c)->fromAddr ) {
            fprintf( sendf, "From: %s\n", 
                     GET_CONFIG(c)->fromAddr );
#ifdef EC_DEBUG
            fprintf( stderr, "mail: From: %s\n", 
                     GET_CONFIG(c)->fromAddr );
#endif
        }
        if ( t.to ) {
            fprintf( sendf, "To: %s\n", t.to );
#ifdef EC_DEBUG
            fprintf( stderr, "mail: To: %s\n", t.to );
#endif
        }
        if ( t.cc ) {
            fprintf( sendf, "Cc: %s\n", t.cc );
#ifdef EC_DEBUG
            fprintf( stderr, "mail: Cc: %s\n", t.cc );
#endif
        }
        if ( t.bcc ) {
            fprintf( sendf, "Bcc: %s\n", t.bcc );
#ifdef EC_DEBUG
            fprintf( stderr, "mail: Bcc: %s\n", t.bcc );
#endif
        }
        if ( t.replyTo ) {
            fprintf( sendf, "Reply-To: %s\n", t.replyTo );
#ifdef EC_DEBUG
            fprintf( stderr, "mail: Reply-To: %s\n", t.replyTo );
#endif
        }
        if ( t.subject ) {
            fprintf( sendf, "Subject: %s\n", t.subject );
#ifdef EC_DEBUG
            fprintf( stderr, "mail: Subject: %s\n", t.subject );
#endif
        }
        fprintf( sendf, "\n" );

        if ( t.body ) {
            fputs( t.body, sendf );
            fprintf( sendf, "\n" );
#ifdef EC_DEBUG
            fputs( t.body, stderr );
#endif
        }
       
        if ( t.signature && sig.signature ) {
            char *c = sig.signature;
            
            while ( ( *c == '\r' ) || ( *c == '\n' ) ) {
                c++;
            }
            if ( strncmp( c, "--", 2 ) && strncmp( c, "__", 2 ) ) {
                fprintf( sendf, "\n-- \n" );
            }
            fputs( sig.signature, sendf );
            fprintf( sendf, "\n" );
#ifdef EC_DEBUG
            fputs( sig.signature, stderr );
#endif
        }

        if ( pclose( sendf ) != 0 ) {
            free_Mail( &t );
            fprintf( stderr, "Error ending sendmail exchange\n" );
            continue;
        }

        if ( !strcmp( GET_CONFIG(c)->sendAction, "delete" ) ) {
            dlp_DeleteRecord( dbi->pilot_socket, dbHandle, 0, recID );
        } else if ( !strcmp( GET_CONFIG(c)->sendAction, 
                             "file" ) ) {
#ifdef PILOT_LINK_0_12
             dlp_WriteRecord( dbi->pilot_socket, dbHandle, attr, recID, 3, 
                             pi_buf->data, pi_buf->used, 0);
#else
            dlp_WriteRecord( dbi->pilot_socket, dbHandle, attr, recID, 3, 
                             buffer, size, 0);
#endif
        }
        free_Mail( &t );
    }
   
    /* read in all the existing records on the Palm so that we can
     * spot duplicate mails
     */
    inbox_list = (GList*) gtk_object_get_data(GTK_OBJECT(c), "inbox_list");
    if ( strcmp( GET_CONFIG(c)->receiveAction, "copy" ) == 0 ||
         strcmp( GET_CONFIG(c)->receiveAction, "mirror" ) == 0 ) {
    	for ( i = 0; ; i++ ) {
	    int attr, length, size;
	    recordid_t recID;
	    MailDBRecord *record;

	    /* iterate through records in category 0 (Inbox) ... */
#ifdef PILOT_LINK_0_12
 	    length = dlp_ReadNextRecInCategory( dbi->pilot_socket, dbHandle, 0,
						pi_buf, &recID, 0, &attr);
	    /* pi-dlp.h does not state that the return value is the length...
	     * so it seems safer to read the length from the pi_buf */
	    if (length >= 0)
		    length = pi_buf->used;
#else
	    length = dlp_ReadNextRecInCategory( dbi->pilot_socket, dbHandle, 0,
						buffer, &recID, 0, &size, &attr);
#endif
	    if ( length < 0 ) {
	    	break;
	    }

	    /* ... and store them in the inbox_list */
	    record = (MailDBRecord *) malloc(sizeof(*record) + length);
	    record->recID = recID;
	    record->size = length;
	    record->buffer = ((guchar *) record) + sizeof(*record);
#ifdef PILOT_LINK_0_12
	    memcpy(record->buffer, pi_buf->data, length);
#else
	    memcpy(record->buffer, buffer, length);
#endif
	    inbox_list = g_list_append(inbox_list, record);
	    LOG("storing record %d", recID);
	}
    }

    /* the above loop is likely to change the value of inbox_list so we
     * must put it back
     */
    gtk_object_set_data(GTK_OBJECT(c),"inbox_list",(gpointer)inbox_list);



    if ( GET_CONFIG(c)->mhDirectory ) {
#ifdef EC_DEBUG
        fprintf( stderr, "Reading inbound mail from %s\n",
                 GET_CONFIG(c)->mhDirectory );
#endif
        
        for( i = 1; ; i++ ) {
            int len;
            int l;
            struct Mail t;
            int mhmsg;
            
            t.to = NULL;
            t.from = NULL;
            t.cc = NULL;
            t.bcc = NULL;
            t.subject = NULL;
            t.replyTo = NULL;
            t.sentTo = NULL;
            t.body = NULL;
            t.dated = 0;
	    
            if ( ( mhmsg = openmhmsg( GET_CONFIG(c)->mhDirectory, i ) ) < 0 ) {
                break;
            }
           
#ifdef EC_DEBUG 
            fprintf( stderr, "Processing message %d", i );
#endif
            
            len = 0;
#ifdef PILOT_LINK_0_12
            while ( ( len < pi_buf->allocated ) &&
                    ( ( l = read( mhmsg, (char *)(pi_buf->data+len),
                                  pi_buf->allocated-len ) ) > 0 ) ) {
                 len += l;
             }
            pi_buf->data[len] = 0;
#else
            while ( ( len < sizeof(buffer) ) &&
                    ( ( l = read( mhmsg, (char *)(buffer+len),
                                  sizeof(buffer)-len ) ) > 0 ) ) {
                len += l;
            }
            buffer[len] = 0;
#endif
      
            if ( l < 0 ) {
                fprintf( stderr, "Error processing message %d\n", i );
                break;
            } 

#ifdef PILOT_LINK_0_12
	    if (write_message_to_pilot (c, dbi, dbHandle, pi_buf->data, i)) {
#else
	    if (write_message_to_pilot (c, dbi, dbHandle, buffer, i)) {
#endif
		rec++;
                if ( strcmp( GET_CONFIG(c)->receiveAction, "delete" ) == 0 ) {
                    char filename[1000];
                    sprintf( filename, "%s/%d", GET_CONFIG(c)->mhDirectory, 
                             i );
                    close( mhmsg );
                    if ( unlink( filename ) ) {
                        fprintf( stderr, "Error deleting message %d\n", i );
                        dupe++;
                    }
                    continue;
                } else {
                    dupe++;
                }
	    }

            close( mhmsg );
        }
    }
    
    if ( GET_CONFIG(c)->mboxFile ) {
	FILE *f;
        LOG( "Reading inbound mail from %s", GET_CONFIG(c)->mboxFile );
        f = fopen (GET_CONFIG (c)->mboxFile, "r");
	

	if (f) {
#ifdef PILOT_LINK_0_12
	    fgets (pi_buf->data, pi_buf->allocated - 1, f);
	    while (!feof (f) && strncmp (pi_buf->data, "From ", 5)) {
		fgets (pi_buf->data, pi_buf->allocated - 1, f);
 	    }
#else
	    fgets (buffer, sizeof (buffer) - 1, f);
	    while (!feof (f) && strncmp (buffer, "From ", 5)) {
		fgets (buffer, sizeof (buffer) - 1, f);
	    }
#endif

	    for( i = 1; !feof (f); i++ ) {
		int len;
		char *p;
           
		LOG( "Processing message %d", i );
		len = 0;
#ifdef PILOT_LINK_0_12
		while ( ( len < pi_buf->allocated ) &&
			( ( p = fgets ( (char *)(pi_buf->data+len),
					pi_buf->allocated-len, f ) ) != 0 ) ) {		
#else
		while ( ( len < sizeof(buffer) ) &&
			( ( p = fgets ( (char *)(buffer+len),
					sizeof(buffer)-len, f ) ) != 0 ) ) {
#endif
		    if (!strncmp (p, "From ", 5)) {
			break;
		    } else {
			len += strlen (p);
		    }
		}

#ifdef PILOT_LINK_0_12
		pi_buf->data[len] = 0;
#else
		buffer[len] = 0;
#endif
		len = 0;
		
		if ( len < 0 ) {
		    fprintf( stderr, "Error processing message %d\n", i );
		    break;
		}

#ifdef PILOT_LINK_0_12
		write_message_to_pilot (c, dbi, dbHandle, pi_buf->data, i);
#else
		write_message_to_pilot (c, dbi, dbHandle, buffer, i);
#endif
	    }
	    fclose (f);
	    if ( strcmp( GET_CONFIG(c)->receiveAction, "delete" ) == 0 ) {
		if ( unlink( GET_CONFIG(c)->mboxFile ) ) {
		    fprintf( stderr, "Error deleting mbox file %s\n", 
			     GET_CONFIG(c)->mboxFile);
		}
	    }
	}   
    }

    /* in mirror mode the Palm inbox is a literal copy of the 
     * host's mbox, in this case we must remove any items
     * remaining on the inbox_list
     */
    if ( strcmp( GET_CONFIG(c)->receiveAction, "mirror" ) == 0 ) {
	GList *elem = (GList*) gtk_object_get_data(GTK_OBJECT(c), "inbox_list");
	for (; elem != NULL; elem = elem->next) {
	    MailDBRecord *record = (MailDBRecord *) elem->data;
	    LOG("purging out of date record %d", record->recID);
	    dlp_DeleteRecord( dbi->pilot_socket, dbHandle, 0, record->recID );
	}
    }

    free_MailAppInfo( &tai );
    
    dlp_ResetLastSyncPC( dbi->pilot_socket );
    dlp_CloseDB( dbi->pilot_socket, dbHandle );

#ifdef PILOT_LINK_0_12
    pi_buffer_free (pi_buf);
#endif

    return( 0 );
}

gint copy_from_pilot( GnomePilotConduit *c, GnomePilotDBInfo *dbi ) {
    return( synchronize( c, dbi ) );
}

void
handleFileSelector (GtkWidget *widget, gpointer data)
{
    GtkWidget *fs = data;
    GtkWidget *entry = GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(fs), "entry"));
    const gchar *fname;

    fname = gtk_file_selection_get_filename (GTK_FILE_SELECTION(fs));
    gtk_entry_set_text(GTK_ENTRY(entry), fname);
}


void
createFileSelector (GtkWidget *widget, gpointer data)
{
    GtkWidget *fs;

    fs = gtk_file_selection_new(_("Select an mbox file or an MH directory"));
    gtk_object_set_data(GTK_OBJECT(fs), "entry", (gpointer) data);
    gtk_signal_connect (GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),
		"clicked", GTK_SIGNAL_FUNC(handleFileSelector), fs);

    gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(fs)->ok_button),
		"clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy), (gpointer) fs);
    gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(fs)->cancel_button),
		"clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy), (gpointer) fs);

    gtk_widget_show (fs);
    gtk_grab_add (fs); /* take focus from the settings dialog */
}

static GtkWidget
*createCfgWindow (GnomePilotConduit* conduit)
{
    GtkWidget *vbox, *table;
    GtkWidget *label, *widget;
    GtkWidget *menu, *menuItem;
    GtkWidget *box, *button;

    vbox = gtk_vbox_new(FALSE, GNOME_PAD);

    table = gtk_table_new(2, 5, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 10);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, GNOME_PAD);

    /* send_action option menu */
    label = gtk_label_new(_("Send Action:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    widget = gtk_option_menu_new ();
    menu = gtk_menu_new();
    menuItem = gtk_menu_item_new_with_label (_("Delete from PDA"));
    gtk_widget_show(menuItem);
    gtk_object_set_data(GTK_OBJECT(menuItem), "short", "delete");
    gtk_object_set_data(GTK_OBJECT(widget), "delete", (gpointer) 0);
    gtk_menu_append (GTK_MENU (menu), GTK_WIDGET (menuItem));
    menuItem = gtk_menu_item_new_with_label (_("File on PDA"));
    gtk_widget_show(menuItem);
    gtk_object_set_data(GTK_OBJECT(menuItem), "short", "file");
    gtk_object_set_data(GTK_OBJECT(widget), "file", (gpointer) 1);
    gtk_menu_append (GTK_MENU (menu), GTK_WIDGET (menuItem));
    gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), GTK_WIDGET (menu));
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table), widget, 1, 2, 0, 1);
    gtk_object_set_data(GTK_OBJECT(vbox), "send_action", widget);

    /* from_address entry */
    label = gtk_label_new(_("From:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    widget = gtk_entry_new_with_max_length(128);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), widget, 1, 2, 1, 2);
    gtk_object_set_data(GTK_OBJECT(vbox), "from_address", widget);

    /* sendmail entry */
    label = gtk_label_new(_("Sendmail command:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    widget = gtk_entry_new_with_max_length(128);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), widget, 1, 2, 2, 3);
    gtk_object_set_data(GTK_OBJECT(vbox), "sendmail", widget);

    /* receive_action option menu */
    label = gtk_label_new (_("Receive Action:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    widget = gtk_option_menu_new ();
    menu = gtk_menu_new ();
    menuItem = gtk_menu_item_new_with_label (_("Copy from Inbox"));
    gtk_widget_show(menuItem);
    gtk_object_set_data(GTK_OBJECT(menuItem), "short", "copy");
    gtk_object_set_data(GTK_OBJECT(widget), "copy", (gpointer) 0);
    gtk_menu_append (GTK_MENU (menu), GTK_WIDGET (menuItem));
    menuItem = gtk_menu_item_new_with_label (_("Delete from Inbox"));
    gtk_widget_show(menuItem);
    gtk_object_set_data(GTK_OBJECT(menuItem), "short", "delete");
    gtk_object_set_data(GTK_OBJECT(widget), "delete", (gpointer) 1);
    gtk_menu_append (GTK_MENU (menu), GTK_WIDGET (menuItem));
    menuItem = gtk_menu_item_new_with_label (_("Mirror Inbox"));
    gtk_widget_show(menuItem);
    gtk_object_set_data(GTK_OBJECT(menuItem), "short", "mirror");
    gtk_object_set_data(GTK_OBJECT(widget), "mirror", (gpointer) 2);
    gtk_menu_append (GTK_MENU (menu), GTK_WIDGET (menuItem));
    gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), GTK_WIDGET (menu));
    gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, 3, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), widget, 1, 2, 3, 4);
    gtk_object_set_data(GTK_OBJECT(vbox), "receive_action", widget);

    /* mbox_file entry */
    label = gtk_label_new(_("Copy mail from:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    widget = gtk_entry_new_with_max_length(128);
    button = gtk_button_new_with_label("...");
    gtk_signal_connect(GTK_OBJECT(button), "clicked", 
    	GTK_SIGNAL_FUNC(createFileSelector), widget);
    box = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), widget, TRUE, TRUE, 0);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 4, 5);
    gtk_table_attach_defaults(GTK_TABLE(table), box, 1, 2, 4, 5);
    gtk_object_set_data(GTK_OBJECT(vbox), "mbox_file", widget);

    return vbox;
}

static void
setOptionsCfg(GtkWidget *cfg, ConduitCfg *c)
{
    GtkWidget *send_action, *from_address, *sendmail, *receive_action, *mbox_file;
    GtkWidget *menuItem;
    guint id;

    /* fetch all the controls from the cfg window */
    send_action = gtk_object_get_data(GTK_OBJECT(cfg), "send_action");
    from_address = gtk_object_get_data(GTK_OBJECT(cfg), "from_address");
    sendmail = gtk_object_get_data(GTK_OBJECT(cfg), "sendmail");
    receive_action = gtk_object_get_data(GTK_OBJECT(cfg), "receive_action");
    mbox_file = gtk_object_get_data(GTK_OBJECT(cfg), "mbox_file");

    id = (guint) gtk_object_get_data(GTK_OBJECT(send_action), c->sendAction);
    gtk_option_menu_set_history(GTK_OPTION_MENU(send_action), id);

    gtk_entry_set_text(GTK_ENTRY(from_address), (c->fromAddr ? c->fromAddr : ""));
    gtk_entry_set_text(GTK_ENTRY(sendmail), (c->sendmail ? c->sendmail : ""));

    id = (guint) gtk_object_get_data(GTK_OBJECT(receive_action), c->receiveAction);
    gtk_option_menu_set_history(GTK_OPTION_MENU(receive_action), id);
     
    if (c->mboxFile && 0 != strcmp(c->mboxFile, "")) {
        gtk_entry_set_text(GTK_ENTRY(mbox_file), c->mboxFile);
    } else if (c->mhDirectory) {
    	gtk_entry_set_text(GTK_ENTRY(mbox_file), c->mhDirectory);
    } else {
    	gtk_entry_set_text(GTK_ENTRY(mbox_file), "");
    }
}

static void
readOptionsCfg(GtkWidget *cfg, ConduitCfg *c)
{
    GtkWidget *send_action, *from_address, *sendmail, *receive_action, *mbox_file;
    GtkWidget *menu, *menuItem;
    gchar *str;
    struct stat mboxStat;

    /* fetch all the controls from the cfg window */
    send_action = gtk_object_get_data(GTK_OBJECT(cfg), "send_action");
    from_address = gtk_object_get_data(GTK_OBJECT(cfg), "from_address");
    sendmail = gtk_object_get_data(GTK_OBJECT(cfg), "sendmail");
    receive_action = gtk_object_get_data(GTK_OBJECT(cfg), "receive_action");
    mbox_file = gtk_object_get_data(GTK_OBJECT(cfg), "mbox_file");

    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(send_action));
    menuItem = gtk_menu_get_active(GTK_MENU(menu));
    str = g_strdup(gtk_object_get_data(GTK_OBJECT(menuItem), "short"));
    g_free(c->sendAction);
    c->sendAction = str;

    str = gtk_editable_get_chars(GTK_EDITABLE(from_address), 0, -1);
    if (0 == strcmp(str, "")) {
        g_free(str);
	str = NULL;
    }
    g_free(c->fromAddr);
    c->fromAddr = str;

    str = gtk_editable_get_chars(GTK_EDITABLE(sendmail), 0, -1);
    if (0 == strcmp(str, "")) {
	g_free(str);
	str = NULL;
    }
    g_free(c->sendmail);
    c->sendmail = str;
     
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(receive_action));
    menuItem = gtk_menu_get_active(GTK_MENU(menu));
    str = g_strdup(gtk_object_get_data(GTK_OBJECT(menuItem), "short"));
    g_free(c->receiveAction);
    c->receiveAction = str;
     
    str = gtk_editable_get_chars(GTK_EDITABLE(mbox_file), 0, -1);
    if (0 == strcmp(str, "")) {
        g_free(str);
	str = NULL;
    }
    g_free(c->mboxFile);
    c->mboxFile = NULL;
    g_free(c->mhDirectory);
    c->mhDirectory = NULL;
    if (str) {
	 if (0 == stat(str, &mboxStat) && S_ISDIR(mboxStat.st_mode)) {
	     c->mhDirectory = str;
	 } else {
             c->mboxFile = str;
	 }
     }
}

static gint
create_settings_window (GnomePilotConduit *conduit, GtkWidget *parent, gpointer data)
{
    GtkWidget *cfgWindow;
    cfgWindow = createCfgWindow(conduit);
    gtk_container_add(GTK_CONTAINER(parent),cfgWindow);
    gtk_widget_show_all(cfgWindow);
    
    gtk_object_set_data(GTK_OBJECT(conduit),OBJ_DATA_CONFIG_WINDOW,cfgWindow);
    setOptionsCfg(GET_CONDUIT_WINDOW(conduit),GET_CONFIG(conduit));

    return 0;
}

static void
display_settings (GnomePilotConduit *conduit, gpointer data)
{
    setOptionsCfg(GET_CONDUIT_WINDOW(conduit),GET_CONFIG(conduit));
}

static void
save_settings    (GnomePilotConduit *conduit, gpointer data)
{
    readOptionsCfg(GET_CONDUIT_WINDOW(conduit),GET_CONFIG(conduit));
    save_configuration(GET_CONFIG(conduit));
}

static void
revert_settings  (GnomePilotConduit *conduit, gpointer data)
{
    ConduitCfg *cfg,*cfg2;

    cfg2= GET_OLDCONFIG(conduit);
    cfg = GET_CONFIG(conduit);
    save_configuration(cfg2);
    copy_configuration(cfg,cfg2);
    setOptionsCfg(GET_CONDUIT_WINDOW(conduit),cfg);
}

GnomePilotConduit *conduit_get_gpilot_conduit( guint32 pilotId ) 
{
  GtkObject *retval;
  ConduitCfg *cfg1, *cfg2;

  retval = gnome_pilot_conduit_standard_new("MailDB",0x6d61696c, NULL);

  g_assert(retval != NULL);

  /* conduit signals */
  /*
  gtk_signal_connect(retval, "copy_from_pilot", (GtkSignalFunc)copy_from_pilot ,NULL);
  gtk_signal_connect(retval, "copy_to_pilot", (GtkSignalFunc) ,NULL);
  gtk_signal_connect(retval, "merge_to_pilot", (GtkSignalFunc) ,NULL);
  gtk_signal_connect(retval, "merge_from_pilot", (GtkSignalFunc)synchronize ,NULL);
  */
  gtk_signal_connect(retval, "synchronize", (GtkSignalFunc)synchronize ,NULL);

  /* GUI signals */
  gtk_signal_connect(retval, "create_settings_window", (GtkSignalFunc)create_settings_window ,NULL);
  gtk_signal_connect(retval, "display_settings", (GtkSignalFunc)display_settings ,NULL);
  gtk_signal_connect(retval, "save_settings", (GtkSignalFunc)save_settings ,NULL);
  gtk_signal_connect(retval, "revert_settings", (GtkSignalFunc)revert_settings ,NULL);

  load_configuration(&cfg1, pilotId );
  cfg2 = dupe_configuration(cfg1);
  gtk_object_set_data(retval,OBJ_DATA_CONFIG,(gpointer)cfg1);
  gtk_object_set_data(retval,OBJ_DATA_OLDCONFIG,(gpointer)cfg2);

  return GNOME_PILOT_CONDUIT(retval); 
}

void conduit_destroy_gpilot_conduit( GnomePilotConduit *c ) 
{
  ConduitCfg *cfg1, *cfg2;
  GList *inbox_list, *list;
  
  cfg1 = GET_CONFIG(c);
  cfg2 = GET_OLDCONFIG(c);
  destroy_configuration( &cfg1 );
  destroy_configuration( &cfg2 );

  inbox_list = (GList*) gtk_object_get_data(GTK_OBJECT(c), "inbox_list");
  for (list = inbox_list; list != NULL; list = list->next) {
    free(list->data);
  }
  g_list_free(inbox_list);
  inbox_list = NULL;

  gtk_object_destroy(GTK_OBJECT(c));
}

