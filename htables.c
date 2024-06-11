// htables inspired from Python dictionnaries,
// see https://code.activestate.com/recipes/578375/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>

#include "htables.h"

#define EMPTY -1  // reserved index for empty slots
#define DUMMY -2  // reserved index for deleted slots

hash_t hash (char *s) {
  unsigned char *p = (unsigned char*) s;
  hash_t h = *p << 7;
  hash_t l = 0;
  while (*p) {
    h = (1000003 * h) ^ (*p);
    l++;
    p++;
  }
  h ^= l;
  if (h < 0) {
    return -h;
  } else {
    return h;
  }
}

void ht_make_index (htable_t *t) {
  // create the index table using t->isize
  t->index = malloc(t->isize * sizeof(hash_t));
  if (!t->index) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  for (int i=0; i<t->isize; i++) {
    t->index[i] = EMPTY;
  }
}

void ht_init (htable_t *t, valfree_t f) {
  t->used = 0;
  t->filled = 0;
  t->isize = 8;
  ht_make_index(t);
  t->ssize = 6;
  t->slots = malloc(t->ssize * sizeof(slot_t));
  if (!t->slots) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  t->vfree = f;
}

void ht_free (htable_t *t) {
  for (int i=0; i<t->used; i++) {
    free(t->slots[i].key);
    t->vfree(t->slots[i].val);
  }
  free(t->index);
  free(t->slots);
}

void ht_clear (htable_t *t) {
  ht_free(t);
  ht_init(t, t->vfree);
}

hsize_t ht_len (htable_t t) {
  return t.used;
}

void ht_visit (htable_t t, visitor_t visit) {
  for (hsize_t i=0; i<t.used; i++) {
    if (!visit(t.slots[i].key, t.slots[i].val)) {
      break;
    }
  }
}

typedef struct {  // pseudo-randomly traverse a htable
  hash_t mask;    // to truncate values
  hash_t index;   // probed index
  hash_t perturb; // to perturbate next .index
} hprobe_t;

hash_t ht_probe_first (htable_t t, hprobe_t *p, hash_t h) {
  assert(h >= 0);
  p->mask = t.isize - 1;
  p->index = h & p->mask;
  p->perturb = h;
  return p->index;
}

#define PERTURB_SHIFT 5

hash_t ht_probe_next (hprobe_t *p) {
  p->index = 5 * p->index + 1 + p->perturb;
  p->perturb >>= PERTURB_SHIFT;
  return p->index & p->mask;
}

typedef struct {
  hash_t pos; // position in htable.slots (or EMPTY, or DUMMY)
  hash_t idx; // position in htable.index
} lookup_t;

lookup_t ht_lookup (htable_t t, char *k, hash_t h) {
  hash_t freeidx = EMPTY;
  hprobe_t probe;
  lookup_t lookup;
  assert(t.filled < t.isize);
  for (lookup.idx = ht_probe_first(t, &probe, h);;
       lookup.idx = ht_probe_next(&probe)) {
    lookup.pos = t.index[lookup.idx];
    if (lookup.pos == EMPTY) {
      if (freeidx != EMPTY) {
        lookup.pos = DUMMY;
        lookup.idx = freeidx;
      }
      break;
    } else if (lookup.pos == DUMMY) {
      if (freeidx == EMPTY) {
        freeidx = lookup.idx;
      }
    } else if (t.slots[lookup.pos].hash == h
               && strcmp(t.slots[lookup.pos].key, k) == 0) {
      break;
    }
  }
  return lookup;
}

int ht_get (htable_t t, char *k, void **v) {
  hash_t h = hash(k);
  lookup_t lookup = ht_lookup(t, k, h);
  if (lookup.pos < 0) {
    return 0;
  } else {
    if (v) {
      *v = t.slots[lookup.pos].val;
    }
    return 1;
  }
}

int ht_del (htable_t *t, char *k, void **v) {
  hash_t h = hash(k);
  lookup_t lookup = ht_lookup(*t, k, h);
  lookup_t last_lookup;
  slot_t last_slot;
  if (lookup.pos < 0) {
    return 0;
  } else if (v) {
    *v = t->slots[lookup.pos].val;
  } else {
    t->vfree(t->slots[lookup.pos].val);
  }
  t->index[lookup.idx] = DUMMY;
  t->used--;
  free(t->slots[lookup.pos].key);
  if (lookup.pos != t->used) {
    last_slot = t->slots[t->used];
    last_lookup = ht_lookup(*t, last_slot.key, last_slot.hash);
    assert(last_lookup.pos > 0);
    assert(lookup.idx != last_lookup.idx);
    t->index[last_lookup.idx] = lookup.pos;
    t->slots[lookup.pos] = last_slot;
  }
  return 1;
}

void ht_resize_index (htable_t *t) {
  hsize_t pos, idx;
  hprobe_t probe;
  t->isize = 1 << (int)(floor(log2(4 * t->used)) + 1);
  free(t->index);
  ht_make_index(t);
  for (pos=0; pos<t->used; pos++) {
    for (idx = ht_probe_first(*t, &probe, t->slots[pos].hash);
         t->index[idx] != EMPTY;
         idx = ht_probe_next(&probe)) {
      // just probing until end condition is reached
    }
    t->index[idx] = pos;
  }
  t->filled = t->used;
}

void ht_resize_slots (htable_t *t) {
  hsize_t pos;
  slot_t *slots;
  t->ssize = 1 + t->isize * 2 / 3;
  slots = malloc(t->ssize * sizeof(slot_t));
  if (!slots) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  for (pos=0; pos<t->used; pos++) {
    slots[pos] = t->slots[pos];
  }
  free(t->slots);
  t->slots = slots;
}

void ht_resize (htable_t *t) {
  ht_resize_index(t);
  ht_resize_slots(t);
}

void ht_set (htable_t *t, char *k, void *v, void **d) {
  hash_t h = hash(k);
  lookup_t lookup = ht_lookup(*t, k, h);
  if (lookup.pos < 0) {
    assert(t->used < t->ssize);
    t->index[lookup.idx] = t->used;
    t->slots[t->used].hash = h;
    t->slots[t->used].key = strdup(k);
    t->slots[t->used].val = v;
    t->used++;
    if (lookup.pos == EMPTY) {
      t->filled++;
      if (t->filled * 3 > t->isize * 2) {
        ht_resize(t);
      }
    }
  } else {
    if (d) {
      *d = t->slots[lookup.pos].val;
    } else {
      t->vfree(t->slots[lookup.pos].val);
    }
    t->slots[lookup.pos].val = v;
  }
}
