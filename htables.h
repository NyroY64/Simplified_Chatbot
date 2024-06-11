typedef long long hash_t;
typedef unsigned long long hsize_t;
typedef void (*valfree_t) (void*);
typedef int (*visitor_t)(char*, void*);

typedef struct {
  hash_t hash;
  char   *key;
  void   *val;
} slot_t;

typedef struct {
  hsize_t used;
  hsize_t filled;
  hash_t  *index;
  hsize_t isize;
  slot_t  *slots;
  hsize_t ssize;
  valfree_t vfree;
} htable_t;

void ht_init (htable_t *t, valfree_t f);
void ht_free (htable_t *t);
void ht_clear (htable_t *t);
hsize_t ht_len (htable_t t);
void ht_visit (htable_t t, visitor_t visit);
int ht_get (htable_t t, char *k, void **v);
int ht_del (htable_t *t, char *k, void **v);
void ht_set (htable_t *t, char *k, void *v, void **d);
