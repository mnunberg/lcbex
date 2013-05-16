/**
 * Example demonstrating various ways to use view options
 */

/**
 * Need to include this header, which is not automatically included with
 * couchbase.h
 */
#include <lcbex/viewopts.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Demonstrates creating single view options and writing them.
 */
static void create_single_vopt(void)
{
    lcbex_vopt_t vopt;
    lcbex_vopt_t *vopt_list[1];

    lcb_error_t err;
    char *errstr;
    char *qstr;

    /**
     * Assign an option-value pair to the structure.
     */
    err = lcbex_vopt_assign(&vopt,
                          "limit", /* option name */
                          -1, /* length of option name (-1 means auto, or strlen) */
                          "100", /* option value */
                          -1, /* option value length */
                          0, /* special flags */
                          &errstr /* pointer for error string (in case of error) */
                         );
    assert(err == LCB_SUCCESS);

    /**
     * The easiest way to get an option into a proper string is to create
     * a full URI for it.
     */

    vopt_list[0] = &vopt;
    qstr = lcbex_vqstr_make_uri("a_design_doc", -1, /* name and length of design doc */
                              "a_view_function", -1, /* name and length of view function */
                              (const lcbex_vopt_t * const *)vopt_list, 1 /* list and count of options */);

    printf("Have a query string: %s\n", qstr);

    /**
     * When done with it, we can just free it
     */
    free(qstr);

    /**
     * We also need to clean up the vopt itself before retuning
     */
    lcbex_vopt_cleanup(&vopt);
}

/**
 * The simplest use case allows for using string options with string values.
 * However if you're like me, then string literals are often prone to typos and
 * odd runtime issues. To this effect, we can use constants for option names
 * and integer literals for option values.
 */
static void create_with_constants(void)
{
    lcbex_vopt_t vopt;
    lcbex_vopt_t *vopt_list[1];
    lcb_error_t err;
    char *errstr;
    char *qstr;

    int optname;
    int optval;

    /**
     * Let's make the same query as we did in create_single_vopt
     */

    optname = LCBEX_VOPT_OPT_LIMIT;
    optval = 100;

    err = lcbex_vopt_assign(&vopt,
                          /**
                           * the option name and length.
                           * In this case, the option name is a pointer to an
                           * int containing the option constant. The size is
                           * 0.
                           */
                          &optname, 0,

                          /**
                           * We use the same semantics here for option
                           * values. Note that this only works because 'limit'
                           * accepts a numeric value. Not all options do.
                           */
                          &optval, 0,

                          /**
                           * These are two flags to indicate that the option
                           * and value passed are not strings, but ints
                           */
                          LCBEX_VOPT_F_OPTNAME_NUMERIC | LCBEX_VOPT_F_OPTVAL_NUMERIC,

                          &errstr);

    assert(err == LCB_SUCCESS);

    vopt_list[0] = &vopt;

    qstr = lcbex_vqstr_make_uri("a_design", -1, "a_view", -1,
                              (const lcbex_vopt_t * const *)vopt_list, 1);
    printf("Have query string generated from constants: %s\n", qstr);
    free(qstr);
    lcbex_vopt_cleanup(&vopt);
}

/**
 * Demonstrates option validation, and ways to circumvent it.
 */
static void create_invalid_options(void)
{
    lcbex_vopt_t vopt;
    char *errstr;
    lcb_error_t err;

    err = lcbex_vopt_assign(&vopt,
                          "user-defined-option", -1,
                          "bad-value", -1,
                          0,
                          &errstr);

    assert(err == LCB_EINVAL);
    printf("Cannot assign user-defined-option: '%s'\n", errstr);

    /* we must call cleanup on the option, even after an error */
    lcbex_vopt_cleanup(&vopt);

    err = lcbex_vopt_assign(&vopt,
                          "user-defined-option", -1,
                          "bad-value", -1,
                          LCBEX_VOPT_F_PASSTHROUGH,
                          &errstr);

    assert(err == LCB_SUCCESS);
    printf("Can assign user-defined-option when using F_PASSTHROUGH\n");
    lcbex_vopt_cleanup(&vopt);
}

/**
 * This demonstrates how to use the 'createv' function to create a list
 * of view options.
 *
 * The createv function is primarily useful in cases where views are constructed
 * from C itself
 */
static void easy_creation(void)
{
    lcbex_vopt_t *optarry;
    lcbex_vopt_t **vopt_list;
    size_t noptions;
    lcb_error_t err;
    char *errstr;
    char *qstr;
    size_t ii;

    err = lcbex_vopt_createv(&optarry, &noptions, &errstr,
                           "stale", "false",
                           "limit", "100",
                           "startkey_docid", "aass_brewery",
                           /* end the vararg-list with a NULL */
                           NULL
                          );
    assert(err == LCB_SUCCESS);
    /**
     * Now make a query string for it
     */

    vopt_list = malloc(sizeof(*vopt_list) * noptions);

    for (ii = 0; ii < noptions; ii++) {
        vopt_list[ii] = optarry + ii;
    }

    qstr = lcbex_vqstr_make_uri("a_design", -1,
                              "a_view", -1,
                              (const lcbex_vopt_t * const *)vopt_list, noptions);

    printf("Query string from createv: %s\n", qstr);

    /* clean it up */
    lcbex_vopt_cleanup_list(&optarry, noptions, 1);
    free(optarry);
    free(vopt_list);
    free(qstr);
}


static void http_callback(lcb_http_request_t req,
                          lcb_t instance,
                          const void *cookie,
                          lcb_error_t err,
                          const lcb_http_resp_t *resp)
{
    if (err != LCB_SUCCESS || resp->v.v0.status != 200 || resp->v.v0.nbytes == 0) {
        printf("Something's wrong with the view\n");
        return;
    }
    printf("Got view result %.*s\n",
           (int)resp->v.v0.nbytes,
           (const char *)resp->v.v0.bytes);

    (void)req;
    (void)instance;
    (void)cookie;
}
/**
 * This puts it all together. Note that this may fail if you don't have the
 * proper stuff set up.
 *
 * This will query the by_location view of the beer design doc and return
 * all entries which are located in Nevada
 *
 * Some error handling omitted here.
 */
static void view_with_options(void)
{
    lcb_t instance;
    lcb_error_t err;
    struct lcb_create_st create_opts;
    lcb_http_cmd_t htcmd;
    lcb_http_request_t htreq;

    lcbex_vopt_t *vopt_list[4];

    lcbex_vopt_t
    opt_group, opt_grouplevel, opt_startkey, opt_endkey;

    char *qstr;
    char *errstr;
    const char *viewkey_start = "[\"United States\", \"Nevada\", \"A\"]";
    const char *viewkey_end = "[\"United States\", \"Nevada\", \"Z\"]";

    int optname;
    int optval;

    memset(&create_opts, 0, sizeof(create_opts));
    memset(&htcmd, 0, sizeof(htcmd));

    create_opts.v.v1.bucket = "beer-sample";

    err = lcb_create(&instance, &create_opts);

    if (err != LCB_SUCCESS) {
        printf("Couldn't create: %s\n", lcb_strerror(instance, err));
        return;
    }

    err = lcb_connect(instance);
    if (err != LCB_SUCCESS) {
        printf("couldn't connect: %s\n", lcb_strerror(instance, err));
        lcb_destroy(instance);
        return;
    }

    err = lcb_wait(instance);
    if (err !=  LCB_SUCCESS) {
        printf("lcb_wait: %s\n", lcb_strerror(instance, err));
        lcb_destroy(instance);
        return;
    }

    optname = LCBEX_VOPT_OPT_GROUP;
    optval = 1; /* boolean true */
    err = lcbex_vopt_assign(&opt_group,
                          &optname, 0,
                          &optval, 0,
                          LCBEX_VOPT_F_OPTNAME_NUMERIC | LCBEX_VOPT_F_OPTVAL_NUMERIC,
                          &errstr);
    assert(err == LCB_SUCCESS);

    optname = LCBEX_VOPT_OPT_GROUP_LEVEL;
    optval = 3;
    err = lcbex_vopt_assign(&opt_grouplevel,
                          &optname, 0,
                          &optval, 0,
                          LCBEX_VOPT_F_OPTNAME_NUMERIC | LCBEX_VOPT_F_OPTVAL_NUMERIC,
                          &errstr);
    assert(err == LCB_SUCCESS);

    optname = LCBEX_VOPT_OPT_STARTKEY;
    err = lcbex_vopt_assign(&opt_startkey,
                          &optname, 0,
                          viewkey_start, -1,
                          LCBEX_VOPT_F_OPTNAME_NUMERIC | LCBEX_VOPT_F_PCTENCODE,
                          &errstr);
    assert(err == LCB_SUCCESS);

    /** We can also percent-encode option values :) */
    optname = LCBEX_VOPT_OPT_ENDKEY;
    err = lcbex_vopt_assign(&opt_endkey,
                          &optname, 0,
                          viewkey_end, -1,
                          LCBEX_VOPT_F_OPTNAME_NUMERIC | LCBEX_VOPT_F_PCTENCODE,
                          &errstr);
    assert(err == LCB_SUCCESS);

    vopt_list[0] = &opt_group;
    vopt_list[1] = &opt_grouplevel;
    vopt_list[2] = &opt_startkey;
    vopt_list[3] = &opt_endkey;

    qstr = lcbex_vqstr_make_uri("beer", -1,
                              "by_location", -1,
                              (const lcbex_vopt_t * const *)vopt_list, 4);
    assert(qstr);

    printf("Using %s as query string\n", qstr);

    lcb_set_http_complete_callback(instance, http_callback);

    htcmd.v.v1.path = qstr;
    htcmd.v.v1.npath = strlen(qstr);

    err = lcb_make_http_request(instance,
                                NULL, LCB_HTTP_TYPE_VIEW, &htcmd, &htreq);
    assert(err == LCB_SUCCESS);

    /**
     * Now we can free our stuff before we wait
     */
    lcbex_vopt_cleanup_list(vopt_list, 4, 0);
    free(qstr);

    err = lcb_wait(instance);
    assert(err == LCB_SUCCESS);
    lcb_destroy(instance);
}

#define run_example(name) \
    printf("=== Running '%s' ===\n", #name); \
    name(); \
    printf("=== Done ===\n\n");


int main(void)
{
    run_example(create_single_vopt);
    run_example(create_with_constants);
    run_example(create_invalid_options);
    run_example(easy_creation);
    run_example(view_with_options);
    return 0;
}
