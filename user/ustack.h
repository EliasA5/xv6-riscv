#include "kernel/types.h"

#define MAXUALLOC    512 // maximum size of a user stack memory allocation

void *ustack_malloc(uint len);

int ustack_free(void);

