/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_ACCOUNT_CHOOSER_H__
#define __EMPATHY_ACCOUNT_CHOOSER_H__

#include <gtk/gtk.h>

#include <libempathy/empathy-account.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_ACCOUNT_CHOOSER (empathy_account_chooser_get_type ())
#define EMPATHY_ACCOUNT_CHOOSER(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_ACCOUNT_CHOOSER, EmpathyAccountChooser))
#define EMPATHY_ACCOUNT_CHOOSER_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_ACCOUNT_CHOOSER, EmpathyAccountChooserClass))
#define EMPATHY_IS_ACCOUNT_CHOOSER(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_ACCOUNT_CHOOSER))
#define EMPATHY_IS_ACCOUNT_CHOOSER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_ACCOUNT_CHOOSER))
#define EMPATHY_ACCOUNT_CHOOSER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_ACCOUNT_CHOOSER, EmpathyAccountChooserClass))

typedef gboolean (* EmpathyAccountChooserFilterFunc) (EmpathyAccount *account,
						      gpointer   user_data);


typedef struct _EmpathyAccountChooser      EmpathyAccountChooser;
typedef struct _EmpathyAccountChooserClass EmpathyAccountChooserClass;

struct _EmpathyAccountChooser {
	GtkComboBox parent;

	/*<private>*/
	gpointer priv;
};

struct _EmpathyAccountChooserClass {
	GtkComboBoxClass parent_class;
};

GType          empathy_account_chooser_get_type           (void) G_GNUC_CONST;
GtkWidget *    empathy_account_chooser_new                (void);
EmpathyAccount *    empathy_account_chooser_dup_account        (EmpathyAccountChooser *chooser);
TpConnection * empathy_account_chooser_get_connection     (EmpathyAccountChooser *chooser);
gboolean       empathy_account_chooser_set_account        (EmpathyAccountChooser *chooser,
							   EmpathyAccount        *account);
gboolean       empathy_account_chooser_get_has_all_option (EmpathyAccountChooser *chooser);
void           empathy_account_chooser_set_has_all_option (EmpathyAccountChooser *chooser,
							   gboolean               has_all_option);
void           empathy_account_chooser_set_filter         (EmpathyAccountChooser *chooser,
							   EmpathyAccountChooserFilterFunc filter,
							   gpointer               user_data);
gboolean       empathy_account_chooser_filter_is_connected (EmpathyAccount       *account,
							   gpointer               user_data);

G_END_DECLS

#endif /* __EMPATHY_ACCOUNT_CHOOSER_H__ */

