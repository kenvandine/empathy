/*
 * Copyright (C) 2008 Collabora Ltd.
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
 * Authors: Jonny Lamb <jonny.lamb@collabora.co.uk>
 */

#include <gtk/gtk.h>

#ifndef __EMPATHY_IMPORT_DIALOG_H__
#define __EMPATHY_IMPORT_DIALOG_H__

G_BEGIN_DECLS

typedef struct
{
  /* Table mapping CM param string to a GValue */
  GHashTable *settings;
  /* The profile to use for this account */
  McProfile *profile;
  /* The name of the account import source */
  gchar *source;
} EmpathyImportAccountData;

EmpathyImportAccountData *empathy_import_account_data_new (const gchar *source);
void empathy_import_account_data_free (EmpathyImportAccountData *data);
void empathy_import_dialog_show (GtkWindow *parent, gboolean warning);

G_END_DECLS

#endif /* __EMPATHY_IMPORT_DIALOG_H__ */
