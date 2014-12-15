#include <stdlib.h>

#include "casq.h"
#include "atomic.h"

typedef struct casq_link {
    void *data;
    struct casq_link *next;
} casq_link;

struct casq {
    casq_link *head;
    casq_link *accum;
    casq_link *free_links;
    int is_reversing;
};

struct casq *casq_new(void) {
    struct casq *q = calloc(1, sizeof(*q));
    return q;
}

static casq_link *get_link(struct casq *q) {
    for (;;) {
        if (q->free_links == NULL) {
            casq_link *nl = calloc(1, sizeof(*nl));
            return nl;

        } else {
            casq_link *n = q->free_links;
            if (ATOMIC_BOOL_COMPARE_AND_SWAP(&q->free_links, n, n->next)) {
                n->next = NULL;
                return n;
            }
        }
    }
}

bool casq_push(struct casq *q, void *data) {
    casq_link *l = get_link(q);
    if (l == NULL) {
        return false;
    }

    l->data = data;

    for (;;) {                  /* spin, push */
        casq_link *cur_head = q->accum;
        l->next = cur_head;
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&q->accum, cur_head, l)) {
            return true;
        }
    }
}

static void reverse(struct casq *q) {
    casq_link *to_reverse = NULL;
    for (;;) {                  /* spin, unlink */
        to_reverse = q->accum;
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&q->accum, to_reverse, NULL)) {
            break;
        }
    }
    
    casq_link *new_head = q->head;
    casq_link *next = NULL;

    for (casq_link *cur = to_reverse; cur; cur = next) {
        next = cur->next;
        cur->next = new_head;
        new_head = cur;
    }
    
    q->head = new_head;
}

bool casq_empty(struct casq *q) {
    bool res = q->head == NULL && q->accum == NULL;
    return res;
}

void *casq_pop(struct casq *q) {
    for (;;) {
        if (q->head != NULL) {
            casq_link *l = NULL;
            for (;;) {              /* spin, pop */
                l = q->head;
                if (ATOMIC_BOOL_COMPARE_AND_SWAP(&q->head, l, l->next)) {
                    break;
                }            
            }
            void *res = l->data;
            l->data = NULL;
            for (;;) {              /* spin, push */
                l->next = q->free_links;
                if (ATOMIC_BOOL_COMPARE_AND_SWAP(&q->free_links, q->free_links, l)) {
                    break;
                }
            }
            return res;
        } else if (q->accum != NULL) {
            /* If no more links are available, reverse the accumulated ones on demand. */
            if (ATOMIC_BOOL_COMPARE_AND_SWAP(&q->is_reversing, 0, 1)) { /* spin, lock */
                reverse(q);
                q->is_reversing = 0;
            } else {
                continue;
            }
        } else {                    /* empty */
            return NULL;
        }
    }
}

static void free_ll(casq_link *l, casq_free_cb *cb, void *udata) {
    casq_link *cur = NULL;
    casq_link *next = NULL;
    for (cur = l; cur; cur = next) {
        next = cur->next;
        if (cur->data) { cb(cur->data, udata); }
        free(cur);
    }
}

void casq_free(struct casq *q, casq_free_cb *cb, void *udata) {
    casq_link *head = NULL;
    casq_link *accum = NULL;
    casq_link *free_links = NULL;

    for (;;) {
        head = q->head;
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&q->head, head, NULL)) { break; }
    }

    for (;;) {
        accum = q->accum;
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&q->accum, accum, NULL)) { break; }
    }

    for (;;) {
        free_links = q->free_links;
        if (ATOMIC_BOOL_COMPARE_AND_SWAP(&q->free_links, free_links, NULL)) { break; }
    }

    free_ll(head, cb, udata);
    free_ll(accum, cb, udata);
    free_ll(free_links, cb, udata);

    free(q);
}

