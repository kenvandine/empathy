/*
 * empathy-sound.h - Various sound related utility functions.
 * Copyright (C) 2009 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef __EMPATHY_SOUND_H__
#define __EMPATHY_SOUND_H__

#include <gtk/gtk.h>

#include <canberra-gtk.h>

G_BEGIN_DECLS

/* NOTE: Keep this sync with sound_entries in empathy-sound.c */
typedef enum {
	EMPATHY_SOUND_MESSAGE_INCOMING = 0,
	EMPATHY_SOUND_MESSAGE_OUTGOING,
	EMPATHY_SOUND_CONVERSATION_NEW,
	EMPATHY_SOUND_CONTACT_CONNECTED,
	EMPATHY_SOUND_CONTACT_DISCONNECTED,
	EMPATHY_SOUND_ACCOUNT_CONNECTED,
	EMPATHY_SOUND_ACCOUNT_DISCONNECTED,
	EMPATHY_SOUND_PHONE_INCOMING,
	EMPATHY_SOUND_PHONE_OUTGOING,
	EMPATHY_SOUND_PHONE_HANGUP,
	LAST_EMPATHY_SOUND,
} EmpathySound;

gboolean empathy_sound_play (GtkWidget *widget, EmpathySound sound_id);
void empathy_sound_stop (EmpathySound sound_id);

gboolean empathy_sound_start_playing (GtkWidget *widget, EmpathySound sound_id,
    guint timeout_before_replay);

gboolean empathy_sound_play_full (GtkWidget *widget, EmpathySound sound_id,
    ca_finish_callback_t callback, gpointer user_data);

G_END_DECLS

#endif /* #ifndef __EMPATHY_SOUND_H__*/
