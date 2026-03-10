/*
 * roc_match_test.c
 * Test driver for roc_match(), covering every rule in Definition 3.2.3
 * and explicitly verifying the critical non-rule (v !~ n?).
 *
 * Each test prints: rule label, value, pattern, expected result, actual result.
 */

#include "roc_match.h"

/* ----------------------------------------------------------------
 * Small helper: build a 2-element tuple node inline
 * ---------------------------------------------------------------- */
static Node *tuple2(Node *a, Node *b) {
    Node *children[2] = { a, b };
    int all_values = a->is_value && b->is_value;
    return mk_tuple_from_array(children, 2, all_values);
}
static Node *tuple3(Node *a, Node *b, Node *c) {
    Node *children[3] = { a, b, c };
    int all_values = a->is_value && b->is_value && c->is_value;
    return mk_tuple_from_array(children, 3, all_values);
}

/* ----------------------------------------------------------------
 * run_test — execute one match and report
 * ---------------------------------------------------------------- */
static int tests_run    = 0;
static int tests_passed = 0;

static void run_test(const char *label,
                     Node *value, Node *pattern,
                     int expect_match) {
    tests_run++;
    BindingList *bl = binding_list_new();
    int result = roc_match(value, pattern, bl);

    const char *pass_fail = (result == expect_match) ? "PASS" : "FAIL";
    if (result == expect_match) tests_passed++;

    printf("[%s] %s\n", pass_fail, label);
    printf("  value  : "); node_print(value,   0);
    printf("  pattern: "); node_print(pattern, 0);
    printf("  expect : %s  got: %s\n",
           expect_match ? "MATCH" : "NO MATCH",
           result       ? "MATCH" : "NO MATCH");
    if (result) {
        printf("  bindings:\n");
        binding_list_print(bl);
    }
    printf("\n");

    binding_list_free(bl);
}

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(void) {
    printf("=======================================================\n");
    printf(" ROC Pattern Matcher — Definition 3.2.3 Test Suite\n");
    printf("=======================================================\n\n");

    /* ----------------------------------------------------------
     * Rule M4: v ~ v   (unbindable matches itself)
     * [m,n] ~ [m,n]  and  [m,n] !~ [m,p]
     * ---------------------------------------------------------- */
    printf("--- Rule M4: v ~ v  (unbindable matches itself) ---\n\n");

    run_test("M4a  n ~ n  (same name)",
             mk_name("m"), mk_name("m"), 1);

    run_test("M4b  n ~ n  (different names)",
             mk_name("m"), mk_name("p"), 0);

    run_test("M4c  [m,n] ~ [m,n]  (dissertation example)",
             tuple2(mk_name("m"), mk_name("n")),
             tuple2(mk_name("m"), mk_name("n")), 1);

    run_test("M4d  [m,n] !~ [m,p]  (second element differs)",
             tuple2(mk_name("m"), mk_name("n")),
             tuple2(mk_name("m"), mk_name("p")), 0);

    run_test("M4e  nil ~ nil",
             mk_nil(), mk_nil(), 1);

    run_test("M4f  Agent A ~ Agent A",
             mk_agent("Alpha"), mk_agent("Alpha"), 1);

    run_test("M4g  Agent A ~ Agent B  (different agents)",
             mk_agent("Alpha"), mk_agent("Beta"), 0);

    /* ----------------------------------------------------------
     * Rule M1: v# ~ v#  (bindable matches bindable)
     * ---------------------------------------------------------- */
    printf("--- Rule M1: v# ~ v#  (bindable matches bindable) ---\n\n");

    run_test("M1a  n# ~ n#  (same name)",
             mk_bind(mk_name("m")), mk_bind(mk_name("m")), 1);

    run_test("M1b  n# ~ n#  (different names)",
             mk_bind(mk_name("m")), mk_bind(mk_name("p")), 0);

    run_test("M1c  [m,n#] ~ [m,n#]  (bindable in tuple)",
             tuple2(mk_name("m"), mk_bind(mk_name("n"))),
             tuple2(mk_name("m"), mk_bind(mk_name("n"))), 1);

    /* ----------------------------------------------------------
     * Rule M2: v# ~ v  (bindable matches unbindable)
     * ---------------------------------------------------------- */
    printf("--- Rule M2: v# ~ v  (bindable matches unbindable) ---\n\n");

    run_test("M2a  n# ~ n  (same name)",
             mk_bind(mk_name("m")), mk_name("m"), 1);

    run_test("M2b  [m,n#] ~ [m,n]  (dissertation example)",
             tuple2(mk_name("m"), mk_bind(mk_name("n"))),
             tuple2(mk_name("m"), mk_name("n")), 1);

    run_test("M2c  n# ~ n  (different names — should fail)",
             mk_bind(mk_name("m")), mk_name("p"), 0);

    /* ----------------------------------------------------------
     * Rule M5: v ~ v#  (unbindable matches bindable)
     * ---------------------------------------------------------- */
    printf("--- Rule M5: v ~ v#  (unbindable matches bindable) ---\n\n");

    run_test("M5a  n ~ n#  (same name)",
             mk_name("m"), mk_bind(mk_name("m")), 1);

    run_test("M5b  [m,n] ~ [m,n#]  (mixed tuple)",
             tuple2(mk_name("m"), mk_name("n")),
             tuple2(mk_name("m"), mk_bind(mk_name("n"))), 1);

    run_test("M5c  n ~ n#  (different names — should fail)",
             mk_name("m"), mk_bind(mk_name("p")), 0);

    /* ----------------------------------------------------------
     * Rule M3: v# ~ n?  (bindable matches wildcard — BINDS)
     * ---------------------------------------------------------- */
    printf("--- Rule M3: v# ~ n?  (bindable matches wildcard) ---\n\n");

    run_test("M3a  n# ~ n?  (single bindable → single wildcard)",
             mk_bind(mk_name("m")), mk_wc("k"), 1);

    run_test("M3b  [m,n#] ~ [m,n?]  (dissertation example)",
             tuple2(mk_name("m"), mk_bind(mk_name("n"))),
             tuple2(mk_name("m"), mk_wc("n")), 1);

    run_test("M3c  [m#,p#] ~ [k?,q?]  (two wildcards bound)",
             tuple2(mk_bind(mk_name("m")), mk_bind(mk_name("p"))),
             tuple2(mk_wc("k"), mk_wc("q")), 1);

    run_test("M3d  Agent# ~ k?  (bindable agent matches wildcard)",
             mk_bind(mk_agent("Alpha")), mk_wc("k"), 1);

    /* ----------------------------------------------------------
     * Non-rule: v !~ n?  (unbindable CANNOT match wildcard)
     * ---------------------------------------------------------- */
    printf("--- Non-rule: v !~ n?  (unbindable cannot match wildcard) ---\n\n");

    run_test("NR1  n !~ n?  (unbindable name vs wildcard)",
             mk_name("m"), mk_wc("k"), 0);

    run_test("NR2  [m,n] !~ [m,p?]  (dissertation counter-example)",
             tuple2(mk_name("m"), mk_name("n")),
             tuple2(mk_name("m"), mk_wc("p")), 0);

    run_test("NR3  nil !~ k?  (nil is unbindable)",
             mk_nil(), mk_wc("k"), 0);

    /* ----------------------------------------------------------
     * Rule M6: tuple element-wise matching with mixed bindability
     * ---------------------------------------------------------- */
    printf("--- Rule M6: tuple element-wise (mixed cases) ---\n\n");

    /*  [m, n#, p] ~ [m, q?, p]  →  q := n  */
    run_test("M6a  [m, n#, p] ~ [m, q?, p]  (one wildcard in middle)",
             tuple3(mk_name("m"), mk_bind(mk_name("n")), mk_name("p")),
             tuple3(mk_name("m"), mk_wc("q"),            mk_name("p")), 1);

    /*  [m#, n#] ~ [m, n?]  →  n? := n  (m# ~ m via M2) */
    run_test("M6b  [m#, n#] ~ [m, n?]  (bindable value + wildcard)",
             tuple2(mk_bind(mk_name("m")), mk_bind(mk_name("n"))),
             tuple2(mk_name("m"),          mk_wc("n")), 1);

    /*  [m, n] ~ [m]  (arity mismatch — must fail) */
    run_test("M6c  [m,n] !~ [m]  (arity mismatch)",
             tuple2(mk_name("m"), mk_name("n")),
             mk_tuple_from_array((Node*[]){mk_name("m")}, 1, 1), 0);

    /*  Nested tuple: [[m,n#],p] ~ [[m,k?],p]  →  k := n  */
    {
        Node *inner_v = tuple2(mk_name("m"), mk_bind(mk_name("n")));
        Node *inner_x = tuple2(mk_name("m"), mk_wc("k"));
        run_test("M6d  [[m,n#],p] ~ [[m,k?],p]  (nested tuple wildcard bind)",
                 tuple2(inner_v, mk_name("p")),
                 tuple2(inner_x, mk_name("p")), 1);
    }

    /* ----------------------------------------------------------
     * Rollback test: failed match must leave bindings unchanged
     * ---------------------------------------------------------- */
    printf("--- Rollback: failed branch must not pollute BindingList ---\n\n");
    {
        BindingList *bl = binding_list_new();
        /* Pre-populate with one existing binding */
        Node *pre_val = mk_name("existing");
        /* Manually insert a binding to simulate prior state */
        bl->entries = realloc(bl->entries, 2 * sizeof(Binding));
        bl->entries[0].wildcard_name = strdup("prior");
        bl->entries[0].bound_value   = pre_val;
        bl->count = 1;

        /* Now attempt a failing match: [m,n] ~ [m,k?]  (n unbindable, k? needs bindable) */
        Node *v = tuple2(mk_name("m"), mk_name("n"));
        Node *x = tuple2(mk_name("m"), mk_wc("k"));
        int result = roc_match(v, x, bl);

        int pass = (!result && bl->count == 1 &&
                    strcmp(bl->entries[0].wildcard_name, "prior") == 0);
        printf("[%s] Rollback: failed match leaves prior binding intact\n",
               pass ? "PASS" : "FAIL");
        printf("  bindings after failed attempt:\n");
        binding_list_print(bl);
        printf("\n");
        if (pass) tests_passed++;
        tests_run++;

        binding_list_free(bl);
    }

    /* ----------------------------------------------------------
     * Summary
     * ---------------------------------------------------------- */
    printf("=======================================================\n");
    printf(" Results: %d / %d tests passed\n", tests_passed, tests_run);
    printf("=======================================================\n");
    return (tests_passed == tests_run) ? 0 : 1;
}
