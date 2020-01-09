#ifndef __EXPENSE_CONDUIT_H__
#define __EXPENSE_CONDUIT_H__

#include <unistd.h>
#include <pi-expense.h>

#define OBJ_DATA_CONDUIT "conduit_data"
#define OBJ_DATA_CONFIG  "conduit_config"
#define OBJ_DATA_OLDCONFIG  "conduit_oldconfig"
#define OBJ_DATA_CONFIG_WINDOW  "config_window"
#define CONFIG_PREFIX    "/gnome-pilot.d/expense-conduit/Pilot_%u/"

enum ExpenseOutputFormat { 
  eSimpleFormat, eComplexFormat
};

typedef struct ConduitCfg {
  gchar *dir;
  gchar *dateFormat;
  mode_t dirMode;
  mode_t fileMode;
  enum ExpenseOutputFormat outputFormat;

  guint32 pilotId;
  pid_t child;
} ConduitCfg;

typedef struct ConduitData {
  struct ExpenseAppInfo ai;
  struct ExpensePref pref;
  GnomePilotDBInfo *dbi;
} ConduitData;

#define GET_CONDUIT_CFG(s) ((ConduitCfg*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_CONFIG))
#define GET_CONDUIT_OLDCFG(s) ((ConduitCfg*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_OLDCONFIG))
#define GET_CONDUIT_DATA(s) ((ConduitData*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_CONDUIT))
#define GET_CONDUIT_WINDOW(s) ((GtkWidget*)gtk_object_get_data(GTK_OBJECT(s),OBJ_DATA_CONFIG_WINDOW))

#endif /* __EXPENSE_CONDUIT_H__ */
