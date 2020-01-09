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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gnome.h>

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-file.h>
#include <pi-dlp.h>
#include <pi-version.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard-abs.h>

#include "memo_file_conduit.h"

#define MC_DEBUG

#ifdef MC_DEBUG
#define LOG(args...) g_log (G_LOG_DOMAIN, \
                            G_LOG_LEVEL_MESSAGE, \
                             args)
#else
#define LOG(args...)
#endif

GnomePilotConduit *conduit_get_gpilot_conduit (guint32 pilotId);
void conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit);

#define CONDUIT_VERSION "0.9"

static void 
load_configuration (GnomePilotConduit *conduit, ConduitCfg **c,guint32 pilotId) 
{
	char *prefix;
	char *buf, *key;
	g_return_if_fail (c!=NULL);
	
	prefix = g_strdup_printf (CONFIG_PREFIX, pilotId);
 
	*c = g_new0 (ConduitCfg, 1);
	gnome_config_push_prefix (prefix);
	(*c)->sync_type = GnomePilotConduitSyncTypeCustom; /* this will be reset by capplet */
	(*c)->open_secret = gnome_config_get_bool ("open secret=FALSE"); 
	
	buf = gnome_config_get_string ("file mode=0600");
	(*c)->file_mode =(mode_t)strtol (buf, NULL, 0);
	g_free (buf);

	buf = gnome_config_get_string ("dir mode=0700");
	(*c)->dir_mode =(mode_t)strtol (buf, NULL, 0);
	g_free (buf);

	buf = gnome_config_get_string ("secret mode=0600");
	(*c)->secret_mode =(mode_t)strtol (buf, NULL, 0);
	g_free (buf);

	key = g_strdup_printf ("dir=%s/memo_file", gnome_pilot_conduit_get_base_dir (conduit));
	(*c)->dir = gnome_config_get_string (key);
	g_free (key);
	
	while ((*c)->dir && strlen ((*c)->dir) > 0 && (*c)->dir[strlen ((*c)->dir)-1] == '/') {
		(*c)->dir[strlen ((*c)->dir)-1] = '\0';
	}

	if(mkdir((*c)->dir, (*c)->dir_mode) < 0) { /* Wow, I never though I would
							  use octal in C :) */
		if(errno != EEXIST) {
			g_free ((*c)->dir);
			(*c)->dir = NULL;
		}
	}    
	
	(*c)->ignore_end=gnome_config_get_string ("ignore end");
	(*c)->ignore_start=gnome_config_get_string ("ignore start");
	gnome_config_pop_prefix ();
	g_free (prefix);
	(*c)->pilotId = pilotId;
}

static void 
copy_configuration (ConduitCfg *d, ConduitCfg *c)
{
	g_return_if_fail (c!=NULL);
	g_return_if_fail (d!=NULL);
	d->sync_type=c->sync_type;
	if (d->dir) g_free (d->dir);
	d->dir = g_strdup (c->dir);
	if (d->ignore_start) g_free (d->ignore_start);
	d->ignore_start = g_strdup (c->ignore_start);
	if (d->ignore_end) g_free (d->ignore_end);
	d->ignore_end = g_strdup (c->ignore_end);
	d->file_mode = c->file_mode;
	d->dir_mode = c->dir_mode;
	d->secret_mode = c->secret_mode;
	d->open_secret = c->open_secret;
	d->pilotId = c->pilotId;
}

static ConduitCfg*
dupe_configuration (ConduitCfg *c) 
{
	ConduitCfg *d;
	g_return_val_if_fail (c!=NULL, NULL);
	d = g_new0 (ConduitCfg, 1);
	d->dir=NULL;
	copy_configuration (d, c);
	return d;
}

static void 
destroy_configuration (ConduitCfg **c) 
{
	g_return_if_fail (c!=NULL);
	if ((*c)->dir) g_free ((*c)->dir);
	if ((*c)->ignore_start) g_free ((*c)->ignore_start);
	if ((*c)->ignore_end) g_free ((*c)->ignore_end);
	g_free (*c);
	*c = NULL;
}

static void 
save_configuration (ConduitCfg *c) 
{
	char *prefix;
	char *entry;

	g_return_if_fail (c!=NULL);
	prefix = g_strdup_printf (CONFIG_PREFIX, c->pilotId);

	gnome_config_push_prefix (prefix);
	gnome_config_set_bool ("open secret", c->open_secret); 

	entry = g_strdup_printf ("0%o", c->secret_mode);
	gnome_config_set_string ("secret mode", entry);
	g_free (entry);

	entry = g_strdup_printf ("0%o", c->file_mode);
	gnome_config_set_string ("file mode", entry);
	g_free (entry);

	entry = g_strdup_printf ("0%o", c->dir_mode);
	gnome_config_set_string ("dir mode", entry);
	g_free (entry);

	gnome_config_set_string ("dir", c->dir);
	gnome_config_set_string ("ignore end", c->ignore_end);
	gnome_config_set_string ("ignore start", c->ignore_start);
	gnome_config_pop_prefix ();
	g_free (prefix);

	gnome_config_sync ();
	gnome_config_drop_all ();

}

static IterateData *
new_iterate_data (int _flag, int _archived) 
{
	static IterateData d;
	d.flag = _flag;
	d.archived = _archived;
	d.prev = NULL;
	d.first = NULL;
	return &d;
}

/* from pilot-xfer */
static void 
protect_name (char *d, char *s) 
{
	while (*s) {
		switch (*s) {
		case '/': *(d++) = '='; *(d++) = '2'; *(d++) = 'F'; break;
		case '=': *(d++) = '='; *(d++) = '3'; *(d++) = 'D'; break;
		case '\x0A': *(d++) = '='; *(d++) = '0'; *(d++) = 'A'; break;
		case '\x0D': *(d++) = '='; *(d++) = '0'; *(d++) = 'D'; break;
			/*case ' ': *(d++) = '='; *(d++) = '2'; *(d++) = '0'; break;*/
		default: 
			if (*s < ' ') {
				gchar tmp[6];
				g_snprintf (tmp, 5,"=%2X",(unsigned char)*s);
				*(d++) = tmp[0]; *(d++) = tmp[1]; *(d++) = tmp[2];
			} else
				*(d++) = *s;
			break;
		}
		++s;
	}
	*d = '\0';
}

/** GCompareFunc for finding a record */

static gint 
match_record_id (MemoLocalRecord *a, int *b) 
{
	if (!a) return -1;
	return !(a->local.ID == *b);
}



/* from sync-memodir */
static char * 
newfilename (MemoLocalRecord * r) 
{
	char *name;	
	char buf[4096]; /* Used for the escaped version, so must be 4 x as big */
	char *rec, *end;
	int i;
	
	rec = r->record;
	end = &r->record[r->length];

	/* use first line as file name
	 * but change whitespace chars into '.'
	 */
	while ( rec < end && isspace (*rec) )		/* skip whitespace */
		++rec;

	/* allocate and null out name array */
	name = g_new0 (char, 1024);
	/* Go only upto 1023 characters for file names. That should be enough. 
	   protect_name () can maximally generate 4*n character name
	*/
	for ( i = 0; rec < end && i <1023; ++i, ++rec) {
		if ( *rec == '\n' )
			break;
		else {
			name[i] = *rec;
		}
	}
	
	if ( *name == '\0' ) {
		/* an empty memo, will also be used if first line is \n */
		strcpy ( name, "empty" );
	}
	
	strcpy (buf, name);
	protect_name (name, buf);

	return name;
}

/** 
    generates a pathname for a category 
 */
static gchar *
category_path (int category, GnomePilotConduitStandardAbs *abs) 
{
	char *filename;
	gchar buf[64];
	
	if (category==16) {
		strcpy (buf,"Archived");
	} else {
		protect_name (buf, GET_CONDUIT_DATA (abs)->ai.category.name[category]);
	}
  
	filename =g_strdup_printf ("%s/%s",
				   GET_CONDUIT_CFG (abs)->dir,
				   buf);
	
	return filename;
}
	
static void 
generate_name (MemoLocalRecord *local, GnomePilotConduitStandardAbs *abs) 
{
	struct stat stbuf;
	char *fname;
	char *categorypath;
	int i = 1;
	
	fname = newfilename (local);
	categorypath = category_path (local->local.archived?16:local->category, abs);
	if (local->filename)
		g_free (local->filename);
	local->filename = g_strdup_printf ("%s/%s",
					   categorypath,
					   fname);

	/* file name already exists, tack on a unique number */
	if (stat (local->filename,&stbuf) != 0) {
		g_free (categorypath);
		g_free (fname);
		return;
	}

	for (i = 2; ; ++i) {
		g_free (local->filename);
		local->filename=g_strdup_printf ("%s/%s.%d", 
						categorypath,
						fname, i);
		if (stat (local->filename, &stbuf) != 0) {
			g_free (categorypath);
			g_free (fname);
			return;
		}
	}
}

/** generates a name for a .ids filename
    returns NULL if no name specified for given category (which is a FIXME:)
*/
static gchar *
idfile_name (int cnt, GnomePilotConduitStandardAbs *abs) 
{
	gchar *filename;
	gchar *cpath = NULL;

	cpath = category_path (cnt, abs);
	if (cpath) {
		filename = g_strdup_printf ("%s/.ids", cpath);
		g_free (cpath);
	} else {
		return NULL;
	}

	return filename;
}

/**
   spools a records, gets a unique filename based on the first line and saves it
*/

static void 
spool_foreach (MemoLocalRecord *local, GnomePilotConduitStandardAbs *abs)
{
	int f;
	char *entry;
	char *idfilename;
	mode_t mode;
	
	if (!local || local->length == 0 ||
	   local->local.attr==GnomePilotRecordDeleted) { 
		return;
	}
	
	LOG ("spool_foreach");
	generate_name (local, abs);

	/* Do we store secret records or not ? */
	if (local->local.secret) {
		mode=GET_CONDUIT_CFG (abs)->secret_mode;
	} else {
		mode=GET_CONDUIT_CFG (abs)->file_mode;
	}

	/* Save the contents */
	f = open (local->filename, O_WRONLY|O_CREAT|O_TRUNC, mode);
	if (f==-1) {
		LOG ("Cannot write to %s", local->filename);
	}
	write (f, local->record,(local->length-1 > 0) ? local->length-1 : 0);
	close (f);
	
	/* Write a .ids file entry */
	idfilename = idfile_name (local->category, abs);
	f = open (idfilename, O_WRONLY|O_APPEND|O_CREAT, 0600);
	g_return_if_fail (f!=-1);
	entry = g_strdup_printf ("%lu:%d:%lu;%s\n",
				 local->local.ID,
				 local->local.secret,
				 time (NULL),
				 local->filename);
	write (f, entry, strlen (entry));
	g_free (entry);
	g_free (idfilename);
	close (f);
}


/** obliterates the backup dir, called by spool_records after memos are saved */
static void 
nuke_backup (GnomePilotConduitStandardAbs *abs) 
{
	DIR *dir,*subdir;
	struct dirent *de;
	char *main_dir, *sub_dir, *memo_file;

	g_message ("nuke_backup");
		
	main_dir = g_strdup_printf ("%s.old", GET_CONDUIT_CFG (abs)->dir);
	if ((dir=opendir (main_dir))==NULL) {
		LOG ("nuke_backup cannot open %s", GET_CONDUIT_CFG (abs)->dir);
		return;
	}

	while ((de=readdir (dir))) {
		if (strcmp (de->d_name,".")==0) continue;
		if (strcmp (de->d_name,"..")==0) continue;
		if (strcmp (de->d_name,".categories")==0) {
			char *cat_file = g_strdup_printf ("%s/.categories", main_dir);
			unlink (cat_file);
			g_free (cat_file);
			continue;
		}

		/* backup ensures that GET_CONDUIT_CFG (abs)->dir doesn't end with / */
		sub_dir = g_strdup_printf ("%s.old/%s",
					   GET_CONDUIT_CFG (abs)->dir,
					   de->d_name); 
		if ((subdir=opendir (sub_dir))==NULL) {
			LOG ("nuke_backup cannot open subdir %s", sub_dir);
			g_free (sub_dir);
			continue;
		}
		while ((de=readdir (subdir))) {
			memo_file = g_strdup_printf ("%s/%s", sub_dir, de->d_name);
			unlink (memo_file);
			g_free (memo_file);
		}
		closedir (subdir);
		if (rmdir (sub_dir)<0) {
			LOG ("cannot rmdir %s", sub_dir);
		}
		g_free (sub_dir);
	}
	closedir (dir);
	if (rmdir (main_dir)<0) {
		LOG ("cannot rmdir %s", main_dir);
	}
	g_free (main_dir);
}

/** 
    moves the Memo directory to dir.backup, and recreates dir, used by spool_records
*/
static gboolean
backup_directory (GnomePilotConduitStandardAbs *abs) 
{
	char *filename;
	gchar tmp[FILENAME_MAX];

	strcpy (tmp, GET_CONDUIT_CFG (abs)->dir);
	filename = g_strdup_printf ("%s.old", tmp); 

	if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
		nuke_backup (abs);
	}
	
	LOG ("renaming directory %s to %s", GET_CONDUIT_CFG (abs)->dir, filename);
	if (rename (GET_CONDUIT_CFG (abs)->dir, filename)!=0) {
		LOG ("rename error : %s", g_strerror (errno));
		g_free (filename);
		return FALSE;
	} else {
		mkdir (GET_CONDUIT_CFG (abs)->dir, GET_CONDUIT_CFG (abs)->dir_mode);
		g_free (filename);
		return TRUE;
	}
}

/**
   saves all the records, called by the destructor.
   First it deletes all the files
*/

static void 
spool_records (GnomePilotConduitStandardAbs *abs) 
{
	int f, cnt;
	char *filename;

	g_return_if_fail (GET_CONDUIT_CFG (abs)->dir != NULL);

	filename = g_strdup_printf ("%s/.categories",
				    GET_CONDUIT_CFG (abs)->dir);

	/* backup, in case we die before all is spooled */
	if (backup_directory (abs)==FALSE) {
		/* FIXME */
		LOG ("Backup failed, I really should do something about that...");
	}
	
	f = open (filename, O_WRONLY|O_APPEND|O_CREAT, 0600);
	
	mkdir (GET_CONDUIT_CFG (abs)->dir, GET_CONDUIT_CFG (abs)->dir_mode);
	for (cnt=0;cnt<17;cnt++) {
		char *entry;
		char *categorypath = category_path (cnt, abs);
		mkdir (categorypath, GET_CONDUIT_CFG (abs)->dir_mode);
		entry = g_strdup_printf ("%d;%s\n", cnt, categorypath);
		write (f, entry, strlen (entry));
		g_free (entry);
		g_free (categorypath);
	}
	close (f);
	g_free (filename);
	
	g_list_foreach (GET_CONDUIT_DATA (abs)->records,(GFunc)spool_foreach, abs);
	
	nuke_backup (abs);
};


/**
   free a record, called by destroy_abs for all records
*/

static void 
free_records_foreach (MemoLocalRecord *local, gpointer whatever) 
{
	g_return_if_fail (local != NULL);
	if (local->record)  g_free (local->record);
	if (local->filename) g_free (local->filename);
	local->record=NULL;
	local->filename=NULL;
	g_free (local);
}

/**
   frees and destroys a record
*/

static void 
destroy_records_foreach (MemoLocalRecord *local, gpointer whatever) 
{
	if (!local) return;
	if (local->filename) unlink (local->filename);
}

/**
   marks a record as deleted
*/

static void 
delete_records_foreach (MemoLocalRecord *local, gpointer whatever) 
{
	if (!local) return;
	local->local.attr = GnomePilotRecordDeleted;
}

/**
   deletes a record if attr says so
*/

static void 
purge_records_foreach (MemoLocalRecord *local, gpointer whatever) 
{
	if (!local) return;
	if (local->local.attr == GnomePilotRecordDeleted) {
		destroy_records_foreach (local, whatever);
	}
}

/** 
  foreach function that sets the .next pointer 
*/
static void 
iterate_foreach (MemoLocalRecord *local, IterateData *d) 
{
	gboolean accept;
	if (!local) return; 
	accept = TRUE;
	
	local->next = NULL;

	/* only check if archived = 0 | = 1 */
	if (d->archived>=0) 
		if (d->archived != local->local.archived) accept = FALSE;
	if (d->flag>=0)
		if (d->flag != local->local.attr) accept = FALSE;

	if (local->ignore == TRUE) accept = FALSE;

	if (accept) { 
		if (d->prev) 
			d->prev->next = local;
		else
			d->first = local;
		d->prev = local;
	}
}

static void 
create_deleted_record_foreach (gchar *key, LoadInfo *li, GList **records) 
{
	MemoLocalRecord *local;
	local = g_new0 (MemoLocalRecord, 1);
	local->local.ID = li->id;
	local->local.secret = li->secret;
	local->next = NULL;
	local->category = 0;
	local->local.attr = GnomePilotRecordDeleted;
	local->local.archived = 0;
	local->length = 0;
	local->record = NULL;
	local->filename = g_strdup (key);
	local->ignore = FALSE;

	*records = g_list_append (*records, local);
}

/** 
    fills a record from a file, assuming at least filename is set properly
    archived and secret aren't touched
  */ 
static void 
load_record (GnomePilotConduitStandardAbs *abs, MemoLocalRecord *local) 
{
	FILE *rec;
	struct stat st;
	
	local->record = NULL;
	local->length = 0;
	local->local.attr = GnomePilotRecordNothing;
	/* stat the file and get modtime */
	if (stat (local->filename,&st)<0) {
		LOG ("load_record cannot stat record file \"%s\"", local->filename);
		local->local.attr = GnomePilotRecordDeleted;
		return;
	}
	if (st.st_mtime > local->mtime) { 
		if (local->local.ID == 0)
			local->local.attr = GnomePilotRecordNew;
		else
			local->local.attr = GnomePilotRecordModified;
	}
	
	/* open the file and read the contents */
	if ((rec=fopen (local->filename,"rb"))==NULL) {
		local->local.attr = GnomePilotRecordDeleted; /* FIXME: is this safe ? what if the access is wrong ? */
		return;
	}
	
	fseek (rec, 0L, SEEK_END);
	local->length = ftell (rec)+1;
	rewind (rec);
	local->record = (unsigned char*)g_malloc (local->length);
	fread (local->record, local->length-1, 1, rec);
	local->record[local->length-1]='\0';
	fclose (rec);
}

/** loads the .categories file */
static GHashTable *
load_categories (GnomePilotConduitStandardAbs *abs) 
{
	GHashTable *categories;
	FILE *f;
	char *filename;
	char category_dir[FILENAME_MAX];
	
	LOG ("load_categories");
	categories = g_hash_table_new (g_str_hash, g_str_equal);
	filename = g_strdup_printf ("%s/.categories", GET_CONDUIT_CFG (abs)->dir);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
		return categories;

	if ((f = fopen (filename,"r"))==NULL) return NULL;
	
	while (fgets (category_dir, FILENAME_MAX-1, f)!=NULL) {
		gint cat;
		gchar *ptr;
		
		cat = atol (category_dir);
		ptr = strchr (category_dir,';');
		if (ptr) {
			ptr++;
			ptr[strlen (ptr)-1] = '\0';
			g_hash_table_insert (categories, g_strdup (ptr), GINT_TO_POINTER (cat));
		}
	}
	fclose (f);
	g_free (filename);
	return categories;
}

static gboolean 
ignore_file_name (GnomePilotConduitStandardAbs* abs, gchar* name)
{
	ConduitCfg *cfg;
	if (name[0]=='.') return TRUE;

	cfg=GET_CONDUIT_CFG (abs);
	if (cfg->ignore_start && strlen (cfg->ignore_start)>0 &&
	   strncmp (name, cfg->ignore_start, strlen (cfg->ignore_start))==0)
		return TRUE;
	
	if (cfg->ignore_end && strlen (cfg->ignore_end)>0 &&
	   strcmp (name+strlen (name)-strlen (cfg->ignore_end), cfg->ignore_end)==0)
		return TRUE;
	
	return FALSE;
}

static void 
free_str_foreach (gchar *s, gpointer whatever) 
{
	g_free (s);
}

/** loads the records into the abs structure */
static gboolean
load_records (GnomePilotConduitStandardAbs *abs) 
{
	FILE *idfile;
	DIR *dir;
	struct dirent *de;
	char *filename;
	char entry[FILENAME_MAX];
	char *ptr;
	MemoLocalRecord *local;
	GHashTable *categories;
	int category;
	int total=0, updated=0, deleted=0, newrecs=0;
	
	LOG ("load_records");

	if ((dir=opendir (GET_CONDUIT_CFG (abs)->dir))==NULL) {
		LOG ("load_records cannot open %s", GET_CONDUIT_CFG (abs)->dir);
		return FALSE;
	}
	if ((categories = load_categories (abs))==NULL) {
		LOG ("no categories, no records");
		closedir (dir);
		return FALSE;
	}
	while ((de=readdir (dir))) {
		GHashTable *loadinfo;
		LoadInfo *info;
		
		if (strcmp (de->d_name,".")==0) continue;
		if (strcmp (de->d_name,"..")==0) continue;
		if (strcmp (de->d_name,".categories")==0) continue;
		
		/* first load id:sec:names from the .ids file */
		loadinfo = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

		filename = g_strdup_printf ("%s/%s", GET_CONDUIT_CFG (abs)->dir, de->d_name);
		category = GPOINTER_TO_INT (g_hash_table_lookup (categories, filename));
		g_free (filename);
		if (category<0 || category > 16) category = 0;
		if (category == 16) {
			/* ignore archived dir */
			continue;
		}
		
		filename = g_strdup_printf ("%s/%s/.ids", GET_CONDUIT_CFG (abs)->dir, de->d_name);
		if ((idfile=fopen (filename,"rt"))!=NULL) {
			while (fgets (entry, FILENAME_MAX-1, idfile)!=NULL) {
				gchar *key;
				info = g_new0 (LoadInfo, 1);
				/* mmmm, sscanf... */
				sscanf (entry,"%lu:%d:%lu;", &info->id, &info->secret, &info->mtime);
				ptr = strchr (entry,';'); 
				ptr++;
				
				key = g_strdup (ptr);
				/* Cut the \n */
				key[strlen (key)-1] = '\0';
				
				g_hash_table_insert (loadinfo, key, info);
			}
			fclose (idfile);
		}
		g_free (filename);
		
		/* now check all files in directory */
		{
			DIR *l_dir;
			struct dirent *l_de;
			filename = g_strdup_printf ("%s/%s", GET_CONDUIT_CFG (abs)->dir, de->d_name);
			if ((l_dir=opendir (filename))==NULL) {
				LOG ("load_records cannot open %s", filename);
			} else {
				LOG ("Reading directory %s", filename);
				while ((l_de=readdir (l_dir))) {
					if (ignore_file_name (abs, l_de->d_name)){
						LOG ("Ignoring %s", l_de->d_name);
						continue;
					}
					
					local = g_new0 (MemoLocalRecord, 1);
					local->filename = g_strdup_printf ("%s/%s", filename, l_de->d_name);
					
					if ((info=g_hash_table_lookup (loadinfo, local->filename))!=NULL) {
						local->local.ID = info->id;
						local->local.secret = info->secret;
						local->mtime=info->mtime;
						g_hash_table_remove (loadinfo, local->filename);
					} else {
						local->local.ID = 0;
						local->local.secret = 0;
						local->mtime=0;
					}
					local->local.archived = 0;
					local->category = category;
					local->ignore = FALSE;
					local->record = NULL;
					load_record (abs, local);
					
					GET_CONDUIT_DATA (abs)->records = g_list_append (GET_CONDUIT_DATA (abs)->records, local);
					total++;
					switch (local->local.attr) {
					case GnomePilotRecordDeleted: deleted++; break;
					case GnomePilotRecordModified: updated++;break;
					case GnomePilotRecordNew: newrecs++; break;
					default: break;
					}
					LOG ("Found local file %s, state %d", l_de->d_name, local->local.attr);
				}
				closedir (l_dir);      
			}
			g_free (filename);
		}
		if (g_hash_table_size (loadinfo)>0) {
			deleted += g_hash_table_size (loadinfo);
			g_hash_table_foreach (loadinfo,(GHFunc)create_deleted_record_foreach,&GET_CONDUIT_DATA (abs)->records);
		}
		g_hash_table_destroy (loadinfo);
	}
	closedir (dir);
	g_hash_table_foreach (categories,(GHFunc)free_str_foreach, NULL);
	g_hash_table_destroy (categories);
		
	gnome_pilot_conduit_standard_abs_set_num_local_records (abs, total);
	gnome_pilot_conduit_standard_abs_set_num_updated_local_records (abs, updated);
	gnome_pilot_conduit_standard_abs_set_num_new_local_records (abs, newrecs);
	gnome_pilot_conduit_standard_abs_set_num_deleted_local_records (abs, deleted);
	
	LOG ("records: total = %d updated = %d new = %d deleted = %d", total, updated, newrecs, deleted);

	return TRUE;
}


static gint 
pre_sync (GnomePilotConduit *c, GnomePilotDBInfo *dbi) 
{
	int l;
#ifdef PILOT_LINK_0_12
	pi_buffer_t *pi_buf;
#else
	unsigned char *buf;
#endif  
	g_message ("MemoFile Conduit v %s", CONDUIT_VERSION);

	LOG ("PreSync");

	GET_CONDUIT_DATA (c)->dbi=dbi;
  
#ifdef PILOT_LINK_0_12
	pi_buf = pi_buffer_new (0xffff);
	if ((l=dlp_ReadAppBlock (dbi->pilot_socket, dbi->db_handle, 0,0xffff, pi_buf))<0) {	
#else
	buf = (unsigned char*)g_malloc (0xffff);
	if ((l=dlp_ReadAppBlock (dbi->pilot_socket, dbi->db_handle, 0,(unsigned char *)buf, 0xffff))<0) {
#endif
		LOG ("dlp_ReadAppBlock (...) failed");
#ifdef PILOT_LINK_0_12
		pi_buffer_free (pi_buf);
#else
		g_free (buf);
#endif
		return -1;
	}

#ifdef PILOT_LINK_0_12
	unpack_MemoAppInfo (&(GET_CONDUIT_DATA (c)->ai), pi_buf->data, l);
	pi_buffer_free (pi_buf);
#else
	unpack_MemoAppInfo (&(GET_CONDUIT_DATA (c)->ai), buf, l);
	g_free (buf);
#endif

	if (GET_CONDUIT_CFG (c)->dir==NULL || *(GET_CONDUIT_CFG (c)->dir) == '\0') {
		return -1;
	}

	if (!load_records ((GnomePilotConduitStandardAbs*)c))
		return -1;

	/* If local store is empty force the slow sync. */
	if (g_list_length (GET_CONDUIT_DATA (c)->records)==0){
		gnome_pilot_conduit_standard_set_slow ((GnomePilotConduitStandard*)c, TRUE);
	}
	return 0;
}

static gint
match_record	(GnomePilotConduitStandardAbs *abs,
		 MemoLocalRecord **local,
		 PilotRecord *remote,
		 gpointer data)
{
	GList *tmp;
	LOG ("MatchRecord"); 

	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);

	tmp = g_list_find_custom (GET_CONDUIT_DATA (abs)->records,(gpointer)&remote->ID,(GCompareFunc)match_record_id);
	if (tmp==NULL) 
		*local = NULL;
	else {
		*local = tmp->data;
	}
	return 0;
}

static gint
free_match	(GnomePilotConduitStandardAbs *abs,
		 MemoLocalRecord **local,
		 gpointer data)
{
	LOG ("FreeMatch");

	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (*local!=NULL,-1);
	
	*local = NULL;
	return 0;
}

static gint
archive_local (GnomePilotConduitStandardAbs *abs,
	       MemoLocalRecord *local,
	       gpointer data)
{
	LOG ("ArchiveLocal");
	g_return_val_if_fail (local!=NULL,-1);
	local->local.archived = 1; 
	local->local.attr=GnomePilotRecordNothing;
	return 0;
}


static gint
store_remote (GnomePilotConduitStandardAbs *abs,
	      PilotRecord *remote,
	      gpointer data)
{
	MemoLocalRecord *local;
	GList *tmp;
	ConduitData *cd;

	LOG ("StoreRemote");
	
	g_return_val_if_fail (remote!=NULL,-1);
	
	cd = GET_CONDUIT_DATA (abs);
	tmp = g_list_find_custom (cd->records,(gpointer)&remote->ID,(GCompareFunc)match_record_id);
  
	if (tmp==NULL) {
		/* new record */
		local = g_new0 (MemoLocalRecord, 1);
		cd->records = g_list_append (cd->records, local);
		
	} else {
		local = tmp->data;
		if (local->record) {
			g_free (local->record);
			local->record=NULL;
		}
	}

	local->local.ID = remote->ID; 
	local->local.attr = remote->attr;
	local->local.archived = remote->archived;
	local->local.secret = remote->secret;
	local->length = remote->length;
	local->category = remote->category; 
	local->ignore = FALSE;
	local->record = NULL;
	if (local->length) {
		/* paranoia check */
		if (remote->record==NULL) {
			LOG ("record with NULL contents encountered");
			local->record = NULL;
			local->length = 0;
		} else {
			local->record = (unsigned char*)g_malloc (local->length);
			memcpy (local->record, remote->record, local->length);
		}
	}
	
	return 0;
}

static gint
archive_remote (GnomePilotConduitStandardAbs *abs,
		MemoLocalRecord *local,
		PilotRecord *remote,
		gpointer data)
{
	LOG ("ArchiveRemote");
	g_return_val_if_fail (remote!=NULL,-1);
	remote->archived=TRUE;
	remote->attr = GnomePilotRecordNothing;
	store_remote (abs, remote, data);
	return 0;
}

static gint
iterate (GnomePilotConduitStandardAbs *abs,
	 MemoLocalRecord **local,
	 gpointer data)
{
	LOG ("Iterate");
	g_return_val_if_fail (local!=NULL,-1);
	if (!*local) {
		/* setup the links */
		IterateData *d;
		d = new_iterate_data (-1,-1);
		g_list_foreach (GET_CONDUIT_DATA (abs)->records,(GFunc)iterate_foreach, d);
		*local = d->first;
	} else {
		*local = (*local)->next;
	}
	if (*local==NULL) return 0;
	else return 1;
}

static gint
iterate_specific (GnomePilotConduitStandardAbs *abs,
		  MemoLocalRecord **local,
		  gint flag,
		  gint archived,
		  gpointer data)
{
	LOG ("IterateSpecific, *local %s NULL,    flag = %d, archived = %d",
	    (*local)==NULL?"==":"!=", flag, archived);
	g_return_val_if_fail (local!=NULL,-1);
	if (! (*local)) {
		/* setup the links */
		IterateData *d;
		d = new_iterate_data (flag, archived);
		if (g_list_length (GET_CONDUIT_DATA (abs)->records)>0) {
			g_list_foreach (GET_CONDUIT_DATA (abs)->records,(GFunc)iterate_foreach, d);
			(*local) = d->first;
		} else {
			(*local)=NULL;
		}
	} else {
		(*local) = (*local)->next;
	}
	if ((*local) == NULL) return 0;
	else return 1;
}

static gint
purge (GnomePilotConduitStandardAbs *abs,
       gpointer data)
{
	LOG ("Purge");

	g_list_foreach (GET_CONDUIT_DATA (abs)->records,
		       (GFunc)purge_records_foreach,
		       GET_CONDUIT_DATA (abs)->records);
	spool_records (abs);
	
	return 0;
}

static gint
set_status (GnomePilotConduitStandardAbs *abs,
	    MemoLocalRecord *local,
	    gint status,
	    gpointer data)
{
	LOG ("SetStatus %d", status);
	g_return_val_if_fail (local!=NULL,-1);
	local->local.attr = status; 
	if (status==GnomePilotRecordDeleted) local->ignore=TRUE;
	return 0;
}

static gint
set_pilot_id (GnomePilotConduitStandardAbs *abs,
	      MemoLocalRecord *local,
	      guint32 ID,
	      gpointer data)
{
	LOG ("SetPilotId, ID = %u", ID);
	g_return_val_if_fail (local!=NULL,-1);
	local->local.ID = ID; 
	return 0;

}
static gint
compare (GnomePilotConduitStandardAbs *abs,
	 MemoLocalRecord *local,
	 PilotRecord *remote,
	 gpointer data)
{
	LOG ("Compare");
	
	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);
	if (local->record==NULL || remote->record == NULL) return -1;
	return strncmp (local->record, remote->record, local->length);
}

static gint
compare_backup (GnomePilotConduitStandardAbs *abs,
		MemoLocalRecord *local,
		PilotRecord *remote,
		gpointer data)
{
	LOG ("CompareBackup");
	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);
	if (local->record==NULL || remote->record == NULL) return -1;

	return -1;
}
static gint
free_transmit (GnomePilotConduitStandardAbs *abs,
	       MemoLocalRecord *local,
	       PilotRecord **remote,
	       gpointer data)
{
	LOG ("FreeTransmit");
	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);
	g_return_val_if_fail (*remote!=NULL,-1);
  
	if ((*remote)->record) g_free ((*remote)->record); 
	*remote = NULL;
	return 0;
}

static gint
delete_all (GnomePilotConduitStandardAbs *abs,
	    gpointer data)
{
	LOG ("DeleteAll");
	g_list_foreach (GET_CONDUIT_DATA (abs)->records,
		       (GFunc)delete_records_foreach,
		       NULL);
	return 0;
}

static gint
transmit (GnomePilotConduitStandardAbs *abs,
	  MemoLocalRecord *local,
	  PilotRecord **remote,
	  gpointer data)
{
	static PilotRecord p;
	LOG ("Transmit, local %s NULL", local==NULL?"==":"!=");
	
	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);
	
	p.record = NULL;

	p.ID = local->local.ID;
	p.attr = local->local.attr;
	p.archived = local->local.archived;
	p.secret = local->local.secret;
	p.length = local->length;
	p.category = local->category;
	if (p.length) {
		p.record = (unsigned char*)g_malloc (p.length);
		memcpy (p.record, local->record, p.length);
	}
	*remote = &p;
	return 0;
}



typedef struct _FieldInfo FieldInfo;
struct _FieldInfo
{
	gchar    *name;
	gchar    *label_data;
	gchar    *obj_data;
	gpointer  insert_func;
};



static void
insert_ignore_space (GtkEditable    *editable, const gchar    *text,
		     gint len, gint *position, void *data)
{
	gint i;
	const gchar *curname;

	curname = gtk_entry_get_text (GTK_ENTRY (editable));
	if (*curname == '\0' && len > 0) {
		if (isspace (text[0])) {
			gtk_signal_emit_stop_by_name (GTK_OBJECT (editable), "insert_text");
			return;
		}
	} else { 
		for (i=0; i<len; i++) {
			if (isspace (text[i])) {
				gtk_signal_emit_stop_by_name (GTK_OBJECT (editable),"insert_text");
				return;
			}
		}
	}
}


static void
insert_numeric_callback (GtkEditable    *editable, const gchar    *text,
			 gint len, gint *position, void *data)
{
	gint i;

	for (i=0; i<len; i++) {
		if (!isdigit (text[i])) {
			gtk_signal_emit_stop_by_name (GTK_OBJECT (editable), "insert_text");
			return;
		}
	}
}

/* so I like structures. */
static FieldInfo fields[] =
{ { N_("Memos directory:"), NULL,"dir", insert_ignore_space},
  { N_("Ignore start:"), NULL,"ignore_start", insert_ignore_space},
  { N_("Ignore end:"), NULL,"ignore_end", insert_ignore_space},
  { N_("Directory mode:"), NULL,"dir_mode", insert_numeric_callback},
  { N_("Files mode:"), NULL,"file_mode", insert_numeric_callback},
  { N_("Secret files mode:"),"secret_label","secret_mode", insert_numeric_callback},
  { NULL, NULL, NULL}
};

static void 
secret_toggled_cb (GtkWidget *widget, gpointer data) 
{
	GnomePilotConduit *conduit = (GnomePilotConduit*)data;
	ConduitCfg * curState = GET_CONDUIT_CFG (conduit);
	GtkWidget * main_widget = GET_CONDUIT_WINDOW (conduit);
	curState->open_secret = GTK_TOGGLE_BUTTON (widget)->active;
	gtk_widget_set_sensitive (gtk_object_get_data (GTK_OBJECT (main_widget),"secret_mode"), curState->open_secret);
	gtk_widget_set_sensitive (gtk_object_get_data (GTK_OBJECT (main_widget),"secret_label"), curState->open_secret);
}


static GtkWidget
*createCfgWindow (GnomePilotConduit* conduit, ConduitCfg *cfg)
{
	GtkWidget *vbox, *table;
	GtkWidget *entry, *label;
	GtkWidget *button;
	int i, count=0;

	/* how many fields do we have */
	while (fields[count].name!=0) count++;

	vbox = gtk_vbox_new (FALSE, GNOME_PAD);

	table = gtk_table_new (count, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table), 10);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, GNOME_PAD);

	for (i=0;i<count;i++) {
		label = gtk_label_new (_(fields[i].name));
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
                gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 2, i, i+1);
		if (fields[i].label_data!=NULL) {
			gtk_object_set_data (GTK_OBJECT (vbox), fields[i].label_data, label);
		}
		entry = gtk_entry_new_with_max_length (128);
		gtk_object_set_data (GTK_OBJECT (vbox), fields[i].obj_data, entry);
		gtk_table_attach (GTK_TABLE (table), entry, 2, 3, i, i+1, 0, 0, 0, 0);
		gtk_signal_connect (GTK_OBJECT (entry), "insert_text",
				   GTK_SIGNAL_FUNC (fields[i].insert_func),
				   NULL);
	}
	
	button = gtk_check_button_new ();

	gtk_object_set_data (GTK_OBJECT (vbox),"secret_on", button);
	gtk_signal_connect (GTK_OBJECT (button), "toggled",
			   GTK_SIGNAL_FUNC (secret_toggled_cb),
			   conduit);
	gtk_table_attach (GTK_TABLE (table), button, 0, 1, 5, 6, 0, 0, 0, 0); /* 5, 6 is badly hard-coded here. */

	
	return vbox;
}


static void
setOptionsCfg (GtkWidget *pilotcfg, ConduitCfg *state)
{
        gchar buf[8];
	GtkWidget *dir,*ignore_end,*ignore_start,*dir_mode;
	GtkWidget *secret_button, *file_mode,*secret_mode;

	dir = gtk_object_get_data (GTK_OBJECT (pilotcfg), "dir");
	ignore_end  = gtk_object_get_data (GTK_OBJECT (pilotcfg), "ignore_end");
	ignore_start = gtk_object_get_data (GTK_OBJECT (pilotcfg), "ignore_start");
	dir_mode  = gtk_object_get_data (GTK_OBJECT (pilotcfg), "dir_mode");
	file_mode = gtk_object_get_data (GTK_OBJECT (pilotcfg), "file_mode");
	secret_mode = gtk_object_get_data (GTK_OBJECT (pilotcfg), "secret_mode");
	secret_button = gtk_object_get_data (GTK_OBJECT (pilotcfg),"secret_on");
	
	gtk_entry_set_text (GTK_ENTRY (dir), state->dir);
	if (state->ignore_start) gtk_entry_set_text (GTK_ENTRY (ignore_start), state->ignore_start);
	if (state->ignore_end) gtk_entry_set_text (GTK_ENTRY (ignore_end), state->ignore_end);
	g_snprintf (buf, 7,"0%o", state->dir_mode);
	gtk_entry_set_text (GTK_ENTRY (dir_mode), buf);
	g_snprintf (buf, 7,"0%o", state->file_mode);
	gtk_entry_set_text (GTK_ENTRY (file_mode), buf);
	g_snprintf (buf, 7,"0%o", state->secret_mode);
	gtk_entry_set_text (GTK_ENTRY (secret_mode), buf);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (secret_button), state->open_secret);
	gtk_widget_set_sensitive (gtk_object_get_data (GTK_OBJECT (pilotcfg),"secret_mode"), state->open_secret);
	gtk_widget_set_sensitive (gtk_object_get_data (GTK_OBJECT (pilotcfg),"secret_label"), state->open_secret);


}


static void
readOptionsCfg (GtkWidget *pilotcfg, ConduitCfg *state)
{
	GtkWidget *entry; 

	entry  = gtk_object_get_data (GTK_OBJECT (pilotcfg), "dir");
	if (state->dir) g_free (state->dir);
	state->dir = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

	while (state->dir && strlen (state->dir) > 0 && state->dir[strlen (state->dir)-1] == '/') {
		state->dir[strlen (state->dir)-1] = '\0';
	}

	entry  = gtk_object_get_data (GTK_OBJECT (pilotcfg), "ignore_start");
	if (state->ignore_start) g_free (state->ignore_start);
	state->ignore_start = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

	entry  = gtk_object_get_data (GTK_OBJECT (pilotcfg), "ignore_end");
	if (state->ignore_end) g_free (state->ignore_end);
	state->ignore_end = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

	entry  = gtk_object_get_data (GTK_OBJECT (pilotcfg), "dir_mode");
	state->dir_mode = strtol (gtk_entry_get_text (GTK_ENTRY (entry)), NULL, 0);

	entry  = gtk_object_get_data (GTK_OBJECT (pilotcfg), "file_mode");
	state->file_mode = strtol (gtk_entry_get_text (GTK_ENTRY (entry)), NULL, 0);

	entry  = gtk_object_get_data (GTK_OBJECT (pilotcfg), "secret_mode");
	state->secret_mode = strtol (gtk_entry_get_text (GTK_ENTRY (entry)), NULL, 0);
}


static gint
create_settings_window (GnomePilotConduit *conduit, GtkWidget *parent, gpointer data)
{
	GtkWidget *cfgWindow;
	LOG ("create_settings_window");

	cfgWindow = createCfgWindow (conduit, GET_CONDUIT_CFG (conduit));
	gtk_container_add (GTK_CONTAINER (parent), cfgWindow);
	gtk_widget_show_all (cfgWindow);

	gtk_object_set_data (GTK_OBJECT (conduit), OBJ_DATA_CONFIG_WINDOW, cfgWindow);
	setOptionsCfg (GET_CONDUIT_WINDOW (conduit), GET_CONDUIT_CFG (conduit));
	return 0;
}
static void
display_settings (GnomePilotConduit *conduit, gpointer data)
{
	LOG ("display_settings");
	setOptionsCfg (GET_CONDUIT_WINDOW (conduit), GET_CONDUIT_CFG (conduit));
}

static void
save_settings    (GnomePilotConduit *conduit, gpointer data)
{
	LOG ("save_settings");
	readOptionsCfg (GET_CONDUIT_WINDOW (conduit), GET_CONDUIT_CFG (conduit));
	save_configuration (GET_CONDUIT_CFG (conduit));
}

static void
revert_settings  (GnomePilotConduit *conduit, gpointer data)
{
	ConduitCfg *cfg,*cfg2;
	LOG ("revert_settings");
	cfg2= GET_CONDUIT_OLDCFG (conduit);
	cfg = GET_CONDUIT_CFG (conduit);
	save_configuration (cfg2);
	copy_configuration (cfg, cfg2);
	setOptionsCfg (GET_CONDUIT_WINDOW (conduit), cfg);
}

GnomePilotConduit *
conduit_get_gpilot_conduit (guint32 pilotId)
{
	GtkObject *retval;

	ConduitData *cd = g_new0 (ConduitData, 1);
	ConduitCfg *cfg, *cfg2;
	
	cd->records=NULL;
  
	retval = gnome_pilot_conduit_standard_abs_new ("MemoDB", 0x6d656d6f);
	g_assert (retval != NULL);
	
	LOG ("creating memo_file conduit");

	g_assert (retval != NULL);
	gtk_signal_connect (retval, "match_record", (GtkSignalFunc) match_record, NULL);
	gtk_signal_connect (retval, "free_match", (GtkSignalFunc) free_match, NULL);
	gtk_signal_connect (retval, "archive_local", (GtkSignalFunc) archive_local, NULL);
	gtk_signal_connect (retval, "archive_remote", (GtkSignalFunc) archive_remote, NULL);
	gtk_signal_connect (retval, "store_remote", (GtkSignalFunc) store_remote, NULL);
	gtk_signal_connect (retval, "iterate", (GtkSignalFunc) iterate, NULL);
	gtk_signal_connect (retval, "iterate_specific", (GtkSignalFunc) iterate_specific, NULL);
	gtk_signal_connect (retval, "purge", (GtkSignalFunc) purge, NULL);
	gtk_signal_connect (retval, "set_status", (GtkSignalFunc) set_status, NULL);
	gtk_signal_connect (retval, "set_pilot_id", (GtkSignalFunc) set_pilot_id, NULL);
	gtk_signal_connect (retval, "compare", (GtkSignalFunc) compare, NULL);
	gtk_signal_connect (retval, "compare_backup", (GtkSignalFunc) compare_backup, NULL);
	gtk_signal_connect (retval, "free_transmit", (GtkSignalFunc) free_transmit, NULL);
	gtk_signal_connect (retval, "delete_all", (GtkSignalFunc) delete_all, NULL);
	gtk_signal_connect (retval, "transmit", (GtkSignalFunc) transmit, NULL);
	gtk_signal_connect (retval, "pre_sync", (GtkSignalFunc) pre_sync, NULL);

	gtk_signal_connect (retval, "create_settings_window", (GtkSignalFunc) create_settings_window, NULL);
	gtk_signal_connect (retval, "display_settings", (GtkSignalFunc) display_settings, NULL);
	gtk_signal_connect (retval, "save_settings", (GtkSignalFunc) save_settings, NULL);
	gtk_signal_connect (retval, "revert_settings", (GtkSignalFunc) revert_settings, NULL);

	load_configuration (GNOME_PILOT_CONDUIT (retval), &cfg, pilotId);
	cfg2 = dupe_configuration (cfg);
	gtk_object_set_data (GTK_OBJECT (retval), OBJ_DATA_CONFIG, cfg);
	gtk_object_set_data (GTK_OBJECT (retval), OBJ_DATA_OLDCONFIG, cfg2);
	gtk_object_set_data (GTK_OBJECT (retval), OBJ_DATA_CONDUIT, cd);

	if (cfg->dir==NULL) {
		g_warning (_("No dir specified. Please run memo_file conduit capplet first."));
		gnome_pilot_conduit_send_error (GNOME_PILOT_CONDUIT (retval),
					       _("No dir specified. Please run memo_file conduit capplet first."));
		/* FIXME: the following is probably the right way to go, 
		   but with current state it doesn't work. (capplet wouldn't start up)
                destroy_configuration (&cfg);
		return NULL;
		*/
	} 
	
	if (cfg->open_secret) gnome_pilot_conduit_standard_abs_set_db_open_mode (GNOME_PILOT_CONDUIT_STANDARD_ABS (retval),
									      dlpOpenReadWrite|dlpOpenSecret);
	return GNOME_PILOT_CONDUIT (retval);
}

void
conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit)
{
	ConduitData *cd=GET_CONDUIT_DATA (conduit);
	ConduitCfg  *cfg=GET_CONDUIT_CFG (conduit);
	ConduitCfg  *cfg2=GET_CONDUIT_OLDCFG (conduit);
	LOG ("destroying memo_file conduit");

	g_list_foreach (cd->records,(GFunc)free_records_foreach, NULL);
	g_list_free (cd->records);
	g_free (cd);

	destroy_configuration (&cfg);
	if (cfg2) {
		destroy_configuration (&cfg2);
	}
}


