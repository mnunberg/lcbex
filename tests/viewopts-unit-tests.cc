#include <gtest/gtest.h>
#include <lcbex/viewopts.h>
#include <iostream>
#include <list>

using namespace std;

class VoptUnitTests : public ::testing::Test
{
public:
    /**
     * Checks an option to ensure that it can accept boolean-type values
     * @param optname the option name
     * @param optid the option ID. May be 0
     */
    void assertBooleanOption(const char *optname, int optid);

    /**
     * Checks an option to ensure it can accept numeric type options
     * (both numeric strings and proper ints)
     * @param optname the option name
     * @param optid the option id
     */
    void assertNumericOption(const char *optname, int optid);

    /**
     * Checks an option to ensure it can accept arbitrary strings
     * @param optname the option name
     * @param optid the option id
     */
    void assertStringOption(const char *optname, int optid);

    /**
     * Converts an assigned lcb_vopt_t into a key=value string
     * @param vopt an assigned view option
     * @param out a std::string which will be populated with the value
     */
    void getKvString(lcbex_vopt_t *vopt, string &out);

    /**
     * Ensures that the option is serialized into its expected "key=value"
     * portion
     * @param key the option name
     * @param value the option string value
     */
    void assertKvEquals(lcbex_vopt_t *vopt, string &key, string &value);


    void assertKvEquals(lcbex_vopt_t *vopt, const char *key, const char *value) {
        string param_k = string(key);
        string param_v = string(value);
        assertKvEquals(vopt, param_k, param_v);
    }

    /**
     * Assigns a key-value pair to an lcb_vopt_t.
     *
     * The variants 'SS', 'SI', etc. refer to the type of option and value
     * passed. 'S' indicates a string, and 'I' indicates an int.
     *
     * @param vopt a pointer to an uninitialized lcb_vopt_t
     * @param k the option name. This must be a recognized option
     * @param v the option value. This must be a valid value for the option
     * @param flags for lcb_vopt_assign
     *
     * @return an error status
     */
    lcb_error_t voptAssignSS(lcbex_vopt_t *vopt, const char *k, const char *v,
                             int flags = 0) {
        char *errstr;
        return lcbex_vopt_assign(vopt,
                               k, strlen(k),
                               v, strlen(v),
                               flags,
                               &errstr);
    }

    lcb_error_t voptAssignSI(lcbex_vopt_t *vopt, const char *k, int v) {
        char *errstr;
        return lcbex_vopt_assign(vopt,
                               k, strlen(k),
                               &v, 0,
                               LCBEX_VOPT_F_OPTVAL_NUMERIC,
                               &errstr);
    }

    lcb_error_t voptAssignIS(lcbex_vopt_t *vopt, int k, const char *v) {
        char *errstr;
        return lcbex_vopt_assign(vopt, &k, 0, v, strlen(v),
                               LCBEX_VOPT_F_OPTNAME_NUMERIC,
                               &errstr);
    }

    lcb_error_t voptAssignII(lcbex_vopt_t *vopt, int k, int v) {
        char *errstr;
        return lcbex_vopt_assign(vopt, &k, 0, &v, 0,
                               LCBEX_VOPT_F_OPTNAME_NUMERIC | LCBEX_VOPT_F_OPTVAL_NUMERIC,
                               &errstr);
    }

    /**
     * Assign an option to an lcb_vopt_t and assert its serialized value equals
     * its passed value.
     *
     * @param optid the option ID
     * @param k the option name
     * @param v the option value. This is also the expected serialized value
     */
    void voptAssignAssertISS(int optid, const char *k, const char *v) {
        lcbex_vopt_t vopt;
        lcb_error_t err;

        err = voptAssignSS(&vopt, k, v);
        ASSERT_EQ(LCB_SUCCESS, err);
        assertKvEquals(&vopt, k, v);
        lcbex_vopt_cleanup(&vopt);

        if (!optid) {
            return;
        }
        err = voptAssignIS(&vopt, optid, v);
        ASSERT_EQ(LCB_SUCCESS, err);
        assertKvEquals(&vopt, k, v);
        lcbex_vopt_cleanup(&vopt);

    }

    /**
     * Like voptAssignAssertISS, but also accepts a numeric option, which when
     * serialized is expected to be equal to 'k'
     */
    void voptAssignAssertISIS(int optid, const char *k, int optval, const char *v) {
        voptAssignAssertISS(optid, k, v);
        lcbex_vopt_t vopt;
        lcb_error_t err;

        err = voptAssignSI(&vopt, k, optval);
        ASSERT_EQ(LCB_SUCCESS, err);
        assertKvEquals(&vopt, k, v);
        lcbex_vopt_cleanup(&vopt);

        if (!optid) {
            return;
        }

        err = voptAssignII(&vopt, optid, optval);
        ASSERT_EQ(LCB_SUCCESS, err);
        assertKvEquals(&vopt, k, v);
        lcbex_vopt_cleanup(&vopt);
    }

    /**
     * Asserts that trying to assign an invalid value to an option results in
     * an error
     * @param optid the option ID
     * @param k the option name
     * @param v an invalid option string
     */
    void voptAssignAssertFailISS(int optid, const char *k, const char *v) {
        lcbex_vopt_t vopt;
        lcb_error_t err;

        err = voptAssignSS(&vopt, k, v);
        ASSERT_EQ(LCB_EINVAL, err);
        lcbex_vopt_cleanup(&vopt);
        if (!optid) {
            return;
        }

        err = voptAssignIS(&vopt, optid, v);
        ASSERT_EQ(LCB_EINVAL, err);
        lcbex_vopt_cleanup(&vopt);
    }

    /**
     * Asserts that serializing an option/value pair results in the value
     * being coerced to an expected string
     * @param optid the option ID
     * @param k the option name
     * @param optval a numeric variant of the value
     * @param v a string valriant of the value
     * @param exp the expected serialized value
     */
    void voptAssignAssertXfrmISIS(int optid, const char *k,
                                  int optval, const char *v,
                                  const char *exp) {
        lcbex_vopt_t vopt;
        lcb_error_t err;
        err = voptAssignSS(&vopt, k, v);
        ASSERT_EQ(LCB_SUCCESS, err);
        assertKvEquals(&vopt, k, exp);
        lcbex_vopt_cleanup(&vopt);

        err = voptAssignSI(&vopt, k, optval);
        ASSERT_EQ(LCB_SUCCESS, err);
        assertKvEquals(&vopt, k, exp);
        lcbex_vopt_cleanup(&vopt);

        if (!optid) {
            return;
        }

        err = voptAssignIS(&vopt, optid, v);
        ASSERT_EQ(LCB_SUCCESS, err);
        assertKvEquals(&vopt, k, exp);
        lcbex_vopt_cleanup(&vopt);

        err = voptAssignII(&vopt, optid, optval);
        ASSERT_EQ(LCB_SUCCESS, err);
        assertKvEquals(&vopt, k, exp);
    }

};

struct option_info {
    const char *name;
    int id;
};

typedef list<option_info>::iterator optlist_iterator;

void
filterOptions(list<option_info> &opts, const char *type)
{
#define XX(opt_id, opt_name, opt_type) \
    if (strcmp(type, #opt_type) == 0) { \
        option_info oi; \
        oi.name = opt_name; \
        oi.id = opt_id; \
        opts.push_back(oi); \
    }
    LCBEX_XVOPT
#undef XX
}

void
VoptUnitTests::assertKvEquals(lcbex_vopt_t *vopt,
                              string &key,
                              string &value)
{
    string expected = key;
    string got;
    expected += "=";
    expected += value;

    getKvString(vopt, got);
    ASSERT_STREQ(expected.c_str(), got.c_str());
}

/**
 * Wrapper around lcb_vqstr_write which returns a serialized lcb_vopt_t
 * @param vopt an assigned and initialized vopt
 */
void
VoptUnitTests::getKvString(lcbex_vopt_t *vopt, string &ret)
{
    size_t nbuf;

    lcbex_vopt_t **vopt_p = &vopt;

    size_t needed_len = lcbex_vqstr_calc_len(vopt_p, 1);
    ASSERT_GT(needed_len, 0);

    char *buf = new char[needed_len + 1];
    nbuf = lcbex_vqstr_write(vopt_p, 1, buf);

    ASSERT_GT(nbuf, 0);
    ASSERT_LE(nbuf, needed_len);

    ret.assign(buf + 1, nbuf - 1);

    delete[] buf;
}

void
VoptUnitTests::assertBooleanOption(const char *optname, int optid)
{
    voptAssignAssertISIS(optid, optname, 1, "true");
    voptAssignAssertISIS(optid, optname, 0, "false");
    voptAssignAssertFailISS(optid, optname, "bad_value");
}

void
VoptUnitTests::assertStringOption(const char *optname, int optid)
{
    lcbex_vopt_t vopt;
    lcb_error_t err;
    voptAssignAssertISS(optid, optname, "string_value");

    // TODO: might we want to stringify numbers?
    err = voptAssignSI(&vopt, optname, 42);
    ASSERT_EQ(LCB_EINVAL, err);
    lcbex_vopt_cleanup(&vopt);
}

void
VoptUnitTests::assertNumericOption(const char *optname, int optid)
{
    int ivals[] = { 42, -1, 0, 1 };
    int ii;
    for (ii = 0; ii < sizeof(ivals) / sizeof(int); ii++) {

        int curval = ivals[ii];
        char sbuf[64];
        sprintf(sbuf, "%d", curval);

        voptAssignAssertISIS(optid, optname, curval, sbuf);
    }

    voptAssignAssertFailISS(optid, optname, "non-numeric");
}

/**
 * Tests:
 */


/**
 * @test Verify all boolean options can accept boolean-type values
 * @see assertBooleanOption
 */
TEST_F(VoptUnitTests, testBooleanOptions)
{
    list<option_info> opts;
    filterOptions(opts, "bool");
    for (optlist_iterator iter = opts.begin(); iter != opts.end(); iter++) {
        assertBooleanOption(iter->name, iter->id);
    }
}

/**
 * @test verify all numeric options can accept number-type values
 * @see assertNumericOption
 */
TEST_F(VoptUnitTests, testNumericOptions)
{
    list<option_info> opts;
    filterOptions(opts, "num");
    for (optlist_iterator iter = opts.begin(); iter != opts.end(); iter++) {
        assertNumericOption(iter->name, iter->id);
    }
}

/**
 * @test verify all string options can accept arbitrary string values
 * @see assertStringOption
 */
TEST_F(VoptUnitTests, testStringOptions)
{
    list<option_info> opts;
    filterOptions(opts, "string");
    filterOptions(opts, "jval");
    filterOptions(opts, "jarry");

    for (optlist_iterator iter = opts.begin(); iter != opts.end(); iter++) {
        assertStringOption(iter->name, iter->id);
    }
}

/**
 * @test Verify the 'on_error' option values
 * @pre check with the values "stop" and "continue"
 * @post vopt is serialized to the expected string
 *
 * @pre check with an invalid value e.g. "bad_value"
 * @post LCB_EINVAL is returned
 */
TEST_F(VoptUnitTests, testOnError)
{
    voptAssignAssertISS(LCBEX_VOPT_OPT_ONERROR, "on_error", "stop");
    voptAssignAssertISS(LCBEX_VOPT_OPT_ONERROR, "on_error", "continue");
    voptAssignAssertFailISS(LCBEX_VOPT_OPT_ONERROR, "on_error", "bad_value");
}

/**
 * @test verify the 'stale' option values
 * @pre Check the string values 'false', 'ok', and 'update_after'
 * @post the vopt is serialized verbatim to these values without any errors
 *
 * @pre Use the numeric boolean values for the stale option
 * @post false values are serialized to "false", true values are serialized as
 * "ok"
 *
 * @pre Use the string "true" for the stale value
 * @post vopt is serialized to "ok"
 *
 * @pre Use an invalid value "invalid" for stale
 * @post LCB_EINVAL is returned
 *
 */
TEST_F(VoptUnitTests, testStale)
{

    // "positive" values
    voptAssignAssertISS(LCBEX_VOPT_OPT_STALE, "stale", "false");
    voptAssignAssertISS(LCBEX_VOPT_OPT_STALE, "stale", "ok");
    voptAssignAssertISS(LCBEX_VOPT_OPT_STALE, "stale", "update_after");

    // false boolean -> "false"
    voptAssignAssertISIS(LCBEX_VOPT_OPT_STALE,
                         "stale",
                         0, "false");

    // true boolean -> "ok"
    voptAssignAssertISIS(LCBEX_VOPT_OPT_STALE,
                         "stale",
                         1, "ok");

    // string boolean -> proper values
    voptAssignAssertXfrmISIS(LCBEX_VOPT_OPT_STALE, "stale",
                             1, "true",
                             "ok");

    voptAssignAssertFailISS(LCBEX_VOPT_OPT_STALE, "stale", "invalid");
}

/**
 * @test Verify percent-encoding feature
 * @pre Set the startkey_docid value to "a space". Specify the F_PCTENCODE
 * flag to lcb_vopt_assign to make it percent-encode
 * @post Resultant serialized value is percent-encoded i.e. "a%20space"
 */
TEST_F(VoptUnitTests, testPercentEncoding)
{
    lcbex_vopt_t vopt;
    lcb_error_t err;
    char *errstr;

    err = lcbex_vopt_assign(&vopt, "startkey_docid", -1, "a space", -1,
                          LCBEX_VOPT_F_PCTENCODE,
                          &errstr);

    ASSERT_EQ(LCB_SUCCESS, err);
    assertKvEquals(&vopt, "startkey_docid", "a%20space");
    lcbex_vopt_cleanup(&vopt);
}

/**
 * @test Verify that a complete URI path can be generated from a list of
 * lcb_vopt_t
 * @pre Set stale to false and startkey_docid to a value with percent encoding
 * @post URI path is returned and its contents match the expected value
 */
TEST_F(VoptUnitTests, testUriCreation)
{
    const char *expected =
        "_design/ddoc/_view/vdoc?stale=false&startkey_docid=a%20space";

    lcb_error_t err;
    lcbex_vopt_t vopt_stale;
    lcbex_vopt_t vopt_skey_docid;

    ASSERT_EQ(LCB_SUCCESS,
              voptAssignSS(&vopt_stale, "stale", "false"));

    ASSERT_EQ(LCB_SUCCESS,
              voptAssignSS(&vopt_skey_docid, "startkey_docid", "a space",
                           LCBEX_VOPT_F_PCTENCODE));

    lcbex_vopt_t *vopt_list[2];
    vopt_list[0] = &vopt_stale;
    vopt_list[1] = &vopt_skey_docid;

    char *uri = lcbex_vqstr_make_uri("ddoc", -1,
                                   "vdoc", -1,
                                   vopt_list, 2);

    ASSERT_FALSE(uri == NULL);
    ASSERT_STREQ(expected, uri);
    free(uri);
    lcbex_vopt_cleanup_list(vopt_list, 2, 0);
}

/**
 * @test Test vopt assignment with constant strings
 * @pre Specify the VOPT_F_OPTVAL_CONSTANT/VOPT_F_OPTNAME_CONSTANT flags when
 * using a constant string
 *
 * @post is constructed OK. lcb_vopt_cleanup doesn't crash.
 */
TEST_F(VoptUnitTests, testNoAlloc)
{
    lcb_error_t err;
    lcbex_vopt_t vopt;
    char *errstr;
    err = lcbex_vopt_assign(&vopt,
                          "startkey_docid", -1,
                          "constant_value", -1,
                          LCBEX_VOPT_F_OPTNAME_CONSTANT | LCBEX_VOPT_F_OPTVAL_CONSTANT,
                          &errstr);

    ASSERT_EQ(LCB_SUCCESS, err);
    assertKvEquals(&vopt, "startkey_docid", "constant_value");
    lcbex_vopt_cleanup(&vopt);
}

/**
 * @test Check passthrough options
 * @pre Pass unrecognized view options using the F_PASSTHROUGH flag
 * @post The assignment does not fail and the URI is serialized as expected
 * @pre Pass unrecognized view option IDs using the F_PASSTHROUGH flag
 * @post Assignment returns LCB_EINVAL
 */
TEST_F(VoptUnitTests, testPassthrough)
{
    lcb_error_t err;
    lcbex_vopt_t vopt;
    char *errstr;
    int optval;
    int optid;

    err = voptAssignSS(&vopt, "dummy_option", "dummy_value",
                       LCBEX_VOPT_F_PASSTHROUGH);
    ASSERT_EQ(LCB_SUCCESS, err);
    assertKvEquals(&vopt, "dummy_option", "dummy_value");
    lcbex_vopt_cleanup(&vopt);

    optval = 0;
    optid = 50;
    err = lcbex_vopt_assign(&vopt, &optid, 0,
                          &optval, 0,
                          LCBEX_VOPT_F_PASSTHROUGH |
                          LCBEX_VOPT_F_OPTVAL_NUMERIC |
                          LCBEX_VOPT_F_OPTNAME_NUMERIC,
                          &errstr);
    ASSERT_EQ(LCB_EINVAL, err);
    lcbex_vopt_cleanup(&vopt);
}

/**
 * @test Check varargs vopt list creation
 * @pre Call the createv function with a NULL-terminated argument list of valid
 * view options and values
 * @post Assignment is successful and URI is as expected
 *
 * @pre Call the createv function with a NULL-terminated argument list of invalid
 * view options and values
 * @post Assignment fails with LCB_EINVAL. No crashes :)
 *
 * @pre Call the createv function with no arguments
 * @post Returns EINVAL
 *
 * @pre call the createv function with un-even arguments
 * @post returns EINVAL
 */
TEST_F(VoptUnitTests, testCreateVarArgs)
{
    lcb_error_t err;
    lcbex_vopt_t *vopt_list = NULL;
    size_t nvopts = 0;
    char *errstr;
    int ii;
    lcbex_vopt_t *vopt_ptrs[4];

    err = lcbex_vopt_createv(&vopt_list, &nvopts, &errstr,
                           "stale", "false",
                           "on_error", "continue",
                           "reduce", "false",
                           "limit", "20",
                           NULL);

    ASSERT_EQ(LCB_SUCCESS, err);
    ASSERT_EQ(4, nvopts);
    ASSERT_FALSE(vopt_list == NULL);

    for (ii = 0; ii < 4; ii++) {
        vopt_ptrs[ii] = vopt_list + ii;
    }
    /* make me a URI */
    char *uri = lcbex_vqstr_make_uri("ddoc", -1,
                                   "vdoc", -1,
                                   vopt_ptrs, nvopts);
    ASSERT_FALSE(uri == NULL);

    ASSERT_STREQ(
        "_design/ddoc/_view/vdoc?"
        "stale=false&on_error=continue&reduce=false&limit=20",
        uri);

    free(uri);
    lcbex_vopt_cleanup_list(&vopt_list, nvopts, 1);
    free(vopt_list);

    /**
     * Try with invalid options
     */
    vopt_list = (lcbex_vopt_t *) - 1;
    err = lcbex_vopt_createv(&vopt_list, &nvopts, &errstr,
                           "stale", "false",
                           "bob", "loblaw",
                           NULL);

    ASSERT_EQ(LCB_EINVAL, err);
    ASSERT_EQ(NULL, vopt_list);

    err = lcbex_vopt_createv(&vopt_list, &nvopts, &errstr, NULL);
    ASSERT_EQ(LCB_EINVAL, err);
    ASSERT_EQ(NULL, vopt_list);

    err = lcbex_vopt_createv(&vopt_list, &nvopts, &errstr, "on_error", NULL);
    ASSERT_EQ(LCB_EINVAL, err);
    ASSERT_EQ(NULL, vopt_list);
}


/**
 * @test Check what happens when the key or value names are zero
 * @pre Check various combinations of NULL, empty strings, and zero lengths
 * @post They all return LCB_EINVAL on assignment, and don't crash
 */
TEST_F(VoptUnitTests, testZeroLength)
{
    lcb_error_t err;
    lcbex_vopt_t vopt;
    char *errstr;

    err = lcbex_vopt_assign(&vopt,
                          "", -1, "", -1, 0, &errstr);
    ASSERT_EQ(LCB_EINVAL, err);
    lcbex_vopt_cleanup(&vopt);

    err = lcbex_vopt_assign(&vopt,
                          "", -1, "", -1, LCBEX_VOPT_F_PASSTHROUGH, &errstr);
    ASSERT_EQ(LCB_EINVAL, err);
    lcbex_vopt_cleanup(&vopt);


    err = lcbex_vopt_assign(&vopt, NULL, 0, NULL, 0, 0, &errstr);
    ASSERT_EQ(LCB_EINVAL, err);
    lcbex_vopt_cleanup(&vopt);

    err = lcbex_vopt_assign(&vopt, NULL, 0, "value", -1,
                          LCBEX_VOPT_F_PASSTHROUGH, &errstr);
    ASSERT_EQ(LCB_EINVAL, err);
    lcbex_vopt_cleanup(&vopt);

    err = lcbex_vopt_assign(&vopt, "on_error", -1, NULL, 0, 0, &errstr);
    ASSERT_EQ(LCB_EINVAL, err);
    lcbex_vopt_cleanup(&vopt);
}
