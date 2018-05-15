/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | (c) 2008-2015 Dmitry Zenovich                                        |
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
  | Author: Dmitry Zenovich <dzenovich@gmail.com>                        |
  | Modified for php7 "runkit7" by Tyson Andre<tysonandre775@hotmail.com>|
  +----------------------------------------------------------------------+
*/

#ifndef PHP_RUNKIT_ZVAL_H
#define PHP_RUNKIT_ZVAL_H

/* {{{ php_runkit_zval_resolve_class_constant */
inline static void php_runkit_zval_resolve_class_constant(zval *p, zend_class_entry *ce) {
	if (Z_CONSTANT_P(p)) {
#if PHP_VERSION_ID >= 70100
        // TODO: What does this do?
		// TODO: Make a copy if (Z_TYPE_FLAGS_P(p) & IS_TYPE_IMMUTABLE) != 0, test this out?
		zval_update_constant_ex(p, ce);
#else
		zval_update_constant_ex(p, 1, ce);
#endif
	}
}
/* }}} */


#define PHP_RUNKIT_ZVAL_CONVERT_TO_STRING_IF_NEEDED(val, val_copy) \
	if (Z_TYPE_P(member) != IS_STRING) { \
		val_copy = *val; \
		val = &val_copy; \
		zval_copy_ctor(val); \
		val->RUNKIT_REFCOUNT = 1; \
		convert_to_string(val); \
	}

#endif
