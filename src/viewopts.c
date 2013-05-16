/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2013 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
#include "config_static.h"
#include <lcbex/viewopts.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/**
 * View option string manipulation and construction library
 * @author Mark Nunberg
 */

typedef struct view_param_st view_param;


typedef lcb_error_t (*view_param_handler)(view_param *param,
                                          struct lcbex_vopt_st *optobj,
                                          const void *value,
                                          size_t nvalue,
                                          int flags,
                                          char **error);

struct view_param_st {
    int itype;
    const char *param;
    view_param_handler handler;
};

#define DECLARE_HANDLER(name) \
    static lcb_error_t name(\
                        view_param *p, \
                        lcbex_vopt_t *optobj, \
                        const void *value, \
                        size_t nvalue, \
                        int flags, \
                        char **error);

DECLARE_HANDLER(bool_param_handler)
DECLARE_HANDLER(num_param_handler)
DECLARE_HANDLER(string_param_handler)
DECLARE_HANDLER(stale_param_handler)
DECLARE_HANDLER(onerror_param_handler)

#define jval_param_handler string_param_handler
#define jarry_param_handler string_param_handler

#undef DECLARE_HANDLER

static view_param recognized_view_params[] = {
#define XX(b, str, hbase) \
{ b, str, hbase##_param_handler },
    LCBEX_XVOPT
#undef XX
    { 0, NULL, NULL }
};


static char *my_strndup(const char *s, size_t n)
{
    char *buf = calloc(n + 1, 1);

    if (!buf) {
        return NULL;
    }

    strncpy(buf, s, n);

    return buf;
}


/**
 * Sets the option structure's value fields. Should be called only with
 * user input, and only if flags & NUMERIC == 0
 */
static void set_user_string(struct lcbex_vopt_st *optobj,
                            const void *value,
                            size_t nvalue,
                            int flags)
{
    optobj->noptval = nvalue;
    if (flags & LCBEX_VOPT_F_OPTVAL_CONSTANT) {
        optobj->optval = (const char *)value;
        optobj->flags |= LCBEX_VOPT_F_OPTVAL_CONSTANT;

    } else {
        optobj->optval = my_strndup((const char *)value, nvalue);
        optobj->flags &= (~LCBEX_VOPT_F_OPTVAL_CONSTANT);
    }
}

/**
 * Callbacks/Handlers for various parameters
 */
static lcb_error_t bool_param_handler(view_param *param,
                                      struct lcbex_vopt_st *optobj,
                                      const void *value,
                                      size_t nvalue,
                                      int flags,
                                      char **error)
{
    /**
     * Try real hard to get a boolean value
     */
    int bval = -1;
    *error = NULL;

    (void)param;


    if (flags & LCBEX_VOPT_F_OPTVAL_NUMERIC) {
        bval = *(int *)value;

    } else {
        if (strncasecmp(
                    "true", (const char *)value, nvalue) == 0) {
            bval = 1;
        } else if (strncasecmp(
                       "false", (const char *)value, nvalue) == 0) {
            bval = 0;
        } else {
            *error = "String must be either 'true' or 'false'";
            return LCB_EINVAL;
        }
    }

    optobj->optval = bval ? "true" : "false";
    optobj->noptval = strlen(optobj->optval);
    optobj->flags |= LCBEX_VOPT_F_OPTVAL_CONSTANT;

    return LCB_SUCCESS;
}

static lcb_error_t num_param_handler(view_param *param,
                                     struct lcbex_vopt_st *optobj,
                                     const void *value,
                                     size_t nvalue,
                                     int flags,
                                     char **error)
{
    (void)param;

    if (flags & LCBEX_VOPT_F_OPTVAL_NUMERIC) {
        char *numbuf = malloc(128); /* should be enough */

        /* assuming ints never reach this large in my lifetime */
        optobj->noptval = snprintf(numbuf, 128, "%d", *(int *)value);
        optobj->optval = numbuf;

        /* clear the 'constant' flag */
        optobj->flags &= (~LCBEX_VOPT_F_OPTVAL_CONSTANT);
        return LCB_SUCCESS;

    } else {
        size_t ii;
        const char *istr = (const char *)value;

        if (nvalue == SIZE_MAX) {
            nvalue = strlen(istr);
        }

        if (!nvalue) {
            *error = "Received an empty string";
            return LCB_EINVAL;
        }

        /**
         * First character may be a '-', which is technically allowable
         */
        if (isdigit(*istr) == 0 && *istr != '-') {
            *error = "String must consist entirely of a signed number";
            return LCB_EINVAL;
        }

        for (ii = 1; ii < nvalue; ii++) {
            if (!isdigit(istr[ii])) {
                *error = "String must consist entirely of digits";
                return LCB_EINVAL;
            }
        }

        set_user_string(optobj, value, nvalue, flags);

    }

    return 0;
}


/**
 * This is the logic used in PHP's urlencode
 */
static int needs_pct_encoding(char c)
{
    if (c >= 'a' && c <= 'z') {
        return 0;
    }
    if (c >= 'A' && c <= 'Z') {
        return 0;
    }
    if (c >= '0' && c <= '9') {
        return 0;
    }

    if (c == '-' || c == '_' || c == '.') {
        return 0;
    }
    return 1;
}

/**
 * Encodes a string into percent encoding. It is assumed dest has enough
 * space.
 *
 * Returns the effective length of the string.
 */
static size_t do_pct_encode(char *dest, const char *src, size_t nsrc)
{
    size_t d_len = 0;
    size_t ii;
    for (ii = 0; ii < nsrc; ii++) {
        if (needs_pct_encoding(src[ii])) {
            if (dest) {
                sprintf(dest + d_len, "%%%02X", (unsigned char)src[ii]);
            }
            d_len += 3;
        } else {
            if (dest) {
                dest[d_len] = src[ii];
            }
            ++d_len;
        }
    }

    return d_len;
}

static lcb_error_t string_param_handler(view_param *param,
                                        struct lcbex_vopt_st *optobj,
                                        const void *value,
                                        size_t nvalue,
                                        int flags,
                                        char **error)
{
    (void)param;

    if (flags & LCBEX_VOPT_F_OPTVAL_NUMERIC) {
        *error = "Option requires a string value";
        return LCB_EINVAL;
    }

    if ((flags & LCBEX_VOPT_F_PCTENCODE) == 0) {
        /* determine if we need to encode anything as a percent */
        set_user_string(optobj, value, nvalue, flags);
        return LCB_SUCCESS;

    } else {
        size_t needed_size = 0;
        const char *str = (const char *)value;
        size_t ii;

        for (ii = 0; ii < nvalue; ii++) {
            if (needs_pct_encoding(str[ii])) {
                needed_size += 3;
            } else {
                needed_size++;
            }
        }

        if (needed_size == nvalue) {
            set_user_string(optobj, value, nvalue, flags);
            return LCB_SUCCESS;
        }

        optobj->optval = malloc(needed_size + 1);
        ((char *)(optobj->optval))[needed_size] = '\0';
        optobj->noptval = do_pct_encode((char *)optobj->optval, str, nvalue);
    }
    return LCB_SUCCESS;
}

static lcb_error_t stale_param_handler(view_param *p,
                                       struct lcbex_vopt_st *optobj,
                                       const void *value,
                                       size_t nvalue,
                                       int flags,
                                       char **error)
{
    /**
     * See if we can get it to be a true/false param
     */
    optobj->flags |= LCBEX_VOPT_F_OPTVAL_CONSTANT;
    if (bool_param_handler(p, optobj, value, nvalue, flags, error)
            == LCB_SUCCESS) {

        if (*optobj->optval == 't') {
            optobj->optval = "ok";
            optobj->noptval = sizeof("ok") - 1;
        }
        return LCB_SUCCESS;
    }

    if (strncasecmp("update_after", (const char *)value, nvalue) == 0) {
        optobj->optval = "update_after";
        optobj->noptval = sizeof("update_after") - 1;
        return LCB_SUCCESS;

    } else if (strncasecmp("ok", (const char *)value, nvalue) == 0) {
        optobj->optval = "ok";
        optobj->noptval = sizeof("ok") - 1;
        return LCB_SUCCESS;
    }

    *error = "stale must be a boolean or the string 'update_after'";
    return LCB_EINVAL;
}

static lcb_error_t onerror_param_handler(view_param *param,
                                         struct lcbex_vopt_st *optobj,
                                         const void *value,
                                         size_t nvalue,
                                         int flags,
                                         char **error)
{
    (void)param;
    (void)flags;

    *error = "on_error must be one of 'continue' or 'stop'";
    optobj->flags |= LCBEX_VOPT_F_OPTVAL_CONSTANT;

    if (strncasecmp("stop", (const char *)value, nvalue) == 0) {
        optobj->optval = "stop";
        optobj->noptval = sizeof("stop") - 1;

    } else if (strncasecmp("continue", (const char *)value, nvalue) == -0) {
        optobj->optval = "continue";
        optobj->noptval = sizeof("continue") - 1;

    } else {
        return LCB_EINVAL;
    }

    return LCB_SUCCESS;
}


static view_param *find_view_param(const void *option, size_t noption, int flags)
{
    view_param *ret;
    for (ret = recognized_view_params; ret->param; ret++) {
        if (flags & LCBEX_VOPT_F_OPTNAME_NUMERIC) {
            if (*(int *)option == ret->itype) {
                return ret;
            }
        } else {
            if (strncmp((const char *)option, ret->param, noption) == 0) {
                return ret;
            }
        }
    }
    return NULL;
}

LCBEX_API
lcb_error_t lcbex_vopt_assign(struct lcbex_vopt_st *optobj,
                            const void *option,
                            size_t noption,
                            const void *value,
                            size_t nvalue,
                            int flags,
                            char **error_string)
{
    view_param *vparam;
    memset(optobj, 0, sizeof(*optobj));

    if (nvalue == SIZE_MAX) {
        nvalue = strlen((char *)value);
    }
    if (noption == SIZE_MAX) {
        noption = strlen((char *)option);
    }

    if ((flags & LCBEX_VOPT_F_OPTVAL_NUMERIC) == 0 && nvalue == 0) {
        *error_string = "Missing value length";
        return LCB_EINVAL;
    }

    if ((flags & LCBEX_VOPT_F_OPTNAME_NUMERIC) == 0 && noption == 0) {
        *error_string = "Missing option name length";
        return LCB_EINVAL;
    }

    if (flags & LCBEX_VOPT_F_PASSTHROUGH) {
        if (flags & LCBEX_VOPT_F_OPTNAME_NUMERIC) {
            *error_string = "Can't use passthrough with option constants";
            return LCB_EINVAL;
        }

        optobj->optname = my_strndup((const char *)option, noption);
        optobj->noptname = noption;

        if (flags & LCBEX_VOPT_F_OPTVAL_NUMERIC) {
            return num_param_handler(NULL, optobj, value, nvalue, flags,
                                     error_string);
        } else {
            return string_param_handler(NULL, optobj, value, nvalue, flags,
                                        error_string);
        }
        /* not reached */
    }

    optobj->flags = flags;

    vparam = find_view_param(option, noption, flags);
    if (!vparam) {
        *error_string = "Unrecognized option";
        return LCB_EINVAL;
    }

    if (flags & LCBEX_VOPT_F_OPTNAME_NUMERIC) {
        optobj->optname = vparam->param;
        optobj->noptname = strlen(vparam->param);
        optobj->flags |= LCBEX_VOPT_F_OPTNAME_CONSTANT;

    } else {
        optobj->noptname = noption;
        /* are we a literal or constant? */
        if (flags & LCBEX_VOPT_F_OPTNAME_CONSTANT) {
            optobj->optname = (const char *)option;

        } else {
            optobj->optname = my_strndup((const char *)option, noption);
        }
    }

    return vparam->handler(vparam, optobj, value, nvalue, flags, error_string);
}

LCBEX_API
void lcbex_vopt_cleanup(lcbex_vopt_t *optobj)
{
    if (optobj->optname) {
        if ((optobj->flags & LCBEX_VOPT_F_OPTNAME_CONSTANT) == 0) {
            free((void *)optobj->optname);
        }
    }
    if (optobj->optval) {
        if ((optobj->flags & LCBEX_VOPT_F_OPTVAL_CONSTANT) == 0) {
            free((void *)optobj->optval);
        }
    }

    memset(optobj, 0, sizeof(*optobj));
}

LCBEX_API
void lcbex_vopt_cleanup_list(lcbex_vopt_t **options, size_t noptions,
                           int is_continguous)
{
    size_t ii;
    for (ii = 0; ii < noptions; ii++) {
        if (is_continguous) {
            lcbex_vopt_cleanup((*options) + ii);
        } else {
            lcbex_vopt_cleanup(options[ii]);
        }
    }
}

LCBEX_API
size_t lcbex_vqstr_calc_len(const lcbex_vopt_t *const *options,
                          size_t noptions)
{
    size_t ret = 1; /* for the '?' */
    size_t ii;
    for (ii = 0; ii < noptions; ii++) {
        const lcbex_vopt_t *curopt = options[ii];
        ret += curopt->noptname;
        ret += curopt->noptval;
        /* add two for '&' and '=' */
        ret += 2;
    }

    return ret + 1;
}

LCBEX_API
size_t lcbex_vqstr_write(const lcbex_vopt_t *const *options,
                       size_t noptions,
                       char *buf)
{
    size_t ii;
    char *bufp = buf;

    *bufp = '?';
    bufp++;

    for (ii = 0; ii < noptions; ii++) {
        const lcbex_vopt_t *curopt = options[ii];
        memcpy(bufp, curopt->optname, curopt->noptname);
        bufp += curopt->noptname;

        *bufp = '=';
        bufp++;

        memcpy(bufp, curopt->optval, curopt->noptval);
        bufp += curopt->noptval;

        *bufp = '&';
        bufp++;
    }
    bufp--; /* trailing '&' */
    *bufp = '\0';
    return bufp - buf;
}

/**
 * Convenience function to make a view URI.
 */
LCBEX_API
char *lcbex_vqstr_make_uri(const char *design, size_t ndesign,
                         const char *view, size_t nview,
                         const lcbex_vopt_t *const *options,
                         size_t noptions)
{
    size_t needed_len, path_len;
    char *buf;

    if (ndesign == SIZE_MAX) {
        ndesign = strlen(design);
    }

    if (nview == SIZE_MAX) {
        nview = strlen(view);
    }

    /* I'm lazy */
    path_len = snprintf(NULL, 0,
                        "_design/%.*s/_view/%.*s",
                        (int)ndesign, design,
                        (int)nview, view);

    needed_len = lcbex_vqstr_calc_len(options, noptions);
    needed_len += path_len + 1;

    buf = malloc(needed_len);
    snprintf(buf, path_len + 1,
             "_design/%.*s/_view/%.*s",
             (int)ndesign, design,
             (int)nview, view);

    lcbex_vqstr_write(options, noptions, buf + path_len);
    return buf;
}

LCBEX_API
lcb_error_t lcbex_vopt_createv(lcbex_vopt_t *optarray[],
                             size_t *noptions,
                             char **errstr, ...)
{
    /* two lists, first to determine how much to allocated */
    va_list ap;
    char *strp;
    lcb_error_t err = LCB_SUCCESS;
    size_t n_alloc = 8;

    *optarray = malloc(n_alloc * sizeof(**optarray));
    *errstr = "";

    if (!*optarray) {
        return LCB_CLIENT_ENOMEM;
    }

    va_start(ap, errstr);
    *noptions = 0;

    while ((strp = va_arg(ap, char *))) {
        char *value;

        if (*noptions + 1 > n_alloc) {
            n_alloc *= 2;
            *optarray = realloc(*optarray, n_alloc * sizeof(**optarray));

            if (!*optarray) {
                err = LCB_CLIENT_ENOMEM;
                break;
            }
        }

        value = va_arg(ap, char *);

        if (!value) {
            err = LCB_EINVAL;
            *errstr = "Got odd number of arguments";
            break;
        }

        (*noptions)++;

        err = lcbex_vopt_assign((*optarray) + *noptions - 1,
                              strp, -1,
                              value, -1,
                              0,
                              errstr);

        if (err != LCB_SUCCESS) {
            break;
        }
    }
    va_end(ap);

    if (*noptions == 0) {
        err = LCB_EINVAL;
        *errstr = "Got no arguments";
    }

    if (err != LCB_SUCCESS) {
        lcbex_vopt_cleanup_list(optarray, *noptions, 1);
        free(*optarray);
        *optarray = NULL;
    }
    return err;
}
