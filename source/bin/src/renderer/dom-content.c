/*
   DOM domcnt widget.

   Copyright (C) 2022
   Beijing FMSoft Technologies Co., Ltd.

   Written by:
   Vincent Wei <vincent@minigui.org>, 2022

   This file is part of the PurC Midnight Commander (`PurCMC` for short).

   The PurCMC is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The PurCMC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file dom-domcnt.c
 * \brief Source: DOM domcnt widget
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>           /* PRIuMAX */
#include <assert.h>
#include <sys/stat.h>

#include "lib/global.h"
#include "lib/unixcompat.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* is_idle() */
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/timefmt.h"        /* file_date() */
#include "lib/util.h"
#include "lib/widget.h"

#include "dom-content.h"

/*** global variables ********************************************************/

/*** file scope macro definitions ********************************************/

/*** file scope type declarations ********************************************/

/*** file scope variables ****************************************************/

/*** file scope functions ****************************************************/
/* ------------------------------------------------------------------------- */

static void
domcnt_draw_frame (WDOMContent * domcnt)
{
    Widget *w = WIDGET (domcnt);
    int width = str_term_width1 (domcnt->title);

    tty_set_normal_attrs ();
    tty_setcolor (NORMAL_COLOR);
    widget_erase (w);
    tty_draw_box (w->y, w->x, w->lines, w->cols, FALSE);

    if (widget_get_state (w, WST_FOCUSED))
        tty_setcolor (SELECTED_COLOR);
    widget_gotoyx (w, 0, (w->cols - width - 2) / 2);
    tty_printf (" %s ", domcnt->title);
    tty_setcolor (NORMAL_COLOR);
}

/* ------------------------------------------------------------------------- */
static void
domcnt_show_content (WDOMContent * domcnt)
{
    domcnt_draw_frame (domcnt);
    if (domcnt->text && domcnt->text_len > 0) {
        domcnt_display_text (domcnt);
    }
}

static cb_ret_t
domcnt_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDOMContent *domcnt = (WDOMContent *) w;

    switch (msg)
    {
    case MSG_INIT:
        domcnt->data_area.top = 1;
        domcnt->data_area.left = 2;
        domcnt->data_area.height = w->lines - 2;
        domcnt->data_area.width = w->cols - 4;
        return MSG_HANDLED;

    case MSG_DRAW:
        domcnt_show_content (domcnt);
        return MSG_HANDLED;

    case MSG_DESTROY:
        if (domcnt->text)
            g_free (domcnt->text);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/*** public functions ********************************************************/

WDOMContent *
dom_content_new (int y, int x, int lines, int cols,
        const char *title, const char *show_eof)
{
    WDOMContent *domcnt;
    Widget *w;

    domcnt = g_new0 (struct WDOMContent, 1);
    w = WIDGET (domcnt);
    widget_init (w, y, x, lines, cols, domcnt_callback, NULL);
    w->options |= WOP_SELECTABLE;

    domcnt->title = title;
    domcnt->show_eof = show_eof;

    return domcnt;
}

bool
dom_content_load (WDOMContent *domcnt, GString *string)
{
    if (domcnt->text) {
        g_free (domcnt->text);
        domcnt->text = NULL;
        domcnt->text_len = 0;
    }

    if (string) {
        /* take the owner of string */
        domcnt->text_len = string->len;
        domcnt->text = g_string_free (string, FALSE);
    }

    domcnt->dpy_start = 0;
    domcnt->dpy_paragraph_skip_lines = 0;
    domcnt->dpy_wrap_dirty = FALSE;
    domcnt->dpy_text_column = 0;
    domcnt->force_max = -1;
    domcnt->mode_flags.wrap = TRUE;
    domcnt->mode_flags.nroff = FALSE;
    domcnt_formatter_state_init (&domcnt->dpy_state_top, 0);

    if (domcnt->text) {
        domcnt->dpy_start = domcnt_bol (domcnt, 0, 0);
        domcnt->dpy_wrap_dirty = TRUE;
    }

    domcnt_show_content (domcnt);
    return domcnt->text != NULL;
}

