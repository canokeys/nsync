/* Copyright 2016 Google Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License. */

#include "nsync_cpp.h"
#include "platform.h"
#include "compiler.h"
#include "cputype.h"
#include "nsync.h"
#include "atomic.h"
#include "dll.h"
#include "sem.h"
#include "wait_internal.h"
#include "common.h"

NSYNC_CPP_START_

/* Internal details of nsync_counter. */
struct nsync_counter_s_ {
        nsync_atomic_uint32_ waited;    /* wait has been called */
        nsync_mu counter_mu;         /* protects fields below except reads of "value" */
        nsync_atomic_uint32_ value;     /* value of counter */
        struct nsync_dll_element_s_ *waiters;  /* list of waiters */
};

nsync_counter nsync_counter_new (uint32_t value) {
	nsync_counter c = (nsync_counter) nsync_malloc (sizeof (*c));
	if (c != NULL) {
		memset ((void *) c, 0, sizeof (*c));
		ATM_STORE (&c->value, value);
	}
	return (c);
}

void nsync_counter_free (nsync_counter c) {
	nsync_mu_lock (&c->counter_mu);
	ASSERT (nsync_dll_is_empty_ (c->waiters));
	nsync_mu_unlock (&c->counter_mu);
	nsync_free (c);
}

uint32_t nsync_counter_add (nsync_counter c, int32_t delta) {
	uint32_t value;
	IGNORE_RACES_START ();
	if (delta == 0) {
		value = ATM_LOAD_ACQ (&c->value);
	} else {
		nsync_mu_lock (&c->counter_mu);
		do {
			value = ATM_LOAD (&c->value);
		} while (!ATM_CAS_RELACQ (&c->value, value, value+delta));
		value += delta;
		if (delta > 0) {
			/* It's illegal to increase the count from zero if
			   there has been a waiter. */
			ASSERT (value != (uint32_t) delta || !ATM_LOAD (&c->waited));
			ASSERT (value > value - delta); /* Crash on overflow. */
		} else {
			ASSERT (value < value - delta); /* Crash on overflow. */
		}
		if (value == 0) {
			nsync_dll_element_ *p;
			while ((p = nsync_dll_first_ (c->waiters)) != NULL) {
				struct nsync_waiter_s *nw = DLL_NSYNC_WAITER (p);
				c->waiters = nsync_dll_remove_ (c->waiters, p);
				ATM_STORE_REL (&nw->waiting, 0);
				nsync_mu_semaphore_v (nw->sem);
			}
		}
		nsync_mu_unlock (&c->counter_mu);
	}
	IGNORE_RACES_END ();
	return (value);
}

uint32_t nsync_counter_value (nsync_counter c) {
	uint32_t result;
	IGNORE_RACES_START ();
	result = ATM_LOAD_ACQ (&c->value);
	IGNORE_RACES_END ();
	return (result);
}

uint32_t nsync_counter_wait (nsync_counter c, nsync_time abs_deadline) {
	struct nsync_waitable_s waitable;
	struct nsync_waitable_s *pwaitable = &waitable;
	uint32_t result = 0;
	waitable.v = c;
	waitable.funcs = &nsync_counter_waitable_funcs;
	if (nsync_wait_n (NULL, NULL, NULL, abs_deadline, 1, &pwaitable) != 0) {
		IGNORE_RACES_START ();
		result = ATM_LOAD_ACQ (&c->value);
		IGNORE_RACES_END ();
	}
	return (result);
}

static nsync_time counter_ready_time (void *v, struct nsync_waiter_s *nw UNUSED) {
	nsync_counter c = (nsync_counter) v;
	nsync_time r;
	ATM_STORE (&c->waited, 1);
	r = (ATM_LOAD_ACQ (&c->value) == 0? nsync_time_zero : nsync_time_no_deadline);
	return (r);
}

static int counter_enqueue (void *v, struct nsync_waiter_s *nw) {
	nsync_counter c = (nsync_counter) v;
	int32_t value;
	nsync_mu_lock (&c->counter_mu);
	value = ATM_LOAD_ACQ (&c->value);
	if (value != 0) {
		c->waiters = nsync_dll_make_last_in_list_ (c->waiters, &nw->q);
		ATM_STORE (&nw->waiting, 1);
	} else {
		ATM_STORE (&nw->waiting, 0);
	}
	nsync_mu_unlock (&c->counter_mu);
	return (value != 0);
}

static int counter_dequeue (void *v, struct nsync_waiter_s *nw) {
	nsync_counter c = (nsync_counter) v;
	int32_t value;
	nsync_mu_lock (&c->counter_mu);
	value = ATM_LOAD_ACQ (&c->value);
	if (ATM_LOAD_ACQ (&nw->waiting) != 0) {
		c->waiters = nsync_dll_remove_ (c->waiters, &nw->q);
		ATM_STORE (&nw->waiting, 0);
	}
	nsync_mu_unlock (&c->counter_mu);
	return (value != 0);
}

const struct nsync_waitable_funcs_s nsync_counter_waitable_funcs = {
	&counter_ready_time,
	&counter_enqueue,
	&counter_dequeue
};

NSYNC_CPP_END_
