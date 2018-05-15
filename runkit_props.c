/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2006 The PHP Group, (c) 2008-2015 Dmitry Zenovich |
  | "runkit7" patches (c) 2015-2018 Tyson Andre                          |
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
  | Modifications for php7 (in progress) by Tyson Andre                  |
  +----------------------------------------------------------------------+
Note: This file is a stub, some functionality hasn't been ported yet.
(Will be used for runkit_import in the future)

runkit_property_modify() may be added in the future (will change the value, but won't change the set of properties)
*/

/* $Id$ */

#include "runkit.h"
#include "php_runkit_zval.h"

#ifdef PHP_RUNKIT_MANIPULATION_PROPERTIES

/* {{{ php_runkit_make_object_property_public */
static inline void php_runkit_make_object_property_public(zend_string *propname, zend_object *object, int offset, zend_property_info *property_info_ptr) {
	if ((property_info_ptr->flags & (ZEND_ACC_PRIVATE | ZEND_ACC_PROTECTED | ZEND_ACC_SHADOW))) {
		zval *prop_val = NULL;
		if (!object->properties) {
			prop_val = &object->properties_table[offset];
			rebuild_object_properties(object);
		} else if (!Z_ISUNDEF(object->properties_table[offset])) {
			// TODO: Is Z_ISUNDEF the correct thing to do?
			prop_val = &object->properties_table[offset];
		}
		if (prop_val) {
			// TODO: should these references be shared?
			Z_TRY_ADDREF_P(prop_val);
			// Could probably also be ZSTR_H
			if (ZSTR_HASH(propname) != ZSTR_HASH(property_info_ptr->name)) {
				zend_hash_del(object->properties, property_info_ptr->name);
			}
			// TODO: This is probably going to cause a segfault.
			zend_hash_update(object->properties, propname, prop_val);
			php_error_docref(NULL, E_NOTICE, "Making %s::%s public to remove it "
					 "from class without objects overriding", ZSTR_VAL(object->ce->name), ZSTR_VAL(propname));
		}
	}
}
/* }}} */

/* {{{ php_runkit_remove_children_def_props
	Scan the class_table for children of the class just removed */
static void php_runkit_remove_children_def_props(zend_class_entry *ce, zend_class_entry *parent_class, zend_string *pname,
	zend_class_entry *class_we_originally_removing_from, zend_bool was_static, zend_bool remove_from_objects, zend_property_info *parent_property)
{
	if (ce->parent != parent_class) {
		/* Not a child, ignore */
		return;
	}

	php_runkit_def_prop_remove_int(ce, pname, class_we_originally_removing_from, was_static, remove_from_objects,
	                               parent_property);
}
/* }}} */

/* {{{ php_runkit_remove_property_by_full_name */
static int php_runkit_remove_property_by_full_name(zval *pDest, void *argument) {
	const zend_property_info *prop = Z_PTR_P(pDest);
	const zend_property_info *comp_prop = (zend_property_info*) argument;

	ZEND_ASSERT(Z_TYPE_P(pDest) == IS_PTR);

	if (ZSTR_H(prop->name) == ZSTR_H(comp_prop->name) && zend_string_equals(comp_prop->name, prop->name)) {
		return ZEND_HASH_APPLY_REMOVE;
	}

	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */

/* {{{ php_runkit_remove_overlapped_property_from_child
       Clean private properties of children by offset */
static void php_runkit_remove_overlapped_property_from_childs(zend_class_entry *ce, zend_class_entry *parent_class, zend_string *propname, int offset, zend_bool is_static, zend_bool remove_from_objects, zend_property_info *property_info_ptr);
static void php_runkit_remove_overlapped_property_from_childs_foreach(HashTable* ht, zend_class_entry *parent_class, zend_string *propname, int offset, zend_bool is_static, zend_bool remove_from_objects, zend_property_info *property_info_ptr) {
	zend_class_entry *ce;
	ZEND_HASH_FOREACH_PTR(ht, ce) {
		php_runkit_remove_overlapped_property_from_childs(ce, parent_class, propname, offset, is_static, remove_from_objects, property_info_ptr);
	} ZEND_HASH_FOREACH_END();
	// FIXME what was the intent of the original code?
	// php_runkit_def_prop_add_int(ce, propname, property_info_ptr, access_type, (zend_string*) NULL, definer_class, override, override_in_objects);
}
/* }}} */

/* {{{ php_runkit_remove_overlapped_property_from_child
       Clean private properties of child by offset */
static void php_runkit_remove_overlapped_property_from_childs(zend_class_entry *ce, zend_class_entry *parent_class, zend_string *propname, int offset, zend_bool is_static, zend_bool remove_from_objects, zend_property_info *property_info_ptr) {
	uint32_t i;
	zend_property_info *p;
	zval *table;

	if (ce->parent != parent_class) {
		/* Not a child, ignore */
		return;
	}

	php_runkit_remove_overlapped_property_from_childs_foreach(EG(class_table), ce, propname, offset, is_static, remove_from_objects, property_info_ptr);
	// php_runkit_remove_property_from_reflection_objects(ce, propname);

	if (is_static) {
		goto st_success;
	}

	PHP_RUNKIT_ITERATE_THROUGH_OBJECTS_STORE_BEGIN(i)
		if (object->ce == ce) {
			if (!remove_from_objects) {
				php_runkit_make_object_property_public(propname, object, offset, property_info_ptr);
			} else {
				if (!Z_ISUNDEF(object->properties_table[offset])) {
					if (!object->properties) {
						zval_ptr_dtor(&object->properties_table[offset]);
						ZVAL_UNDEF(&object->properties_table[offset]);
					} else {
						zend_hash_del(object->properties, propname);
					}
				}
			}
		}
	PHP_RUNKIT_ITERATE_THROUGH_OBJECTS_STORE_END

st_success:
	table = is_static ? ce->default_static_members_table : ce->default_properties_table;
	if (!Z_ISUNDEF(table[offset])) {
		zval_ptr_dtor(&table[offset]);
		ZVAL_UNDEF(&table[offset]);
		// FIXME reintroduce if property manipulation (adding/removing from instantiated classes) is reintroduced.
		// php_runkit_default_class_members_list_add(&RUNKIT_G(removed_default_class_members), ce, is_static, offset);
	}
	zend_hash_apply_with_argument(&ce->properties_info,
								  php_runkit_remove_property_by_full_name,
	                              property_info_ptr);
	if ((p = zend_hash_find_ptr(&ce->properties_info, propname)) != NULL &&
	    ZSTR_H(p->name) == ZSTR_H(property_info_ptr->name)) {
		zend_hash_del(&ce->properties_info, propname);
	}
}
/* }}} */

/* {{{ php_runkit_def_prop_add_int */
int php_runkit_def_prop_add_int(zend_class_entry *ce, zend_string* propname, zval *copyval, long visibility,
                                zend_string* doc_comment, zend_class_entry *definer_class, int override,
                                int override_in_objects)
{
	uint32_t i;
	int offset;
	zend_property_info *prop_info_ptr = NULL;
	zval *pcopyval = copyval;

	if ((visibility & ZEND_ACC_PRIVATE) && (visibility & ZEND_ACC_STATIC) && definer_class && definer_class != ce) {
		return SUCCESS;
	}

	Z_TRY_ADDREF_P(pcopyval);

	// FIXME: Figure out what to do if references must be used.
	/*
	if (visibility & ZEND_ACC_STATIC) {
		if (definer_class == NULL || ce == definer_class) {
			Z_TRY_DELREF_P(pcopyval);
			SEPARATE_ARG_IF_REF(pcopyval);
		} else {
			// TODO: Equivalent of below
			Z_SET_ISREF_P(pcopyval);
		}
	}
	*/

	// TODO: Extract from zval instead.
	if ((prop_info_ptr = zend_hash_find_ptr(&ce->properties_info, propname)) != NULL) {
		if (!override) {
			zval_ptr_dtor(pcopyval);
			php_error_docref(NULL, E_NOTICE, "%s%s%s already exists, not adding",
			                 ZSTR_VAL(ce->name), (prop_info_ptr->flags & ZEND_ACC_STATIC) ? "::$" : "->", ZSTR_VAL(propname));
			return FAILURE;
		} else {
			php_runkit_def_prop_remove_int(ce, propname, NULL, (zend_bool) 0, override_in_objects, (zend_property_info*) NULL);
			php_runkit_clear_all_functions_runtime_cache();
		}
	}
	prop_info_ptr = NULL;
	zend_string_addref(propname);

	if (zend_declare_property_ex(ce, propname, pcopyval, visibility, doc_comment) == FAILURE) {
		zval_ptr_dtor(pcopyval);
		php_error_docref(NULL, E_WARNING, "Cannot declare new property");
		return FAILURE;
	}

	if (ce != definer_class) {
		if (zend_hash_find(&ce->properties_info, propname) == NULL) {
			zval_ptr_dtor(pcopyval);
			php_error_docref(NULL, E_WARNING, "Cannot find just added property's info");
			return FAILURE;
		}
		if (visibility & ZEND_ACC_PRIVATE) {
			zend_string *newkey;
			// zend_string *oldkey;
			zend_string *interned_name;
			zend_property_info new_prop_info;
			// TODO: Types?
			newkey = zend_mangle_property_name(ZSTR_VAL(definer_class->name), ZSTR_LEN(definer_class->name), ZSTR_VAL(propname), ZSTR_LEN(propname), ce->type & ZEND_INTERNAL_CLASS);
			// old_key = zend_mangle_property_name(ce->name, ce->name_length, propname, propname_len, ce->type & ZEND_INTERNAL_CLASS);
			memcpy(&new_prop_info, prop_info_ptr, sizeof(zend_property_info));
			new_prop_info.name = newkey;
			new_prop_info.ce = definer_class;
			// TODO: revisit reference counting.
			interned_name = zend_new_interned_string(new_prop_info.name);
			if (interned_name != new_prop_info.name) {
				zend_string_release(new_prop_info.name);
				new_prop_info.name = interned_name;
			}

			// TODO: is this needed?
			ZSTR_HASH(new_prop_info.name);
			if (new_prop_info.doc_comment) {
				// TODO: clone instead?
				// zend_string_addref(new_prop_info.doc_comment);
			}
			// TODO: update instead?
			zend_hash_del(&ce->properties_info, propname);
			prop_info_ptr = zend_hash_add_mem(&ce->properties_info, propname, &new_prop_info, sizeof(zend_property_info));
		}
		prop_info_ptr->ce = definer_class;
	}
	/*
	// FIXME reintroduce this
	{
		zend_class_entry *ce_it;
		ZEND_HASH_FOREACH_PTR(EG(class_table), ce_it) {
			php_runkit_update_children_def_props(ce_it, ce, copyval, propname, visibility, definer_class, override, override_in_objects);
		} ZEND_HASH_FOREACH_END();
	}
	*/

	if (!prop_info_ptr && (prop_info_ptr = zend_hash_find_ptr(&ce->properties_info, propname)) == NULL) {
		zval_ptr_dtor(pcopyval);
		php_error_docref(NULL, E_WARNING, "Cannot find just added property's info");
		return FAILURE;
	}
	offset = prop_info_ptr->offset;

	if ((visibility & ZEND_ACC_STATIC)) {
		return SUCCESS;
	}

	PHP_RUNKIT_ITERATE_THROUGH_OBJECTS_STORE_BEGIN(i)
		if (object->ce == ce) {
			int new_size = offset + 1;
			if (new_size >= 1) {
				// Reallocate the object's properties_table so that there are exactly new_size properties
				// (includes an additional terminating undefined property)
				// TODO: This seems like it depends on ZEND_ACC_USE_GUARDS, from looking at the function zend_object_properties_size
				object = perealloc(object, sizeof(zend_object) + sizeof(zval) * (new_size - 1), 0);
				ZVAL_UNDEF(&object->properties_table[new_size - 1]);
			}
			if (!Z_ISUNDEF(ce->default_properties_table[offset])) {
				if (!object->properties) {
					if (override_in_objects) {
						Z_ADDREF(ce->default_properties_table[offset]);
						object->properties_table[offset] = ce->default_properties_table[offset];
					} else {
						if (!Z_ISUNDEF(object->properties_table[offset])) {
							zval_ptr_dtor(&object->properties_table[offset]);
							ZVAL_UNDEF(&object->properties_table[offset]);
						}
					}
				} else {
					// TODO: Use a faster version of zend_hash_find for use in inner loops? Check if the hashes match first?
					// TODO: This seems redundant.
					zval *prop_val;
					if ((prop_val = zend_hash_find(object->properties, prop_info_ptr->name)) == NULL &&
					    (prop_val = zend_hash_find(object->properties, propname)) == NULL) {
						if (override_in_objects) {
							if (!Z_ISUNDEF(object->properties_table[offset])) {
								zval_ptr_dtor(&object->properties_table[offset]);
								ZVAL_UNDEF(&object->properties_table[offset]);
							}
							object->properties_table[offset] = ce->default_properties_table[offset];
							// TODO: Look at this line again
							Z_TRY_ADDREF(object->properties_table[offset]);
							zend_hash_update(object->properties, prop_info_ptr->name, &object->properties_table[offset]);
							/**
							 * TODO: figure out how to replace this?
							if (prop_info_ptr->h != h) {
								zend_hash_quick_del(object->properties, propname, propname_len+1, h);
							}
							*/
						}
					} else {
						if (!override_in_objects) {
							// TODO: Is this correct way to copy?
							object->properties_table[offset] = *prop_val;
							Z_ADDREF_P(&object->properties_table[offset]);
							zend_hash_update(object->properties, prop_info_ptr->name,
									       &object->properties_table[offset]);
							/*
							 * TODO: what does this do?
							 * Find a replacement
							if (prop_info_ptr->h != h) {
								zend_hash_quick_del(object->properties, propname, propname_len+1, h);
							}
							*/
						} else {
							object->properties_table[offset] = ce->default_properties_table[offset];
							Z_ADDREF_P(&object->properties_table[offset]);
							zend_hash_update(object->properties, prop_info_ptr->name,
									       &object->properties_table[offset]);
						}
					}
				}
			}
		}
	PHP_RUNKIT_ITERATE_THROUGH_OBJECTS_STORE_END

	return SUCCESS;
}
/* }}} */

/* {{{ php_runkit_def_prop_add
 */
static int php_runkit_def_prop_add(zend_string *classname, zend_string *propname, zval *value,
                                   long visibility, int override_in_objects)
{
	zend_class_entry *ce;
	zval *copyval;
	zend_property_info *existing_prop;

	if ((ce = php_runkit_fetch_class_int(classname)) == NULL) {
		return FAILURE;
	}

	if (ce->type & ZEND_INTERNAL_CLASS) {
		php_error_docref(NULL, E_WARNING, "Adding properties to internal classes is not allowed");
		return FAILURE;
	} else {
		copyval = value;
	}

	/* Check for existing property by this name */
	/* Existing public? */
	if ((existing_prop = zend_hash_find_ptr(&ce->properties_info, propname)) != NULL) {
		php_error_docref(NULL, E_WARNING, "%s%s%s already exists", ZSTR_VAL(classname),
		                 (existing_prop->flags & ZEND_ACC_STATIC) ? "::$" : "->", ZSTR_VAL(propname));
		return FAILURE;
	}

	php_runkit_zval_resolve_class_constant(copyval, ce);

	if (php_runkit_def_prop_add_int(ce, propname, copyval, visibility, NULL, ce, 0,
	                                override_in_objects) != SUCCESS) {
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ php_runkit_dump_string */
/*
static inline void php_runkit_dump_string(const char *str, int len) {
	int j;
	for (j=0; j<len; j++) {
		printf("%c", str[j]);
	}
}
*/
/* }}} */

/* {{{ php_runkit_dump_hashtable_keys */
/*
static inline void php_runkit_dump_hashtable_keys(HashTable* ht) {
	HashPosition pos;
	void *ptr;
	for(zend_hash_internal_pointer_end_ex(ht, &pos);
	    zend_hash_get_current_data_ex(ht, (void*)&ptr, &pos) == SUCCESS;
	    zend_hash_move_backwards_ex(ht, &pos)) {
		printf("key = ");
		php_runkit_dump_string(pos->arKey, pos->nKeyLength);
	}
}
*/
/* }}} */

/* {{{ php_runkit_def_prop_remove_int */
int php_runkit_def_prop_remove_int(zend_class_entry *ce, zend_string *propname, zend_class_entry *class_we_originally_removing_from,
                                   zend_bool was_static, zend_bool remove_from_objects,
                                   zend_property_info *parent_property)
{
	// FIXME this should never be called until it is fixed.
	uint32_t i;
	int offset;
	int flags;
	zend_property_info *property_info_ptr;
	php_error_docref(NULL, E_ERROR, "php_runkit_def_prop_remove_int should not be called");

	if ((property_info_ptr = zend_hash_find_ptr(&ce->properties_info, propname)) != NULL) {
		if (class_we_originally_removing_from == NULL) {
			class_we_originally_removing_from = property_info_ptr->ce;
		}
		if (parent_property &&
		    (parent_property->offset != property_info_ptr->offset ||
		     parent_property->ce != property_info_ptr->ce ||
		     ((parent_property->flags & ZEND_ACC_STATIC) != (property_info_ptr->flags & ZEND_ACC_STATIC))
		    )) {
			return SUCCESS;
		}

		if (class_we_originally_removing_from != property_info_ptr->ce) {
			return SUCCESS;
		}

		if (property_info_ptr->flags & ZEND_ACC_STATIC) {
			was_static = 1;
			// TODO: is this reasonable?
			if (!Z_ISUNDEF(ce->default_static_members_table[property_info_ptr->offset])) {
				// TODO: Free this property?
				zval_ptr_dtor(&ce->default_static_members_table[property_info_ptr->offset]);
				// TODO: is this reasonable?
				ZVAL_UNDEF(&ce->default_static_members_table[property_info_ptr->offset]);
				// FIXME reintroduce
				//php_runkit_default_class_members_list_add(&RUNKIT_G(removed_default_class_members), ce, 1, property_info_ptr->offset);
			}
		} else {
			was_static = 0;
		}

		flags = property_info_ptr->flags;
		offset = property_info_ptr->offset;

		if (property_info_ptr->flags & (ZEND_ACC_PRIVATE|ZEND_ACC_SHADOW)) {
			if (offset >= 0) {
				php_runkit_remove_overlapped_property_from_childs_foreach(EG(class_table),
				                               ce, propname, offset,
				                               property_info_ptr->flags & ZEND_ACC_STATIC, remove_from_objects, property_info_ptr);
			}
		}
		{
			zend_class_entry *ce_iter;
			ZEND_HASH_FOREACH_PTR(EG(class_table), ce_iter) {

				php_runkit_remove_children_def_props(ce_iter, ce, propname, class_we_originally_removing_from,
		                               was_static, remove_from_objects, property_info_ptr);
			} ZEND_HASH_FOREACH_END();
		}

		// php_runkit_remove_property_from_reflection_objects(ce, propname);
		php_runkit_clear_all_functions_runtime_cache();
	} else {
		if (parent_property) {
			return SUCCESS;
		} else {
			php_error_docref(NULL, E_WARNING, "%s::%s does not exist", ZSTR_VAL(ce->name), ZSTR_VAL(propname));
			return FAILURE;
		}
	}

	if (flags & ZEND_ACC_STATIC) {
		goto st_success;
	}

	PHP_RUNKIT_ITERATE_THROUGH_OBJECTS_STORE_BEGIN(i)
		if (object->ce == ce) {
			if (!remove_from_objects) {
				php_runkit_make_object_property_public(propname, object, offset, property_info_ptr);
			} else {
				if (!Z_ISUNDEF(object->properties_table[offset])) {
					if (!object->properties) {
						zval_ptr_dtor(&object->properties_table[offset]);
						ZVAL_UNDEF(&object->properties_table[offset]);
					} else {
						zend_hash_del(object->properties, property_info_ptr->name);
					}
				}
			}
		}
	PHP_RUNKIT_ITERATE_THROUGH_OBJECTS_STORE_END

st_success:
	// TODO: Should this be undefined?
	if (was_static == 0 && !Z_ISUNDEF(ce->default_properties_table[property_info_ptr->offset])) {
		zval_ptr_dtor(&ce->default_properties_table[property_info_ptr->offset]);
		// TODO: Does this make sense?
		// What is the layout of a table?
		ZVAL_UNDEF(&ce->default_properties_table[property_info_ptr->offset]);
		// php_runkit_default_class_members_list_add(&RUNKIT_G(removed_default_class_members), ce, 0, property_info_ptr->offset);
	}
	return SUCCESS;
}
/* }}} */

/* {{{ php_runkit_def_prop_remove */
static int php_runkit_def_prop_remove(zend_string *classname, zend_string *propname,
                                      zend_bool remove_from_objects)
{
	zend_class_entry *ce;

	if ((ce = php_runkit_fetch_class_int(classname)) == NULL) {
		return FAILURE;
	}

	if (ce->type & ZEND_INTERNAL_CLASS) {
		php_error_docref(NULL, E_WARNING, "Removing properties from internal classes is not allowed");
		return FAILURE;
	}

	php_runkit_clear_all_functions_runtime_cache();
	return php_runkit_def_prop_remove_int(ce, propname, NULL, 0, remove_from_objects, NULL);
}
/* }}} */

/* {{{ php_runkit_remove_property_from_reflection_objects */
/*
void php_runkit_remove_property_from_reflection_objects(zend_class_entry *ce, zend_string *propname) {
	uint32_t i;
	extern PHPAPI zend_class_entry *reflection_property_ptr;

	PHP_RUNKIT_ITERATE_THROUGH_OBJECTS_STORE_BEGIN(i)
		if (object->ce == reflection_property_ptr) {
			reflection_object *refl_obj = (reflection_object *) object;
			if (refl_obj->ptr && refl_obj->ce == ce) {
				property_reference *prop_ref = (property_reference *) refl_obj->ptr;
				if (prop_ref->ce == ce && zend_string_equals(prop_ref->prop.name, propname)) {
					if (refl_obj->ref_type == REF_TYPE_DYNAMIC_PROPERTY) {
						// TODO: set this to null?
						zend_string_release(prop_ref->prop.name);
					}
					efree(refl_obj->ptr);
					refl_obj->ptr = NULL;
				}
				php_runkit_update_reflection_object_name(object, i, RUNKIT_G(removed_property_str));
			}
		}
	PHP_RUNKIT_ITERATE_THROUGH_OBJECTS_STORE_END
}
*/
/* }}} */

/* ******************
   * Properties API *
   ****************** */

/* {{{ proto bool runkit_default_property_add(string classname, string propname, mixed initialvalue[, int visibility])
Add a property to a class with a given visibility
 */
PHP_FUNCTION(runkit_default_property_add)
{
	zend_string *classname;
	zend_string *propname;
	zval *value;
	long visibility = 0;
	int override_in_objects = 0;

	visibility = ZEND_ACC_PUBLIC;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SSz|l/", &classname, &propname,
	                          &value, &visibility) == FAILURE) {
		RETURN_FALSE;
	}

	override_in_objects = (visibility & PHP_RUNKIT_OVERRIDE_OBJECTS) != 0;

	visibility &= ~PHP_RUNKIT_OVERRIDE_OBJECTS;

	RETURN_BOOL(php_runkit_def_prop_add(classname, propname, value, visibility, override_in_objects) == SUCCESS);
}
/* }}} */

/* {{{ proto bool runkit_default_property_remove(string classname, string propname)
Remove a property from a class
 */
PHP_FUNCTION(runkit_default_property_remove)
{
	zend_string *classname;
	zend_string *propname;
	zend_bool remove_from_objects = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|b", &classname, &propname, &remove_from_objects) == FAILURE) {
		RETURN_FALSE;
	}

	RETURN_BOOL(php_runkit_def_prop_remove(classname, propname, remove_from_objects) == SUCCESS);
}
/* }}} */

#endif /* PHP_RUNKIT_MANIPULATION_PROPERTIES */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
