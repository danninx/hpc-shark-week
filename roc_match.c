/*
 * roc_match.c
 * Implementation of ROC pattern matching (Definition 3.2.3)
 *
 * Match rules (reproduced for reference):
 *
 *   (M1)  v# ~ v#        — bindable matches bindable (same underlying value)
 *   (M2)  v# ~ v         — bindable matches unbindable (same underlying value)
 *   (M3)  v# ~ n?        — bindable matches wildcard  → bind n? := v
 *   (M4)  v  ~ v         — unbindable matches itself
 *   (M5)  v  ~ v#        — unbindable matches bindable (same underlying value)
 *   (M6)  v(#) ~ x(#)    — tuple matching: ∀i, vi ~ xi
 *                          the outer bindable tag (#) on a tuple is allowed
 *                          on either side (or neither); matching is element-wise.
 *
 * Critical non-rule:
 *   v !~ n?              — unbindable value CANNOT match a wildcard
 */

#include "roc_match.h"

/* ================================================================
 * Internal helpers
 * ================================================================ */

/*
 * strip_bindable — return the inner value node of a NODE_BINDABLE,
 * or the node itself if it is already an unwrapped value.
 * e.g.  strip(v#) = v,  strip(v) = v
 */
static const Node *strip_bindable(const Node *n) {
    return (n->kind == NODE_BINDABLE) ? n->child : n;
}

/*
 * is_bindable — true if the top-level node carries a # tag.
 */
static int is_bindable(const Node *n) {
    return n->kind == NODE_BINDABLE;
}

/* ================================================================
 * node_equal — deep structural equality on two Nodes
 *
 * Both nodes are expected to be values (is_value == 1), i.e.
 * wildcards should not appear.  If one somehow does it will
 * compare unequal to everything.
 * ================================================================ */
int node_equal(const Node *a, const Node *b) {
    /* strip any bindable wrappers before comparing core values */
    a = strip_bindable(a);
    b = strip_bindable(b);

    if (a->kind != b->kind) return 0;

    switch (a->kind) {
    case NODE_NAME:
    case NODE_AGENT:
        return strcmp(a->name, b->name) == 0;

    case NODE_NIL:
        return 1;

    case NODE_TUPLE:
        if (a->nchildren != b->nchildren) return 0;
        for (int i = 0; i < a->nchildren; i++)
            if (!node_equal(a->children[i], b->children[i])) return 0;
        return 1;

    case NODE_BINDABLE:
        /* already stripped above — should not reach here */
        return node_equal(a->child, b->child);

    case NODE_WILDCARD:
        /* wildcards are not values; treat as unequal */
        return 0;
    }
    return 0;
}

/* ================================================================
 * BindingList operations
 * ================================================================ */

BindingList *binding_list_new(void) {
    BindingList *bl = calloc(1, sizeof *bl);
    bl->capacity = 8;
    bl->entries  = malloc(bl->capacity * sizeof(Binding));
    bl->count    = 0;
    return bl;
}

void binding_list_free(BindingList *bl) {
    if (!bl) return;
    for (int i = 0; i < bl->count; i++)
        free(bl->entries[i].wildcard_name);
    free(bl->entries);
    free(bl);
}

/* Append one binding — internal use only; no duplicate check here. */
static void bl_push(BindingList *bl, const char *wc_name, Node *value) {
    if (bl->count == bl->capacity) {
        bl->capacity *= 2;
        bl->entries = realloc(bl->entries, bl->capacity * sizeof(Binding));
    }
    bl->entries[bl->count].wildcard_name = strdup(wc_name);
    bl->entries[bl->count].bound_value   = value;
    bl->count++;
}

Node *binding_list_lookup(const BindingList *bl, const char *name) {
    for (int i = 0; i < bl->count; i++)
        if (strcmp(bl->entries[i].wildcard_name, name) == 0)
            return bl->entries[i].bound_value;
    return NULL;
}

void binding_list_print(const BindingList *bl) {
    if (!bl || bl->count == 0) {
        printf("  (no bindings)\n");
        return;
    }
    for (int i = 0; i < bl->count; i++) {
        printf("  %s? := ", bl->entries[i].wildcard_name);
        node_print(bl->entries[i].bound_value, 0);
    }
}

/* ================================================================
 * roc_match — Definition 3.2.3
 *
 * To support rollback on failure we record how many bindings
 * existed before we entered this call and restore the count if
 * the match fails.  This is safe because bl_push only appends
 * and never reallocates in a way that would invalidate already-
 * stored pointers (the entries array may move but we restore by
 * count, not by pointer).
 * ================================================================ */
int roc_match(const Node *value, const Node *pattern, BindingList *bl) {
    /* Save binding count so we can roll back on failure */
    int saved_count = bl->count;

    /* ---- Unwrap any leading BINDABLE wrappers ---- */
    int val_is_bindable = is_bindable(value);
    (void)is_bindable(pattern); /* pattern bindability is handled by strip_bindable below */

    const Node *v = strip_bindable(value);    /* core value   */
    const Node *x = strip_bindable(pattern);  /* core pattern */

    /*
     * Rule M3: v# ~ n?
     * A bindable value matches a wildcard; bind the wildcard name
     * to the core value v (the # wrapper is stripped per a{v/x}).
     * An unbindable value CANNOT match a wildcard (see non-rule).
     */
    if (x->kind == NODE_WILDCARD) {
        if (!val_is_bindable) goto fail; /* non-rule: v !~ n? */
        bl_push(bl, x->name, (Node *)v);
        return 1;
    }

    /*
     * Rules M1 / M2 / M4 / M5:
     * When neither side is a tuple and neither is a wildcard,
     * the core values must be structurally equal.
     * The bindable flag on either side is irrelevant to equality
     * (M1: v# ~ v#, M2: v# ~ v, M4: v ~ v, M5: v ~ v#).
     */
    if (v->kind != NODE_TUPLE && x->kind != NODE_TUPLE) {
        if (!node_equal(v, x)) goto fail;
        return 1;
    }

    /*
     * Rule M6: tuple matching — element-wise.
     * v(#) ~ x(#)  iff  ∀i, vi ~ xi
     *
     * The outer bindable tag is immaterial to element-wise matching;
     * the elements themselves carry their own bindable/unbindable tags.
     * Both sides must be tuples of the same arity.
     */
    if (v->kind == NODE_TUPLE && x->kind == NODE_TUPLE) {
        if (v->nchildren != x->nchildren) goto fail;
        for (int i = 0; i < v->nchildren; i++) {
            if (!roc_match(v->children[i], x->children[i], bl))
                goto fail;
        }
        return 1;
    }

    /* One side is a tuple and the other is not — cannot match. */

fail:
    /* Roll back any bindings added during this (failed) call */
    for (int i = saved_count; i < bl->count; i++)
        free(bl->entries[i].wildcard_name);
    bl->count = saved_count;
    return 0;
}

/* ================================================================
 * Node constructors
 * ================================================================ */

Node *mk_name(const char *s) {
    Node *n = calloc(1, sizeof *n);
    n->kind = NODE_NAME; n->name = strdup(s); n->is_value = 1;
    return n;
}
Node *mk_agent(const char *s) {
    Node *n = calloc(1, sizeof *n);
    n->kind = NODE_AGENT; n->name = strdup(s); n->is_value = 1;
    return n;
}
Node *mk_nil(void) {
    Node *n = calloc(1, sizeof *n);
    n->kind = NODE_NIL; n->is_value = 1;
    return n;
}
Node *mk_wc(const char *s) {
    Node *n = calloc(1, sizeof *n);
    n->kind = NODE_WILDCARD; n->name = strdup(s); n->is_value = 0;
    return n;
}
Node *mk_bind(Node *v) {
    Node *n = calloc(1, sizeof *n);
    n->kind = NODE_BINDABLE; n->child = v; n->is_value = 1;
    return n;
}
Node *mk_tuple_from_array(Node **children, int nc, int all_values) {
    Node *n = calloc(1, sizeof *n);
    n->kind      = NODE_TUPLE;
    n->nchildren = nc;
    n->children  = malloc(nc * sizeof(Node *));
    memcpy(n->children, children, nc * sizeof(Node *));
    n->is_value  = all_values;
    return n;
}

/* ================================================================
 * node_print — pretty printer
 * ================================================================ */
void node_print(const Node *n, int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
    switch (n->kind) {
    case NODE_NAME:     printf("Name(%s)\n",      n->name); break;
    case NODE_AGENT:    printf("Agent(%s)\n",     n->name); break;
    case NODE_NIL:      printf("Nil\n");                    break;
    case NODE_WILDCARD: printf("Wildcard(%s?)\n", n->name); break;
    case NODE_BINDABLE:
        printf("Bindable(\n");
        node_print(n->child, depth + 1);
        for (int i = 0; i < depth; i++) printf("  ");
        printf(")\n");
        break;
    case NODE_TUPLE:
        printf("Tuple[\n");
        for (int i = 0; i < n->nchildren; i++)
            node_print(n->children[i], depth + 1);
        for (int i = 0; i < depth; i++) printf("  ");
        printf("]\n");
        break;
    }
}
