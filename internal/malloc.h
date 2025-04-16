// Copyright canokeys.org
// SPDX-License-Identifier: Apache-2.0

#ifndef NSYNC_INTERNAL_MALLOC_H_
#define NSYNC_INTERNAL_MALLOC_H_

#include <stdlib.h>
#include <stdint.h>

#include "nsync_malloc.h"

static void *nsync_malloc(size_t size) {
    if (nsync_malloc_ptr_ != NULL) {
        return (*nsync_malloc_ptr_)(size);
    } else {
        return malloc(size);
    }
}

static void nsync_free(void *ptr) {
    if (nsync_free_ptr_ != NULL) {
        (*nsync_free_ptr_)(ptr);
    } else {
        free(ptr);
    }
}

static void *nsync_calloc(size_t nmemb, size_t size) {
    if (nsync_malloc_ptr_ != NULL) {
        return (*nsync_malloc_ptr_)(nmemb * size);
    } else {
        return calloc(nmemb, size);
    }
}

#endif /*NSYNC_INTERNAL_MALLOC_H_*/
