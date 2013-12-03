/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2007-2010 David Zeuthen <zeuthen@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include "util.h"

#include <ctype.h>
#include <string.h>

/**
 * udisks_safe_append_to_object_path:
 * @str: A #GString to append to.
 * @s: A UTF-8 string.
 *
 * Appends @s to @str in a way such that only characters that can be
 * used in a D-Bus object path will be used. E.g. a character not in
 * <literal>[A-Z][a-z][0-9]_</literal> will be escaped as _HEX where
 * HEX is a two-digit hexadecimal number.
 *
 * Note that his mapping is not bijective - e.g. you cannot go back
 * to the original string.
 */
void
ul_util_safe_append_to_object_path (GString *str,
                                    const gchar *s)
{
  guint n;
  for (n = 0; s[n] != '\0'; n++)
    {
      gint c = s[n];
      /* D-Bus spec sez:
       *
       * Each element must only contain the ASCII characters "[A-Z][a-z][0-9]_"
       */
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
        {
          g_string_append_c (str, c);
        }
      else
        {
          /* Escape bytes not in [A-Z][a-z][0-9] as _<hex-with-two-digits> */
          g_string_append_printf (str, "_%02x", c);
        }
    }
}

static void
escaper (GString *s, const gchar *str)
{
  const gchar *p;
  for (p = str; *p != '\0'; p++)
    {
      gint c = *p;
      switch (c)
        {
        case '"':
          g_string_append (s, "\\\"");
          break;

        case '\\':
          g_string_append (s, "\\\\");
          break;

        default:
          g_string_append_c (s, c);
          break;
        }
    }
}

/**
 * ul_util_escape_and_quote:
 * @str: The string to escape.
 *
 * Like ul_util_escape() but also wraps the result in
 * double-quotes.
 *
 * Returns: The double-quoted and escaped string. Free with g_free().
 */
gchar *
ul_util_escape_and_quote (const gchar *str)
{
  GString *s;

  g_return_val_if_fail (str != NULL, NULL);

  s = g_string_new ("\"");
  escaper (s, str);
  g_string_append_c (s, '"');

  return g_string_free (s, FALSE);
}

/**
 * ul_util_escape:
 * @str: The string to escape.
 *
 * Escapes double-quotes (&quot;) and back-slashes (\) in a string
 * using back-slash (\).
 *
 * Returns: The escaped string. Free with g_free().
 */
gchar *
ul_util_escape (const gchar *str)
{
  GString *s;

  g_return_val_if_fail (str != NULL, NULL);

  s = g_string_new (NULL);
  escaper (s, str);

  return g_string_free (s, FALSE);
}

struct VariantReaderData {
  const GVariantType *type;
  void (*callback) (GPid pid, GVariant *result, GError *error, gpointer user_data);
  gpointer user_data;
  GPid pid;
  GIOChannel *output_channel;
  GByteArray *output;
  gint output_watch;
};

static gboolean
variant_reader_child_output (GIOChannel *source,
                             GIOCondition condition,
                             gpointer user_data)
{
  struct VariantReaderData *data = user_data;
  guint8 buf[1024];
  gsize bytes_read;

  g_io_channel_read_chars (source, (gchar *)buf, sizeof buf, &bytes_read, NULL);
  g_byte_array_append (data->output, buf, bytes_read);
  return TRUE;
}

static void
variant_reader_watch_child (GPid     pid,
                            gint     status,
                            gpointer user_data)
{
  struct VariantReaderData *data = user_data;
  guint8 *buf;
  gsize buf_size;
  GVariant *result;
  GError *error = NULL;

  data->pid = 0;

  if (!g_spawn_check_exit_status (status, &error))
    {
      data->callback (pid, NULL, error, data->user_data);
      g_error_free (error);
      g_byte_array_free (data->output, TRUE);
    }
  else
    {
      if (g_io_channel_read_to_end (data->output_channel, (gchar **)&buf, &buf_size, NULL) == G_IO_STATUS_NORMAL)
        {
          g_byte_array_append (data->output, buf, buf_size);
          g_free (buf);
        }

      result = g_variant_new_from_data (data->type,
                                        data->output->data,
                                        data->output->len,
                                        TRUE,
                                        g_free, NULL);
      g_byte_array_free (data->output, FALSE);
      data->callback (pid, result, NULL, data->user_data);
      g_variant_unref (result);
    }
}

static void
variant_reader_destroy (gpointer user_data)
{
  struct VariantReaderData *data = user_data;

  g_source_remove (data->output_watch);
  g_io_channel_unref (data->output_channel);
  g_free (data);
}

GPid
ul_util_spawn_for_variant (const gchar **argv,
                           const GVariantType *type,
                           void (*callback) (GPid, GVariant *, GError *, gpointer),
                           gpointer user_data)
{
  GError *error;
  struct VariantReaderData *data;
  GPid pid;
  gint output_fd;

  if (!g_spawn_async_with_pipes (NULL,
                                 (gchar **)argv,
                                 NULL,
                                 G_SPAWN_DO_NOT_REAP_CHILD,
                                 NULL,
                                 NULL,
                                 &pid,
                                 NULL,
                                 &output_fd,
                                 NULL,
                                 &error))
    {
      callback (0, NULL, error, user_data);
      g_error_free (error);
      return 0;
    }

  data = g_new0 (struct VariantReaderData, 1);

  data->type = type;
  data->callback = callback;
  data->user_data = user_data;

  data->pid = pid;
  data->output = g_byte_array_new ();
  data->output_channel = g_io_channel_unix_new (output_fd);
  g_io_channel_set_encoding (data->output_channel, NULL, NULL);
  g_io_channel_set_flags (data->output_channel, G_IO_FLAG_NONBLOCK, NULL);
  data->output_watch = g_io_add_watch (data->output_channel, G_IO_IN, variant_reader_child_output, data);

  g_child_watch_add_full (G_PRIORITY_DEFAULT_IDLE,
                          pid, variant_reader_watch_child, data, variant_reader_destroy);
  return pid;
}

static gboolean
valid_lvm_name_char (gint c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '+' || c == '_' || c == '.' || c == '-';
}

#define LVM_ENCODING_PREFIX "+_"

gchar *
ul_util_encode_lvm_name (const gchar *name,
                         gboolean for_logical_volume)
{
  const gchar *n;
  GString *enc;
  gchar *encoded;

  for (n = name; *n; n++)
    {
      if (!valid_lvm_name_char (*n))
        goto encode;
    }

  if (*name == '-')
    goto encode;

  if (g_str_has_prefix (name, LVM_ENCODING_PREFIX))
    goto encode;

  if (for_logical_volume
      && (strstr (name, "_mlog")
          || strstr (name, "_mimage")
          || strstr (name, "_rimage")
          || strstr (name, "_rmeta")
          || strstr (name, "_tdata")
          || strstr (name, "_tmeta")
          || g_str_has_prefix (name, "pvmove")
          || g_str_has_prefix (name, "snapshot")))
    goto encode;

  return g_strdup (name);

 encode:
  enc = g_string_new (LVM_ENCODING_PREFIX);
  for (n = name; *n; n++)
    {
      if (!valid_lvm_name_char (*n) || *n == '_')
        g_string_append_printf (enc, "_%02x", *(unsigned char *)n);
      else
        g_string_append_c (enc, *n);
    }
  encoded = enc->str;
  g_string_free (enc, FALSE);
  return encoded;
}

gchar *
ul_util_decode_lvm_name (const gchar *encoded)
{
  const gchar *e;
  GString *dec;
  gchar *decoded;

  if (!g_str_has_prefix (encoded, LVM_ENCODING_PREFIX))
    return g_strdup (encoded);

  dec = g_string_new ("");
  for (e = encoded + strlen(LVM_ENCODING_PREFIX); *e; e++)
    {
      if (e[0] == '_')
        {
          if (isxdigit(e[1]) && isxdigit(e[2]))
            {
              gint c = (g_ascii_xdigit_value (e[1]) << 4) | g_ascii_xdigit_value (e[2]);
              g_string_append_c (dec, c);
              e += 2;
            }
          else
            {
              g_string_free (dec, TRUE);
              return g_strdup (encoded);
            }
        }
      else
        g_string_append_c (dec, *e);
    }
  decoded = dec->str;
  g_string_free (dec, FALSE);
  return decoded;
}
