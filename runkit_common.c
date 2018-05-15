/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2006 The PHP Group, (c) 2008-2012 Dmitry Zenovich |
  | "runkit7" patches (c) 2015-2018 Tyson Andre                          |
  +----------------------------------------------------------------------+
  | This source file is subject to the new BSD license,                  |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.opensource.org/licenses/BSD-3-Clause                      |
  | If you did not receive a copy of the license and are unable to       |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Sara Golemon <pollita@php.net>                               |
  | Modified by Dmitry Zenovich <dzenovich@gmail.com>                    |
  | Modified for php7 by Tyson Andre<tysonandre775@hotmail.com>          |
  +----------------------------------------------------------------------+
*/

#include "runkit.h"

/* {{{ */
void ensure_all_objects_of_class_have_magic_methods(zend_class_entry *ce) {
	uint32_t i;
	if (ce->ce_flags & ZEND_ACC_USE_GUARDS) {
		return;  // Nothing to do?
	}
	// Make sure that new classes will be able to call magic methods
	ce->ce_flags |= ZEND_ACC_USE_GUARDS;
	// Make sure that existing classes will be able to call magic methods
  // TODO: Add test of adding magic methods working on objects initialized before the call to runkit.
	PHP_RUNKIT_ITERATE_THROUGH_OBJECTS_STORE_BEGIN(i)
	if (object->ce == ce) {
	  GC_FLAGS(object) |= IS_OBJ_USE_GUARDS;  // see zend_object_std_init
	  break;
	}
	PHP_RUNKIT_ITERATE_THROUGH_OBJECTS_STORE_END
}
/* }}} */

/* {{{ PHP_RUNKIT_ADD_MAGIC_METHOD */
void PHP_RUNKIT_ADD_MAGIC_METHOD(zend_class_entry *ce, zend_string* lcmname, zend_function *fe, const zend_function *orig_fe) {
	if (zend_string_equals_literal(lcmname, ZEND_CLONE_FUNC_NAME)) {
		(ce)->clone = (fe);
#if PHP_VERSION_ID < 70200
		(fe)->common.fn_flags |= ZEND_ACC_CLONE;
#endif
	} else if (zend_string_equals_literal(lcmname, ZEND_CONSTRUCTOR_FUNC_NAME)) {
		if (!(ce)->constructor || (ce)->constructor == (orig_fe)) {
			(ce)->constructor = (fe); (fe)->common.fn_flags |= ZEND_ACC_CTOR;
		}
	} else if (zend_string_equals_literal(lcmname, ZEND_DESTRUCTOR_FUNC_NAME)) {
		(ce)->destructor = (fe); (fe)->common.fn_flags |= ZEND_ACC_DTOR;
	} else if (zend_string_equals_literal(lcmname, ZEND_GET_FUNC_NAME)) {
		(ce)->__get = (fe);
		ensure_all_objects_of_class_have_magic_methods(ce);
	} else if (zend_string_equals_literal(lcmname, ZEND_SET_FUNC_NAME)) {
		(ce)->__set = (fe);
		ensure_all_objects_of_class_have_magic_methods(ce);
	} else if (zend_string_equals_literal(lcmname, ZEND_CALL_FUNC_NAME)) {
		(ce)->__call = (fe);
	} else if (zend_string_equals_literal(lcmname, ZEND_UNSET_FUNC_NAME)) {
		(ce)->__unset = (fe);
		ensure_all_objects_of_class_have_magic_methods(ce);
	} else if (zend_string_equals_literal(lcmname, ZEND_ISSET_FUNC_NAME)) {
		(ce)->__isset = (fe);
		ensure_all_objects_of_class_have_magic_methods(ce);
	} else if (zend_string_equals_literal(lcmname, ZEND_CALLSTATIC_FUNC_NAME)) {
		(ce)->__callstatic = (fe);
	} else if (zend_string_equals_literal(lcmname, ZEND_TOSTRING_FUNC_NAME)) {
		(ce)->__tostring = (fe);
	} else if (zend_string_equals_literal(lcmname, ZEND_DEBUGINFO_FUNC_NAME)) {
		(ce)->__debugInfo = (fe);
	} else if (instanceof_function_ex(ce, zend_ce_serializable, 1) && zend_string_equals_literal(lcmname, "serialize")) {
		(ce)->serialize_func = (fe);
	} else if (instanceof_function_ex(ce, zend_ce_serializable, 1) && zend_string_equals_literal(lcmname, "unserialize")) {
		(ce)->unserialize_func = (fe);
	} else if (zend_string_equals_ci(lcmname, (ce)->name)) {
		// TODO: Re-examine the changes to the constructor code for any bugs.
		if (!(ce)->constructor || (ce)->constructor == (orig_fe)) {
			(ce)->constructor = (fe);
			(fe)->common.fn_flags |= ZEND_ACC_CTOR;
		}
	}
}
/** }}} */

/* {{{ PHP_RUNKIT_DEL_MAGIC_METHOD */
void PHP_RUNKIT_DEL_MAGIC_METHOD(zend_class_entry *ce, const zend_function *fe) {
	if      ((ce)->constructor == (fe))       (ce)->constructor      = NULL;
	else if ((ce)->destructor == (fe))        (ce)->destructor       = NULL;
	else if ((ce)->__get == (fe))             (ce)->__get            = NULL;
	else if ((ce)->__set == (fe))             (ce)->__set            = NULL;
	else if ((ce)->__unset == (fe))           (ce)->__unset          = NULL;
	else if ((ce)->__isset == (fe))           (ce)->__isset          = NULL;
	else if ((ce)->__call == (fe))            (ce)->__call           = NULL;
	else if ((ce)->__callstatic == (fe))      (ce)->__callstatic     = NULL;
	else if ((ce)->__tostring == (fe))        (ce)->__tostring       = NULL;
	else if ((ce)->__debugInfo == (fe))       (ce)->__debugInfo      = NULL;
	else if ((ce)->clone == (fe))             (ce)->clone            = NULL;
	else if (instanceof_function_ex(ce, zend_ce_serializable, 1) && (ce)->serialize_func == (fe))
		(ce)->serialize_func   = NULL;
	else if (instanceof_function_ex(ce, zend_ce_serializable, 1) && (ce)->unserialize_func == (fe))
		(ce)->unserialize_func = NULL;
}
/* }}} */
