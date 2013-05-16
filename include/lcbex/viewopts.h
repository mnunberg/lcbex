/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2010, 2011 Couchbase, Inc.
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

/**
 * View extensions for libcouchbase.
 *
 * This contains functions to:
 *
 * o Build and validate view options
 * o Encode required view option values into JSON
 * o Encode required view option values into percent-encoding
 */

#ifndef LCBEX_VIEWOPTS_H
#define LCBEX_VIEWOPTS_H

#include <lcbex/lcbex.h>

#ifdef __cplusplus
extern "C" {
#endif

    enum {
        /* encode the value in percent-encoding if needed */
        LCBEX_VOPT_F_PCTENCODE = 1 << 0,

        /* option value should be treated as an int (and possibly coerced)
         * rather than a string
         */
        LCBEX_VOPT_F_OPTVAL_NUMERIC = 1 << 1,

        /**
         * This flag indicates the option name is user-specified. No validation
         * will be performed
         */
        LCBEX_VOPT_F_PASSTHROUGH = 1 << 2,

        /* option value is constant. Don't free */
        LCBEX_VOPT_F_OPTVAL_CONSTANT = 1 << 3,

        /* option name is constant. Don't free */
        LCBEX_VOPT_F_OPTNAME_CONSTANT = 1 << 4,

        /* Option name is an integer constant, not a string */
        LCBEX_VOPT_F_OPTNAME_NUMERIC = 1 << 5
    };


    /**
     * This x-macro accepts three arguments:
     * (1) The 'base' constant name for the view option
     * (2) The string name (as is encoded into the URI)
     * (3) The expected type. These are used internally. Types are:
     *      'bool' - coerced into a 'true' or 'false' string
     *      'num' - coerced into a numeric string
     *      'string' - optionally percent-encoded
     *      'jval' - aliased to string, but means a JSON-encoded primitive or
     *          complex value
     *      'jarry' - a JSON array
     *      'onerror' - special type accepting the appropriate values (stop, continue)
     *      'stale' - special type accepting ('ok' (coerced if needed from true), 'false',
     *          and 'update_after')
     *
     * You may use this xmacro if developing higher level wrappers around vopts.
     * Basically, you can create your own handlers which correspond to the
     * appropriate type.
     */
#define LCBEX_XVOPT \
    XX(LCBEX_VOPT_OPT_DESCENDING, "descending", bool) \
    XX(LCBEX_VOPT_OPT_ENDKEY, "endkey", jval) \
    XX(LCBEX_VOPT_OPT_ENDKEY_DOCID, "endkey_docid", string) \
    XX(LCBEX_VOPT_OPT_FULLSET, "full_set", bool) \
    XX(LCBEX_VOPT_OPT_GROUP, "group", bool) \
    XX(LCBEX_VOPT_OPT_GROUP_LEVEL, "group_level", num) \
    XX(LCBEX_VOPT_OPT_INCLUSIVE_END, "inclusive_end", bool) \
    XX(LCBEX_VOPT_OPT_KEYS, "keys", jarry) \
    XX(LCBEX_VOPT_OPT_SINGLE_KEY, "key", jval) \
    XX(LCBEX_VOPT_OPT_ONERROR, "on_error", onerror) \
    XX(LCBEX_VOPT_OPT_REDUCE, "reduce", bool) \
    XX(LCBEX_VOPT_OPT_STALE, "stale", stale) \
    XX(LCBEX_VOPT_OPT_SKIP, "skip", num) \
    XX(LCBEX_VOPT_OPT_LIMIT, "limit", num) \
    XX(LCBEX_VOPT_OPT_STARTKEY, "startkey", jval) \
    XX(LCBEX_VOPT_OPT_STARTKEY_DOCID, "startkey_docid", string) \
    XX(LCBEX_VOPT_OPT_DEBUG, "debug", bool)

    enum {
        LCBEX_VOPT_OPT_CLIENT_PASSTHROUGH = 0,
        LCBEX_VOPT_OPT_DESCENDING,
        LCBEX_VOPT_OPT_ENDKEY,
        LCBEX_VOPT_OPT_ENDKEY_DOCID,
        LCBEX_VOPT_OPT_FULLSET,
        LCBEX_VOPT_OPT_GROUP,
        LCBEX_VOPT_OPT_GROUP_LEVEL,
        LCBEX_VOPT_OPT_INCLUSIVE_END,
        LCBEX_VOPT_OPT_KEYS,
        LCBEX_VOPT_OPT_SINGLE_KEY,
        LCBEX_VOPT_OPT_ONERROR,
        LCBEX_VOPT_OPT_REDUCE,
        LCBEX_VOPT_OPT_STALE,
        LCBEX_VOPT_OPT_SKIP,
        LCBEX_VOPT_OPT_LIMIT,
        LCBEX_VOPT_OPT_STARTKEY,
        LCBEX_VOPT_OPT_STARTKEY_DOCID,
        LCBEX_VOPT_OPT_DEBUG,
        _LCB_VOPT_OPT_MAX
    };


    typedef struct lcbex_vopt_st {
        /* NUL-terminated option name */
        const char *optname;
        /* NUL-terminated option value */
        const char *optval;
        size_t noptname;
        size_t noptval;
        int flags;
    } lcbex_vopt_t;

    /**
     * Properly initializes a view_option structure, checking (if requested)
     * for valid option names and inputs
     *
     * @param optobj an allocated but empty optobj structure
     *
     * @param option An option name.
     * This may be a string or something else dependent on the flags
     *
     * @param noption Size of an option (if the option name is a string)
     *
     * @param value The value for the option. This may be a string (defaul) or
     * something else depending on the flags. If a string, it must be UTF-8 compatible
     *
     * @param nvalue the sizeo of the value (if the value is a string).
     *
     * @param flags. A set of flags to specify for the conversion
     *
     * @param error_string An error string describing the details of why validation
     * failed. This is a constant string and should not be freed. Only valid if the
     * function does not return LCB_SUCEESS
     *
     * @return LCB_SUCCESS if the validation/conversion was a success, or an error
     * code (typically LCB_EINVAL) otherwise.
     *
     * If the operation succeeded, the option object should be cleaned up using
     * free_view_option which will clean up necessary fields within the structure.
     * As the actual structure is not allocated by the library, the library will
     * not free the structure.
     */
    LCBEX_API
    lcb_error_t lcbex_vopt_assign(lcbex_vopt_t *optobj,
                                const void *option,
                                size_t noption,
                                const void *value,
                                size_t nvalue,
                                int flags,
                                char **error_string);

    /**
     * Creates an array of options from a list of strings. The list should be
     * NULL terminated
     * @param optarray a pointer which will contain an array of vopts
     * @param noptions will contain the number of vopts
     * @param errstr - will contain a pointer to a string upon error
     * @param .. "key", "value" pairs; the final parameter should be a NULL
     * @return LCB_SUCCESS on success, error otherwise. All memory is freed on error;
     * otherwise the list must be freed using vopt_cleanup
     *
     * Note that the optarray is a contiguous array of pointers. In order
     * to make this into a format acceptable for the other functions which take
     * a list, you must do something like:
     *
     *  lcb_vopt_t **vopt_list = malloc(sizeof(lcb_vopt_t*) * noptions);
     *  for (ii = 0; ii < noptions; ii++) {
     *      vopt_list[ii] = optarray + ii;
     *  }
     *  ....
     *
     *  free(vopt_list);
     *
     */
    LCBEX_API
    lcb_error_t lcbex_vopt_createv(lcbex_vopt_t *optarray[],
                                 size_t *noptions, char **errstr, ...);

    /**
     * Cleans up a vopt structure. This does not free the structure, but does
     * free any allocated members in the structure's internal fields (if any).
     */
    LCBEX_API
    void lcbex_vopt_cleanup(lcbex_vopt_t *optobj);

    /**
     * Convenience function to free a list of options. This is here since many
     * of the functions already require an lcb_vopt_st **
     *
     * @param options a list of options
     * @param noptions how many options
     * @param contiguous whether the pointer points to an array of pointers,
     * (i.e. where *(options[n]) dereferneces the structure, or a pointer to a
     * continuous block of memory, so that (*options)[n] dereferences
     */
    LCBEX_API
    void lcbex_vopt_cleanup_list(lcbex_vopt_t **options, size_t noptions,
                               int contiguous);


    /**
     * Calculates the minimum size of the query portion of a buffer
     */
    LCBEX_API
    size_t lcbex_vqstr_calc_len(const lcbex_vopt_t *const *options,
                              size_t noptions);

    /**
     * Writes the query string to a buffer. The buffer must have enough space
     * as determined by vqstr_calc_len.
     *
     * @param options a list of options to write
     * @param noptions how many options in the list
     * @param buf an allocated buffer large enough to hold the serialized
     * output of all the options (typically determined by lcb_vqstr_calc_len
     *
     * @return The amount of bytes actually written to the buffer
     */
    LCBEX_API
    size_t lcbex_vqstr_write(const lcbex_vopt_t *const *options, size_t noptions,
                           char *buf);


    /**
     * Creates a proper URI query string for a view with its parameters.
     *
     * @param design the name of the design document
     * @param ndesign length of design name (-1 for nul-terminated)
     * @param view the name of the view to query
     * @param nview the length of the view name (-1 for nul-terminated)
     * @param options the view options for this query
     * @param noptions how many options
     *
     * @return an allocated string (via malloc) which may be used for the view
     * query. The string will be NUL-terminated so strlen may be called to obtain
     * the length.
     */
    LCBEX_API
    char *lcbex_vqstr_make_uri(const char *design, size_t ndesign,
                             const char *view, size_t nview,
                             const lcbex_vopt_t *const *options,
                             size_t noptions);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LCB_VIEWOPTS_H */
