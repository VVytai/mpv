/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "clipboard.h"

#include "common/common.h"
#include "common/global.h"
#include "options/m_config.h"
#include "player/core.h"

struct clipboard_opts {
    bool enabled;
    bool monitor;
};

#define OPT_BASE_STRUCT struct clipboard_opts
const struct m_sub_options clipboard_conf = {
    .opts = (const struct m_option[]) {
        {"enable", OPT_BOOL(enabled), .flags = UPDATE_CLIPBOARD},
        {"monitor", OPT_BOOL(monitor), .flags = UPDATE_CLIPBOARD},
        {0}
    },
    .defaults = &(const struct clipboard_opts) {
        .enabled = true,
    },
    .size = sizeof(struct clipboard_opts)
};

// backend list
extern const struct clipboard_backend clipboard_backend_win32;
extern const struct clipboard_backend clipboard_backend_vo;

static const struct clipboard_backend *const clipboard_backend_list[] = {
#if HAVE_WIN32_DESKTOP
    &clipboard_backend_win32,
#endif
    &clipboard_backend_vo,
};

struct clipboard_ctx *mp_clipboard_create(struct clipboard_init_params *params,
                                          struct mpv_global *global)
{
    struct clipboard_ctx *cl = talloc_ptrtype(NULL, cl);
    *cl = (struct clipboard_ctx) {
        .log = mp_log_new(cl, global->log, "clipboard"),
        .global = global,
        .monitor = params->flags & CLIPBOARD_INIT_ENABLE_MONITORING,
    };

    for (int i = 0; i < MP_ARRAY_SIZE(clipboard_backend_list); i++) {
        const struct clipboard_backend *backend = clipboard_backend_list[i];
        if (backend->init(cl, params) == CLIPBOARD_SUCCESS) {
            MP_VERBOSE(cl, "Initialized %s clipboard backend.\n", backend->name);
            cl->backend = backend;
            goto success;
        }
    }

    MP_WARN(cl, "Failed to initialize any clipboard backend.\n");
    talloc_free(cl);
    return NULL;
success:
    return cl;
}

void mp_clipboard_destroy(struct clipboard_ctx *cl)
{
    if (cl && cl->backend->uninit)
        cl->backend->uninit(cl);
    talloc_free(cl);
}

bool mp_clipboard_data_changed(struct clipboard_ctx *cl)
{
    if (cl && cl->backend->data_changed && cl->monitor)
        return cl->backend->data_changed(cl);
    return false;
}

int mp_clipboard_get_data(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                          struct clipboard_data *out, void *talloc_ctx)
{
    if (cl && cl->backend->get_data)
        return cl->backend->get_data(cl, params, out, talloc_ctx);
    return CLIPBOARD_FAILED;
}

int mp_clipboard_set_data(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                          struct clipboard_data *data)
{
    if (cl && cl->backend->set_data)
        return cl->backend->set_data(cl, params, data);
    return CLIPBOARD_FAILED;
}

void reinit_clipboard(struct MPContext *mpctx)
{
    mp_clipboard_destroy(mpctx->clipboard);
    mpctx->clipboard = NULL;

    struct clipboard_opts *opts = mp_get_config_group(NULL, mpctx->global, &clipboard_conf);
    if (opts->enabled) {
        struct clipboard_init_params params = {
            .mpctx = mpctx,
            .flags = opts->monitor ? CLIPBOARD_INIT_ENABLE_MONITORING : 0,
        };
        mpctx->clipboard = mp_clipboard_create(&params, mpctx->global);
    }
    talloc_free(opts);
}