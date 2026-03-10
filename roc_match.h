/*
 * roc_match.h
 * Pattern matching for the Robust Object Calculus (ROC)
 * Implements Definition 3.2.3 of the MOOSE dissertation (Chapter III)
 *
 * Match operator ~  rules (from Def 3.2.3):
 *
 *   (M1)  v#  ~  v#          bindable  matches bindable of same value
 *   (M2)  v#  ~  v           bindable  matches unbindable
 *   (M3)  v#  ~  n?          bindable  matches wildcard  → binds n := v
 *   (M4)  v   ~  v           unbindable matches itself
 *   (M5)  v   ~  v#          unbindable matches bindable of same value
 *   (M6)  v(#) ~ x(#)  iff   ∀i, vi ~ xi   (element-wise tuple matching)
 *
 * Non-rules (explicitly excluded):
 *   v  !~  n?                unbindable value cannot match a wildcard
 *
 * The function roc_match() performs matching and returns:
 *   - 1 (true)  if value v matches pattern x
 *   - 0 (false) otherwise
 *
 * When matching succeeds, wildcard bindings are accumulated into a
 * BindingList: each binding records the wildcard name and the Node*
 * value that was bound to it (the sub-value of a bindable v#).
 */

#ifndef ROC_MATCH_H
#define ROC_MATCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------
 * AST node (shared with the yacc/lex parser)
 * --------------------------------------------------------------- */
typedef enum {
    NODE_NAME,       /* n        — a name                    */
    NODE_AGENT,      /* a        — an agent                  */
    NODE_NIL,        /* nil      — the empty agent           */
    NODE_TUPLE,      /* [e1..ej] — tuple of values/patterns  */
    NODE_BINDABLE,   /* v#       — a bindable value          */
    NODE_WILDCARD    /* n?       — a wildcard pattern        */
} NodeKind;

typedef struct Node {
    NodeKind      kind;
    char         *name;        /* NODE_NAME, NODE_AGENT, NODE_WILDCARD */
    struct Node **children;    /* NODE_TUPLE elements                  */
    int           nchildren;
    struct Node  *child;       /* NODE_BINDABLE inner value            */
    int           is_value;    /* 1 = no wildcards anywhere in subtree */
} Node;

/* ---------------------------------------------------------------
 * Binding: one wildcard name  →  one bound value
 *
 * When  v# ~ n?  fires (rule M3), the name inside n? is bound
 * to the value v carried by v#.  We store:
 *   wildcard_name  — the name from n?  (e.g. "k")
 *   bound_value    — pointer to the Node for v  (NOT v#; the
 *                    bindable wrapper is stripped per the
 *                    dissertation's substitution notation a{v/x})
 * --------------------------------------------------------------- */
typedef struct {
    char  *wildcard_name;   /* name from n?      */
    Node  *bound_value;     /* the matched value */
} Binding;

/* ---------------------------------------------------------------
 * BindingList: dynamic array of Binding entries
 * --------------------------------------------------------------- */
typedef struct {
    Binding *entries;
    int      count;
    int      capacity;
} BindingList;

/* Lifecycle */
BindingList *binding_list_new(void);
void         binding_list_free(BindingList *bl);

/* Lookup: returns bound value for wildcard name, or NULL if unbound */
Node        *binding_list_lookup(const BindingList *bl, const char *name);

/* Print all bindings (for debugging / demo) */
void         binding_list_print(const BindingList *bl);

/* ---------------------------------------------------------------
 * roc_match — the main matching function
 *
 * Parameters:
 *   value    — a Node* that must satisfy is_value == 1
 *   pattern  — a Node* that may contain wildcards
 *   bindings — an existing BindingList to accumulate bindings into;
 *              on failure the list is left in the state it was
 *              before the call (bindings from a failed branch are
 *              rolled back automatically).
 *
 * Returns:
 *   1  if value ~ pattern  (match succeeds)
 *   0  otherwise
 *
 * On success, any wildcard bindings introduced by rule M3 have
 * been appended to *bindings.
 * On failure, *bindings is unchanged.
 * --------------------------------------------------------------- */
int roc_match(const Node *value, const Node *pattern, BindingList *bindings);

/* ---------------------------------------------------------------
 * node_equal — structural equality on two value Nodes
 * (used internally by the matcher; exported for test convenience)
 * --------------------------------------------------------------- */
int node_equal(const Node *a, const Node *b);

/* ---------------------------------------------------------------
 * Node constructors (also used by the parser)
 * --------------------------------------------------------------- */
Node *mk_name(const char *s);
Node *mk_agent(const char *s);
Node *mk_nil(void);
Node *mk_wc(const char *s);
Node *mk_bind(Node *v);
Node *mk_tuple_from_array(Node **children, int n, int all_values);
void  node_print(const Node *n, int depth);

#endif /* ROC_MATCH_H */
