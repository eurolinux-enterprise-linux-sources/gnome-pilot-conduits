/* $Id: email_conduit.h 230 2002-09-16 17:16:23Z jpr $ */

#ifndef __EMAIL_CONDUIT_H__
#define __EMAIL_CONDUIT_H__

#define OBJ_DATA_CONFIG  "conduit_config"
#define OBJ_DATA_OLDCONFIG  "conduit_oldconfig"
#define OBJ_DATA_CONFIG_WINDOW  "config_window"

typedef struct ConduitCfg {
  gchar *sendmail;
  gchar *fromAddr;
  gchar *sendAction;
  gchar *mhDirectory;
  gchar *mboxFile;
  gchar *receiveAction;
  guint32 pilotId;
  pid_t child;
} ConduitCfg;

typedef struct MailDBRecord {
  int recID;
  int size;
  guchar *buffer;
} MailDBRecord;

#define GET_CONFIG(c) ((ConduitCfg*)(gtk_object_get_data(GTK_OBJECT(c),OBJ_DATA_CONFIG)))
#define GET_OLDCONFIG(c) ((ConduitCfg*)(gtk_object_get_data(GTK_OBJECT(c),OBJ_DATA_OLDCONFIG)))
#define GET_CONDUIT_WINDOW(s) ((GtkWidget*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_CONFIG_WINDOW))

#endif
