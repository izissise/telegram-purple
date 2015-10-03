/*
 This file is part of telegram-purple
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 
 Copyright Matthias Jentsch 2014-2015
 */

#include <glib.h>
#include <tgl.h>
#include <tgl-binlog.h>
#include <tgl-methods-in.h>
#include <tgl-queries.h>

#define _(m) m

#include "msglog.h"
#include "tgp-request.h"
#include "telegram-base.h"
#include "telegram-purple.h"
#include "tgp-2prpl.h"
#include "tgp-utils.h"

static void code_receive_result (struct tgl_state *TLS, void *extra, int success, struct tgl_user *U) {
  struct request_password_data *data = extra;
  if (success) {
    ((void (*) (struct tgl_state *TLS)) data->callback) (TLS);
  } else {
    debug ("bad code, retrying...");
    request_code (TLS, data->callback);
  }
  free (data);
}

void request_code_entered (gpointer extra, const gchar *code) {
  struct request_password_data *data = extra;
  connection_data *conn = data->TLS->ev_base;
  char const *username = purple_account_get_username (conn->pa);
  char *stripped = g_strstrip (purple_markup_strip_html (code));
  debug ("sending code: '%s'\n", stripped);
  
  tgl_do_send_code_result (data->TLS, username, (int)strlen (username), conn->hash, (int)strlen (conn->hash),
      stripped, (int)strlen (stripped), code_receive_result, data);
  g_free (stripped);
}

static void request_code_canceled (gpointer extra) {
  struct request_password_data *data = extra;
  connection_data *conn = data->TLS->ev_base;
  
  purple_connection_error_reason (conn->gc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, "registration canceled");
  free (data);
}

void request_code (struct tgl_state *TLS, void (*callback) (struct tgl_state *TLS)) {
  debug ("Client is not registered, registering...");
  connection_data *conn = TLS->ev_base;
  
  struct request_password_data *data = malloc (sizeof(struct request_password_data));
  data->TLS = TLS;
  data->arg = conn;
  data->callback = callback;

  if (purple_account_get_bool (tg_get_acc (TLS), "compat-verification", 0) ||
      ! purple_request_input (conn->gc, "Telegram Code", "Enter Telegram Code", "Telegram wants to verify your "
        "identity, please enter the code, that you have received via SMS.", NULL, 0, 0, "code", "OK",
        G_CALLBACK(request_code_entered), "Cancel", G_CALLBACK(request_code_canceled), conn->pa, NULL, NULL, data)) {
    
    // purple request API is not available, so we create a new conversation (the Telegram system
    // account "7770000") to prompt the user for the code
    conn->in_fallback_chat = 1;
    purple_connection_set_state (conn->gc, PURPLE_CONNECTED);
    PurpleConversation *conv = purple_conversation_new (PURPLE_CONV_TYPE_IM, conn->pa, "777000");
    purple_conversation_write (conv, "777000", "What is your SMS verification code?",
        PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM, 0);
    free (data);
  }
}

static void code_auth_receive_result (struct tgl_state *TLS, void *extra, int success, struct tgl_user *U) {
  if (! success) {
    debug ("Bad code...");
    request_name_and_code (TLS);
  } else {
    telegram_export_authorization (TLS);
  }
}

void request_name_code_entered (PurpleConnection* gc, PurpleRequestFields* fields) {
  connection_data *conn = purple_connection_get_protocol_data (gc);
  struct tgl_state *TLS = conn->TLS;
  char const *username = purple_account_get_username (conn->pa);
  
  char* first = g_strstrip (g_strdup (purple_request_fields_get_string (fields, "first_name")));
  char* last = g_strstrip (g_strdup (purple_request_fields_get_string (fields, "last_name")));
  char* code = g_strstrip (g_strdup (purple_request_fields_get_string (fields, "code")));
  if (!first || !last || !code) {
    request_name_and_code (TLS);
    return;
  }
  tgl_do_send_code_result_auth (TLS, username, (int)strlen(username), conn->hash,
      (int)strlen (conn->hash), code, (int)strlen (code), first,
      (int)strlen (first), last, (int)strlen (last),
      code_auth_receive_result, NULL);
  g_free (first);
  g_free (last);
  g_free (code);
}

void request_name_and_code (struct tgl_state *TLS) {
  debug ("Phone is not registered, registering...");
  
  connection_data *conn = TLS->ev_base;
  
  PurpleRequestFields* fields = purple_request_fields_new ();
  PurpleRequestField* field = 0;
  
  PurpleRequestFieldGroup* group = purple_request_field_group_new ("Registration");
  field = purple_request_field_string_new ("first_name", "First Name", "", 0);
  purple_request_field_group_add_field (group, field);
  field = purple_request_field_string_new ("last_name", "Last Name", "", 0);
  purple_request_field_group_add_field (group, field);
  purple_request_fields_add_group (fields, group);
  
  group = purple_request_field_group_new ("Authorization");
  field = purple_request_field_string_new ("code", "Telegram Code", "", 0);
  purple_request_field_group_add_field (group, field);
  purple_request_fields_add_group (fields, group);
  
  if (!purple_request_fields (conn->gc, "Register", "Please register your phone number.", NULL, fields, "Ok",
        G_CALLBACK(request_name_code_entered), "Cancel", NULL, conn->pa, NULL, NULL, conn->gc)) {
    // purple_request API not available
    const char *error = "Phone number is not registered, please register your phone on a different client.";
    purple_connection_error_reason (conn->gc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, error);
    purple_notify_error (_telegram_protocol, "Not Registered", "Not Registered", error);
  }
}

static void request_password_entered (struct request_password_data *data, PurpleRequestFields* fields) {
  const char* pass = purple_request_fields_get_string (fields, "password");
  ((void (*)(struct tgl_state *TLS, const char *string[], void *arg)) data->callback) (data->TLS, &pass, data->arg);
  free (data);
}

void request_password (struct tgl_state *TLS, void (*callback) (struct tgl_state *TLS, const char *string[], void *arg),
                       void *arg) {
  connection_data *conn = TLS->ev_base;
  
  struct request_password_data *data = malloc (sizeof(struct request_password_data));
  data->TLS = TLS;
  data->arg = arg;
  data->callback = callback;
  
  PurpleRequestFields* fields = purple_request_fields_new();
  PurpleRequestField* field = NULL;
  
  PurpleRequestFieldGroup* group = purple_request_field_group_new ("");
  field = purple_request_field_string_new ("password", "Password", "", 0);
  purple_request_field_string_set_masked (field, TRUE);
  purple_request_field_group_add_field (group, field);
  purple_request_fields_add_group (fields, group);
  
  if (! purple_request_fields (conn->gc, "Password needed", "Enter password for two factor authentication",
      NULL, fields, "Ok", G_CALLBACK(request_password_entered), "Cancel", NULL, conn->pa,
      NULL, NULL, data)) {
    const char *error = "No password for two factor authentication, enter it in extended settings.";
    purple_connection_error_reason (conn->gc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, error);
    purple_notify_error (_telegram_protocol, "Password invalid", "Password invalid", error);
  }
}

static void accept_secret_chat_cb (gpointer _data, const gchar *code) {
  struct accept_secret_chat_data *data = _data;
  
  tgl_do_accept_encr_chat_request (data->TLS, data->U, write_secret_chat_gw, 0);
  
  g_free (data);
}

static void decline_secret_chat_cb (gpointer _data, const gchar *code) {
  struct accept_secret_chat_data *data = _data;
  
  bl_do_peer_delete (data->TLS, data->U->id);
  purple_blist_remove_buddy (p2tgl_buddy_find(data->TLS, data->U->id));
  
  g_free (data);
}

void request_accept_secret_chat (struct tgl_state *TLS, struct tgl_secret_chat *U) {
  connection_data *conn = TLS->ev_base;
  PurpleBuddy *who = p2tgl_buddy_find (TLS, TGL_MK_USER (U->user_id));
  struct accept_secret_chat_data *data = g_new (struct accept_secret_chat_data, 1);
  data->TLS = TLS;
  data->U = U;
  
  gchar *message = g_strdup_printf ("Accept Secret Chat '%s'?", U->print_name);
  purple_request_accept_cancel (conn->gc, "Secret Chat", message, "Secret chats can only have one "
      "end point. If you accept a secret chat on this device, its messages will not be available anywhere "
      "else. If you decline, you can accept the chat on other devices.", 0, conn->pa, who->name, NULL, data,
      G_CALLBACK(accept_secret_chat_cb), G_CALLBACK(decline_secret_chat_cb));
  g_free (message);
}

static void create_group_chat_cb (void *_data, PurpleRequestFields* fields) {
  struct accept_create_chat_data *data = _data;
  const char *users[3] = {
    purple_request_fields_get_string (fields, "user1"),
    purple_request_fields_get_string (fields, "user2"),
    purple_request_fields_get_string (fields, "user3")
  };
  
  tgp_create_group_chat_by_usernames (data->TLS, data->title, users, 3, FALSE);
  g_free (data->title);
  free (data);
}

static void cancel_group_chat_cb (gpointer data) {
  struct accept_create_chat_data *d = data;
  g_free (d->title);
  free (d);
}

void request_choose_user (struct accept_create_chat_data *data) {
  struct tgl_state *TLS = data->TLS;
  connection_data *conn = TLS->ev_base;
  
  // Telegram doesn't allow to create chats with only one user, so we need to force
  // the user to specify at least one other one.
  PurpleRequestFields* fields = purple_request_fields_new();
  PurpleRequestFieldGroup* group = purple_request_field_group_new (
      "Use the autocompletion to invite at least one additional user.\n You can always add more users once"
      " the chat was created...");
  
  PurpleRequestField *field = purple_request_field_string_new ("user1", "User Name", NULL, FALSE);
  purple_request_field_set_type_hint (field, "screenname");
  purple_request_field_group_add_field (group, field);
  
  field = purple_request_field_string_new ("user2", "User Name", NULL, FALSE);
  purple_request_field_set_type_hint (field, "screenname");
  purple_request_field_group_add_field (group, field);
  
  field = purple_request_field_string_new ("user3", "User Name", NULL, FALSE);
  purple_request_field_set_type_hint (field, "screenname");
  purple_request_field_group_add_field (group, field);
  
  purple_request_fields_add_group (fields, group);
  purple_request_fields (conn->gc, "Create Group", "Invite Users", NULL, fields, "Ok",
      G_CALLBACK(create_group_chat_cb), "Cancel", G_CALLBACK(cancel_group_chat_cb), conn->pa, NULL, NULL, data);
}

void request_create_chat (struct tgl_state *TLS, const char *subject) {
  connection_data *conn = TLS->ev_base;
  
  struct accept_create_chat_data *data = malloc (sizeof (struct accept_create_chat_data));
  data->title = g_strdup (subject);
  data->TLS = TLS;
  char *title = g_strdup_printf ("Chat doesn't exist, create a new group chat named '%s'?", subject);
  
  purple_request_accept_cancel (conn->gc, "Create New Group Chat", title, NULL, 1, conn->pa, NULL,
      NULL, data, G_CALLBACK(request_choose_user), G_CALLBACK(cancel_group_chat_cb));
  g_free (title);
}
