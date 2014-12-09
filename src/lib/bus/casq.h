#ifndef CASQ_H
#define CASQ_H

#include <stdbool.h>

/* Atomic queue. */
struct casq;

struct casq *casq_new(void);

bool casq_push(struct casq *q, void *data);
void *casq_pop(struct casq *q);
bool casq_empty(struct casq *q);

typedef void (casq_free_cb)(void *data, void *udata);

void casq_free(struct casq *q, casq_free_cb *cb, void *udata);

#endif
