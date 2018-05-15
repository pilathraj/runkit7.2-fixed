/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2006 The PHP Group, (c) 2008-2015 Dmitry Zenovich |
  | Patches (c) 2015-2018 Tyson Andre                                    |
  +----------------------------------------------------------------------+
  | This source file is subject to the new BSD license,                  |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.opensource.org/licenses/BSD-3-Clause                      |
  | If you did not receive a copy of the license and are unable to       |
  | obtain it through the world-wide-web, please send a note to          |
  | dzenovich@gmail.com so we can mail you a copy immediately.           |
  +----------------------------------------------------------------------+
  | Author: Sara Golemon <pollita@php.net>                               |
  | Modified by Dmitry Zenovich <dzenovich@gmail.com>                    |
  | Modified for php7 "runkit7" by Tyson Andre<tysonandre775@hotmail.com>|
  +----------------------------------------------------------------------+
*/

#ifndef PHP_RUNKIT_HASH_H
#define PHP_RUNKIT_HASH_H

#ifdef PHP_RUNKIT_MANIPULATION

#include "runkit.h"
#include "Zend/zend_types.h"
#include "Zend/zend_types.h"

/* {{{ php_runkit_hash_get_bucket */
// Copied from zend_hash_find_bucket of zend_hash.c and modified slightly.
// Same as the implementation in 7.0 and 7.1
inline static Bucket *php_runkit_zend_hash_find_bucket(HashTable *ht, zend_string* key)
{
	zend_ulong h;
	uint32_t nIndex;
	uint32_t idx;
	Bucket *p, *arData;

	h = zend_string_hash_val(key);
	arData = ht->arData;
	nIndex = h | ht->nTableMask;
	idx = HT_HASH_EX(arData, nIndex);
	while (EXPECTED(idx != HT_INVALID_IDX)) {
		p = HT_HASH_TO_BUCKET_EX(arData, idx);
		if (EXPECTED(p->key == key)) { /* check for the same interned string */
			return p;
		} else if (EXPECTED(p->h == h) &&
				EXPECTED(p->key) &&
				EXPECTED(ZSTR_LEN(p->key) == ZSTR_LEN(key)) &&
				EXPECTED(memcmp(ZSTR_VAL(p->key), ZSTR_VAL(key), ZSTR_LEN(key)) == 0)) {
			return p;
		}
		idx = Z_NEXT(p->val);
	}
	return NULL;
}
/* }}} */

/* {{{ php_runkit_hash_move_runkit_to_front }}} */
// Moves the runkit module to the front of the list (After "core", but before modules such as "session"
// so that it will be unloaded after all of the PHP code is executed.
inline static void php_runkit_hash_move_runkit_to_front() {
	zend_ulong numkey;
	zend_string *strkey;
	zend_string *runkit_str;
	dtor_func_t oldPDestructor;
	zend_module_entry *module;
	int num = 0;
	HashTable tmp;
	if (RUNKIT_G(module_moved_to_front)) {
		// Already moved it to the front (but after "core").
		return;
	}
	RUNKIT_G(module_moved_to_front) = 1;

	runkit_str = zend_string_init("runkit", sizeof("runkit") - 1, 0);
	// 1. If runkit is not part of the module registry, warn and do nothing.
	if (!zend_hash_exists(&module_registry, runkit_str)) {
		php_error_docref(NULL, E_WARNING, "Failed to find \"runkit\" module when attempting to change module unloading order - The lifetime of internal function overrides will be unexpected");
		zend_string_release(runkit_str);
		return;
	}
	// php_error_docref(NULL, E_WARNING, "In php_runkit_hash_move_to_front size=%d", (int)zend_hash_num_elements(&module_registry));

	// 2. Create a temporary table with "runkit" at the front.
	ZEND_HASH_FOREACH_KEY_PTR(&module_registry, numkey, strkey, module) {
		// php_error_docref(NULL, E_WARNING, "In php_runkit_hash_move_to_front numkey=%d strkey=%s refcount=%d zv=%llx", (int)numkey, strkey != NULL ? ZSTR_VAL(strkey) : "null", (int)(strkey != NULL ? zend_string_refcount(strkey) : 0), (long long) (uintptr_t)module);
		if (num++ == 0) {
			zend_bool first_is_core = 0;
			Bucket *b;
			zend_hash_init(&tmp, zend_hash_num_elements(&module_registry), NULL, NULL, 0);
			// add "core" first, then "runkit", then the remaining modules.
			// If core isn't first, core tries to free the memory of strings that runkit allocated.
			first_is_core = strkey != NULL && zend_string_equals_literal(strkey, "core");
			if (first_is_core) {
				zend_hash_add_ptr(&tmp, strkey, module);
			} else {
				php_error_docref(NULL, E_WARNING, "unexpected module order: \"core\" isn't first");
			}
			// Otherwise, initialize the temporary table (with no destructor function)
			b = php_runkit_zend_hash_find_bucket(&module_registry, runkit_str);

			zend_hash_add_ptr(&tmp, b->key, Z_PTR(b->val));
			if (first_is_core) {
				continue;  // Already added "core" to tmp map
			}
		}
		if (strkey != NULL) {
			if (zend_string_equals(runkit_str, strkey)) {
				// Already added persistent string for "runkit" to the front.
				continue;
			}
			zend_hash_add_ptr(&tmp, strkey, module);
		} else {
			zend_hash_index_add_ptr(&tmp, numkey, module);
		}
	} ZEND_HASH_FOREACH_END();
	zend_string_release(runkit_str);
	runkit_str = NULL;

	oldPDestructor = module_registry.pDestructor;
	module_registry.pDestructor = NULL;
	zend_hash_clean(&module_registry);
	module_registry.pDestructor = oldPDestructor;
	oldPDestructor = NULL;

	// 3. Copy the reordered table to the original module_registry
	ZEND_HASH_FOREACH_KEY_PTR(&tmp, numkey, strkey, module) {
		if (strkey != NULL) {
			zend_hash_add_ptr(&module_registry, strkey, module);
		} else {
			zend_hash_index_add_ptr(&module_registry, numkey, module);
		}
	} ZEND_HASH_FOREACH_END();

	tmp.pDestructor = NULL;
	zend_hash_destroy(&tmp);

	return;
}

#endif /* PHP_RUNKIT_MANIPULATION */

#endif
