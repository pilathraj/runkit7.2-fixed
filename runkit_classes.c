/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2006 The PHP Group, (c) 2008-2015 Dmitry Zenovich |
  | (c) 2015-2018 Tyson Andre                                            |
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
  +----------------------------------------------------------------------+
FIXME: Check that all applicable flags, member variables, etc. are manipulated appropriately for classes, etc.
runkit_property_modify() may be implemented in the future.
*/

/* $Id$ */

#include "runkit.h"
#include "php_runkit_zval.h"

#ifdef PHP_RUNKIT_MANIPULATION
#ifdef PHP_RUNKIT_MANIPULATION_CLASSES
/* {{{ php_runkit_remove_inherited_methods_foreach */
// Remove methods that were inherited from class ce from the function_table.
static int php_runkit_remove_inherited_methods(zval *pDest, void *argument); // forward declare.
static void php_runkit_remove_inherited_methods_foreach(HashTable *function_table, zend_class_entry *ce) {
	zend_hash_apply_with_argument(function_table, php_runkit_remove_inherited_methods, ce);
}

/* }}} */

/* {{{ php_runkit_remove_inherited_methods */
static int php_runkit_remove_inherited_methods(zval *pDest, void *argument)
{
	zend_function *fe = Z_FUNC_P(pDest);
	zend_class_entry *ce = (zend_class_entry*) argument;
	zend_string * const fname = fe->common.function_name;
	zend_string *fname_lower;
	zend_class_entry *ancestor_class;
	ZEND_ASSERT(Z_TYPE_P(pDest) == IS_PTR);

	fname_lower = zend_string_tolower(fname);

	ancestor_class = php_runkit_locate_scope(ce, fe, fname_lower);

	if (ancestor_class == ce) {
		zend_string_release(fname_lower);
		return ZEND_HASH_APPLY_KEEP;
	}

	php_runkit_clean_children_methods_foreach(EG(class_table), ancestor_class, ce, fname_lower, fe);
	PHP_RUNKIT_DEL_MAGIC_METHOD(ce, fe);
	php_runkit_remove_function_from_reflection_objects(fe);

	zend_string_release(fname_lower);
	return ZEND_HASH_APPLY_REMOVE;
}
/* }}} */

/* {{{ php_runkit_memrchr */
static inline const void *php_runkit_memrchr(const void *s, int c, size_t n)
{
	register const unsigned char *e;

	if (n <= 0) {
		return NULL;
	}

	for (e = (const unsigned char *)s + n - 1; e >= (const unsigned char *)s; e--) {
		if (*e == (const unsigned char)c) {
			return (const void *)e;
		}
	}

	return NULL;
}
/* }}} */

/* {{{ proto bool runkit_class_emancipate(string classname)
	Convert an inherited class to a base class, removes any method whose scope is ancestral */
PHP_FUNCTION(runkit_class_emancipate)
{
	zend_class_entry *ce;
	zend_string *classname;
	HashPosition pos;
	zend_string *key;
	zend_property_info *property_info_ptr = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &classname) == FAILURE) {
		RETURN_FALSE;
	}

	if ((ce = php_runkit_fetch_class(classname)) == NULL) {
		RETURN_FALSE;
	}

	if (!ce->parent) {
		php_error_docref(NULL, E_NOTICE, "Class %s has no parent to emancipate from", ZSTR_VAL(classname));
		RETURN_TRUE;
	}

	php_runkit_clear_all_functions_runtime_cache();

	php_runkit_remove_inherited_methods_foreach(&ce->function_table, ce);

	ZEND_HASH_FOREACH_STR_KEY_PTR(&ce->parent->properties_info, key, property_info_ptr) {
		if (key != NULL) {
			const char *propname = ZSTR_VAL(property_info_ptr->name);
			int propname_len = ZSTR_LEN(property_info_ptr->name);
			const char *last_null;
			// TODO: Abstract this out.
			zend_string *propname_zs;

			last_null = php_runkit_memrchr(propname, 0, propname_len);
			if (last_null) {
			    propname_len -= last_null - propname + 1;
			    propname = last_null+1;
			}
			propname_zs = zend_string_init(propname, propname_len, 0);

			php_runkit_def_prop_remove_int(ce, propname_zs,
			                            ce->parent, (property_info_ptr->flags & ZEND_ACC_STATIC) != 0,
			                            1 /* remove_from_objects */, property_info_ptr);
			// TODO: zend_string_release?
		}
		zend_hash_move_forward_ex(&ce->parent->properties_info, &pos);
	} ZEND_HASH_FOREACH_END();
	ce->parent = NULL;

	RETURN_TRUE;
}
/* }}} */

/* {{{ php_runkit_inherit_methods_foreach
    Inherit methods from a new ancestor (in function_table) */
static int php_runkit_inherit_methods(zend_function *fe, zend_class_entry *ce);
static void php_runkit_inherit_methods_foreach(HashTable *function_table, zend_class_entry *ce) {
	zend_function *fe;
	ZEND_HASH_FOREACH_PTR(function_table, fe) {
		php_runkit_inherit_methods(fe, ce);
	} ZEND_HASH_FOREACH_END();
}
/* }}} */

/* {{{ php_runkit_inherit_methods
	Inherit methods from a new ancestor */
static int php_runkit_inherit_methods(zend_function *fe, zend_class_entry *ce)
{
	zend_string *fname = fe->common.function_name;
	zend_string *fname_lower;
	zend_class_entry *ancestor_class;

	/* method name keys must be lower case */
	fname_lower = zend_string_tolower(fname);

	if (zend_hash_exists(&ce->function_table, fname_lower)) {
		zend_string_release(fname_lower);
		return ZEND_HASH_APPLY_KEEP;
	}

	ancestor_class = php_runkit_locate_scope(ce, fe, fname_lower);

	if (runkit_zend_hash_add_or_update_ptr(&ce->function_table, fname_lower, fe, HASH_ADD) == NULL) {
		php_error_docref(NULL, E_WARNING, "Error inheriting parent method: %s()", ZSTR_VAL(fe->common.function_name));
		zend_string_release(fname_lower);
		return ZEND_HASH_APPLY_KEEP;
	}

	if ((fe = zend_hash_find_ptr(&ce->function_table, fname_lower)) == NULL) {
		php_error_docref(NULL, E_WARNING, "Unable to locate newly added method");
		zend_string_release(fname_lower);
		return ZEND_HASH_APPLY_KEEP;
	}

	PHP_RUNKIT_FUNCTION_ADD_REF(fe);
	PHP_RUNKIT_ADD_MAGIC_METHOD(ce, fname_lower, fe, NULL);

	php_runkit_update_children_methods_foreach(EG(class_table), ancestor_class, ce, fe, fname_lower, NULL);
	zend_string_release(fname_lower);

	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */

/* {{{ php_runkit_class_copy
       Copy class into class table */
int php_runkit_class_copy(zend_class_entry *src, zend_string *classname)
{
	zend_class_entry *new_class_entry, *parent = NULL;
	zend_string *classname_lower;

	classname_lower = zend_string_to_interned(zend_string_tolower(classname));

	new_class_entry = pemalloc(sizeof(zend_class_entry), 1);
	if (src->parent && src->parent->name) {
		parent = php_runkit_fetch_class_int(src->parent->name);
	}
	new_class_entry->type = ZEND_USER_CLASS;
	new_class_entry->name = classname;

	zend_initialize_class_data(new_class_entry, 1);
	new_class_entry->parent = parent;
	new_class_entry->info.user.filename = src->info.user.filename;
	new_class_entry->info.user.line_start = src->info.user.line_start;
	new_class_entry->info.user.doc_comment = src->info.user.doc_comment;
	new_class_entry->info.user.line_end = src->info.user.line_end;
	new_class_entry->num_traits = src->num_traits;
	new_class_entry->traits = src->traits;
	new_class_entry->ce_flags = src->ce_flags;

	// TODO: zend_hash_update_ptr for everything to do with class_table
	zend_hash_update_ptr(EG(class_table), classname_lower, new_class_entry);

	new_class_entry->num_interfaces = src->num_interfaces;
	// zend_string_release(classname_lower);  // this was interned

	if (new_class_entry->parent) {
		php_runkit_inherit_methods_foreach(&(new_class_entry->parent->function_table), new_class_entry);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ proto bool runkit_class_adopt(string classname, string parentname)
	Convert a base class to an inherited class, add ancestral methods when appropriate */
PHP_FUNCTION(runkit_class_adopt)
{
	zend_class_entry *ce, *parent;
	zend_string *classname, *parentname;
	zend_string *key;
	zend_property_info *property_info_ptr = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &classname, &parentname) == FAILURE) {
		RETURN_FALSE;
	}

	if ((ce = php_runkit_fetch_class(classname)) == NULL) {
		RETURN_FALSE;
	}

	if (ce->parent) {
		php_error_docref(NULL, E_WARNING, "Class %s already has a parent", ZSTR_VAL(classname));
		RETURN_FALSE;
	}

	if ((parent = php_runkit_fetch_class(parentname)) == NULL) {
		RETURN_FALSE;
	}

	ce->parent = parent;

	ZEND_HASH_FOREACH_STR_KEY_PTR(&parent->properties_info, key, property_info_ptr) {
		if (key != NULL) {
			zval *p;
			const char *propname = ZSTR_VAL(property_info_ptr->name);
			int propname_len = ZSTR_LEN(property_info_ptr->name);
			const char *last_null;
			zend_string* propname_zs;

			if (property_info_ptr->flags & ZEND_ACC_STATIC) {
				p = &CE_STATIC_MEMBERS(parent)[property_info_ptr->offset];
			} else {
				p = &parent->default_properties_table[property_info_ptr->offset];
			}

			php_runkit_zval_resolve_class_constant(p, parent);

			last_null = php_runkit_memrchr(propname, 0, propname_len);
			if (last_null) {
				propname_len -= last_null - propname + 1;
				propname = last_null+1;
			}
			propname_zs = zend_string_init(propname, propname_len, 0);

			php_runkit_def_prop_add_int(ce, propname_zs,
										p, property_info_ptr->flags/*visibility*/,
										property_info_ptr->doc_comment,
										property_info_ptr->ce /*definer_class*/, 0 /*override*/, 1 /*override_in_objects*/);
			// TODO: free propname_zs?
		}
	} ZEND_HASH_FOREACH_END();

	php_runkit_clear_all_functions_runtime_cache();

	php_runkit_inherit_methods_foreach(&parent->function_table, ce);

	RETURN_TRUE;
}
/* }}} */

#endif /* PHP_RUNKIT_MANIPULATION_CLASSES */
#endif /* PHP_RUNKIT_MANIPULATION */
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

