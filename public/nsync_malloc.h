// Copyright canokeys.org
// SPDX-License-Identifier: Apache-2.0

#ifndef NSYNC_PUBLIC_NSYNC_MALLOC_H_
#define NSYNC_PUBLIC_NSYNC_MALLOC_H_

#include <stdlib.h>
#include <stdint.h>

extern void *(*nsync_malloc_ptr_) (size_t size);
extern void (*nsync_free_ptr_) (void *ptr);

#endif /*NSYNC_PUBLIC_NSYNC_MALLOC_H_*/