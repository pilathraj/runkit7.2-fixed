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
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Sara Golemon <pollita@php.net>                               |
  | Props:  Wez Furlong                                                  |
  | Modified by Dmitry Zenovich <dzenovich@gmail.com>                    |
  | Modified for php7 by Tyson Andre <tysonandre775@hotmail.com>         |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "runkit.h"
#include "ext/standard/php_smart_string.h"

#ifdef PHP_RUNKIT_SANDBOX

#include "Zend/zend.h"  // debugging, tsrm_ls_cache
// TODO: ifdef compile_dl_runkit, which is always?

#include "SAPI.h"
#include "php_main.h"
#include "php_runkit_sandbox.h"
#include "php_runkit_zval.h"
#include "Zend/zend_language_scanner.h"

#include <ext/standard/php_var.h>  // debug

#if 0
static zend_object_handlers php_runkit_sandbox_object_handlers;
static zend_class_entry *php_runkit_sandbox_class_entry;

#define PHP_RUNKIT_SANDBOX_BEGIN(objval) \
{ \
	void *prior_context = tsrm_set_interpreter_context(objval->context);

#define PHP_RUNKIT_SANDBOX_ABORT(objval) \
{ \
	tsrm_set_interpreter_context(prior_context); \
}

#define PHP_RUNKIT_SANDBOX_END(objval) \
	PHP_RUNKIT_SANDBOX_ABORT(objval) \
	if (objval->bailed_out_in_eval) { \
		/* We're actually in bailout mode, but the child's bailout address had to resolve first */ \
		zend_bailout(); \
	} \
}

#define PHP_RUNKIT_SANDBOX_FETCHBOX(zval_p) (php_runkit_sandbox_object*)zend_objects_get_address(zval_p)

int php_runkit_sandbox_array_deep_copy(RUNKIT_53_TSRMLS_ARG(zval **value), int num_args, va_list args, zend_hash_key *hash_key)
{
	HashTable *target_hashtable = va_arg(args, HashTable*);
	zval *copyval;

	MAKE_STD_ZVAL(copyval);
	*copyval = **value;

	PHP_SANDBOX_CROSS_SCOPE_ZVAL_COPY_CTOR(copyval);

	if (hash_key->nKeyLength) {
		zend_u_hash_quick_update(target_hashtable, hash_key->type == HASH_KEY_IS_UNICODE ? IS_UNICODE : IS_STRING, hash_key->u.string, hash_key->nKeyLength, hash_key->h, &copyval, sizeof(zval*), NULL);
	} else {
		zend_hash_index_update(target_hashtable, hash_key->h, &copyval, sizeof(zval*), NULL);
	}

	return ZEND_HASH_APPLY_KEEP;
}

/* ******************
   * Runkit_Sandbox *
   ****************** */

static HashTable *php_runkit_sandbox_parse_multipath(const char *paths) {
	HashTable *ht;
	int len = strlen(paths);
	char *s, *colon, *pathcopy, tmppath[MAXPATHLEN] = {0};

	if (!len) {
		return NULL;
	}

	pathcopy = estrndup(paths, len);

	ALLOC_HASHTABLE(ht);
	zend_hash_init(ht, 4, NULL, NULL, 0);
	for (s = pathcopy; (colon = strchr(s, ZEND_PATHS_SEPARATOR)); s = colon + 1) {
		if (colon > s) {
			*colon = 0;
			VCWD_REALPATH(s, tmppath);
			if (*tmppath) {
				zend_hash_next_index_insert(ht, tmppath, strlen(tmppath) + 1, NULL);
			} else {
				/* s isn't real, leave it in the HashTable anyway
				 * to avoid making the old setting look like
				 * it had no values in it and thus allowing privilege elevation.
				 */
				zend_hash_next_index_insert(ht, s, (colon - s) + 1, NULL);
			}
		}
	}

	if (*s) {
		VCWD_REALPATH(s, tmppath);
		if (*tmppath) {
			zend_hash_next_index_insert(ht, tmppath, strlen(tmppath) + 1, NULL);
		} else {
			/* See above */
			zend_hash_next_index_insert(ht, s, strlen(s) + 1, NULL);
		}
	}

	efree(pathcopy);
	return ht;
}

static void php_runkit_sandbox_free_multipath(HashTable *ht) {
	if (ht) {
		zend_hash_destroy(ht);
		FREE_HASHTABLE(ht);
	}
}

static char *php_runkit_sandbox_implode_stringht(HashTable *ht) {
	smart_str s = { 0 };
	HashPosition pos;
	char *str;

	for(zend_hash_internal_pointer_reset_ex(ht, &pos);
	    SUCCESS == zend_hash_get_current_data_ex(ht, (void**)&str, &pos);
	    zend_hash_move_forward_ex(ht, &pos)) {
		if (s.len) {
			smart_str_appendc(&s, ZEND_PATHS_SEPARATOR);
		}
		smart_str_appends(&s, str);
	}
	smart_str_0(&s);
	return s.c;
}

static char *php_runkit_sandbox_tighten_paths(HashTable *oldht, HashTable *newht) {
	HashPosition newpos;
	char *newstr;

	if (!oldht && !newht) {
		return NULL;
	}
	if (!oldht) {
		/* From nothing to something */
		return php_runkit_sandbox_implode_stringht(newht);
	}
	if (!newht) {
		/* From something to nothing */
		return NULL;
	}

	if (zend_hash_num_elements(oldht) == 0) {
		/* Special edge case
		 * The parent's setting is ':' or similar.
		 * Despite looking similar to an empty setting,
		 * this actually has the opposite meaning.
		 * '' provides open access to the filesystem
		 * ':' provides access to none of it.
		 * THIS DISTINCTION MATTERS. :p
		 */
		return NULL;
	}

	for(zend_hash_internal_pointer_reset_ex(newht, &newpos);
	    SUCCESS == zend_hash_get_current_data_ex(newht, (void**)&newstr, &newpos);
	    zend_hash_move_forward_ex(newht, &newpos)) {
		HashPosition oldpos;
		char *oldstr;
		int newstr_len = strlen(newstr);

		for(zend_hash_internal_pointer_reset_ex(oldht, &oldpos);
		    SUCCESS == zend_hash_get_current_data_ex(oldht, (void**)&oldstr, &oldpos);
		    zend_hash_move_forward_ex(oldht, &oldpos)) {
			int oldstr_len = strlen(oldstr);
			if ((oldstr_len <= newstr_len) &&
			    !strncmp(oldstr, newstr, oldstr_len) &&
			    ((oldstr_len == newstr_len) || (newstr[oldstr_len] == DEFAULT_SLASH))) {
				goto newstr_ok;
			}
		}
		/* Couldn't find an old path that we're constricting, so fail */
		return NULL;
newstr_ok: ;
	}

	return php_runkit_sandbox_implode_stringht(newht);
}

/* Special .ini options (normally PHP_INI_SYSTEM) which can be overridden within a sandbox in the direction of tighter security
 *
 * safe_mode = true
 *		safe_mode can only be turned on for a sandbox, not off.  Doing so would tend to defeat safe_mode as applied to the calling script
 * safe_mode_gid = false
 *		safe_mode_gid can only be turned off as it allows a partial bypass of safe_mode restrictions when on
 * safe_mode_include_dir = /path/to/includables
 *		safe_mode_include_dir must be at or below the currently defined include_dir
 * open_basedir = /path/to/basedir
 *		open_basedir must be at or below the currently defined basedir for the same reason that safe_mode can only be turned on
 * allow_url_fopen = false
 *		allow_url_fopen may only be turned off for a sandbox, not on.   Once again, don't castrate the existing restrictions
 * allow_url_include = false
 *		allow_url_include may only be turned off for a sandbox, not on.   Once again, don't castrate the existing restrictions
 * disable_functions = coma_separated,list_of,additional_functions
 *		ADDITIONAL functions, on top of already disabled functions to disable
 * disable_classes = coma_separated,list_of,additional_classes
 *		ADDITIONAL classes, on top of already disabled classes to disable
 * runkit.superglobals = coma_separated,list_of,superglobals
 *		ADDITIONAL superglobals to define in the subinterpreter
 * runkit.internal_override = false
 *		runkit.internal_override may be disabled (but not re-enabled) within sandboxes
 */
static inline void php_runkit_sandbox_ini_override(php_runkit_sandbox_object *objval, HashTable *options)
{
	zend_bool allow_url_fopen;
#ifdef ZEND_ENGINE_2_2
	zend_bool allow_url_include;
#endif
	HashTable *open_basedirs = NULL;
	zval **tmpzval;

	/* Collect up parent values */
	tsrm_set_interpreter_context(objval->parent_context);
	{
		/* Check current settings in parent context */
		TSRMLS_FETCH_FROM_CTX(objval->parent_context);
		if (PG(open_basedir) && *PG(open_basedir)) {
			open_basedirs = php_runkit_sandbox_parse_multipath(PG(open_basedir));
		}
		allow_url_fopen = PG(allow_url_fopen);
#ifdef ZEND_ENGINE_2_2
		allow_url_include = PG(allow_url_include);
#endif
	}
	tsrm_set_interpreter_context(objval->context);

	/* open_basedir goes deeper only */
	if ((tmpzval = zend_hash_str_find(options, "open_basedir", sizeof("open_basedir"), (void*)&tmpzval)) != NULL &&
		Z_TYPE_PP(tmpzval) == IS_STRING) {

		if (!open_basedirs) {
			/* simplest case -- no open basedir existed yet */
			zend_alter_ini_entry("open_basedir", sizeof("open_basedir"), Z_STRVAL_PP(tmpzval), Z_STRLEN_PP(tmpzval), PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);
			goto child_open_basedir_set;
		} else {
			HashTable *new_open_basedirs = php_runkit_sandbox_parse_multipath(Z_STRVAL_PP(tmpzval));			char *new_open_basedir = php_runkit_sandbox_tighten_paths(open_basedirs, new_open_basedirs);
			php_runkit_sandbox_free_multipath(new_open_basedirs);

			if (new_open_basedir) {
				/* Tightening up of existing security level */
				zend_alter_ini_entry("open_basedir", sizeof("open_basedir"), new_open_basedir, strlen(new_open_basedir), PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);
				efree(new_open_basedir);
				goto child_open_basedir_set;
			}
		}
	}
	if (open_basedirs) {
		/* Inherit parent's setting by default which may be PHP_INI_USER level setting */
		char *parent_open_basedir = php_runkit_sandbox_implode_stringht(open_basedirs);
		zend_alter_ini_entry("open_basedir", sizeof("open_basedir"), parent_open_basedir, strlen(parent_open_basedir), PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);
		efree(parent_open_basedir);
	}
child_open_basedir_set:

	/* allow_url_fopen goes off only */
	if (allow_url_fopen &&
		(tmpzval = zend_hash_str_find(options, "allow_url_fopen", sizeof("allow_url_fopen"), (void*)&tmpzval)) != NULL) {
		zval copyval = **tmpzval;

		zval_copy_ctor(&copyval);
		convert_to_boolean(&copyval);

		if (!Z_BVAL(copyval)) {
			zend_alter_ini_entry("allow_url_fopen", sizeof("allow_url_fopen"), "0", 1, PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);
		}
	}

	/* Can only disable additional functions */
	if ((tmpzval = zend_hash_str_find(options, "disable_functions", sizeof("disable_functions"), (void*)&tmpzval)) != NULL &&
		Z_TYPE_PP(tmpzval) == IS_STRING) {
		/* NOTE: disable_functions doesn't prevent $obj->function_name, it only blocks code inside $obj->eval() statements
		 * This could be brought into consistency, but I actually think it's okay to leave those functions available to calling script
		 */
		/* This buffer needs to be around when the error message occurs since the underlying implementation in Zend expects it to be */
		int disable_functions_len = Z_STRLEN_PP(tmpzval);
		char *p, *s;

		objval->disable_functions = estrndup(Z_STRVAL_PP(tmpzval), disable_functions_len);

		s = objval->disable_functions;
		while ((p = strchr(s, ','))) {
			*p = '\0';
			zend_disable_function(s, p - s);
			s = p + 1;
		}
		zend_disable_function(s, strlen(s));
	}

	/* Can only disable additional classes */
	if ((tmpzval = zend_hash_str_find(options, "disable_classes", sizeof("disable_classes"), (void*)&tmpzval)) != NULL &&
		Z_TYPE_PP(tmpzval) == IS_STRING) {
		/* This buffer needs to be around when the error message occurs since the underlying implementation in Zend expects it to be */
		int disable_classes_len = Z_STRLEN_PP(tmpzval);
		char *p, *s;

		objval->disable_classes = estrndup(Z_STRVAL_PP(tmpzval), disable_classes_len);

		s = objval->disable_classes;
		while ((p = strchr(s, ','))) {
			*p = '\0';
			zend_disable_class(s, p - s);
			s = p + 1;
		}
		zend_disable_class(s, strlen(s));
	}

	/* Additional superglobals to define */
	if ((tmpzval = zend_hash_str_find(options, "runkit.superglobal", sizeof("runkit.superglobal"), (void*)&tmpzval)) != NULL &&
		Z_TYPE_PP(tmpzval) == IS_STRING) {
		char *p, *s = Z_STRVAL_PP(tmpzval);
		int len;

		while ((p = strchr(s, ','))) {
			if (p - s) {
				*p = '\0';
				zend_register_auto_global(zend_string_init(s, p - s, 0), 0, NULL);
				zend_activate_auto_globals();
				*p = ',';
			}
			s = p + 1;
		}
		zend_register_auto_global(zend_string_init(s, strlen(s), 0), 0, NULL);
	}

	/* May only turn off */
	if (zend_hash_str_find(options, "runkit.internal_override", sizeof("runkit.internal_override"), (void*)&tmpzval) == SUCCESS) {
		zval copyval = **tmpzval;

		zval_copy_ctor(&copyval);
		convert_to_boolean(&copyval);

		if (!Z_BVAL(copyval)) {
			zend_alter_ini_entry("runkit.internal_override", sizeof("runkit.internal_override"), "0", 1, PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);
		}
	}

#ifdef ZEND_ENGINE_2_2
	/* May only turn off */
	if (allow_url_include &&
	    ((tmpzval = zend_hash_str_find(options, "allow_url_include", sizeof("allow_url_include"), (void**)&tmpzval)) != NULL) &&
	    !zend_is_true(*tmpzval)) {
		zend_alter_ini_entry("allow_url_include", sizeof("allow_url_include"), "0", 1, PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);
	}
#endif

	if (open_basedirs) {
		tsrm_set_interpreter_context(objval->parent_context);
    php_runkit_sandbox_free_multipath(open_basedirs);
		tsrm_set_interpreter_context(objval->context);
	}
}
/* }}} */

/* {{{ proto void Runkit_Sandbox::__construct(array options)
 * Options: see php_runkit_sandbox_ini_override()
 */
PHP_METHOD(Runkit_Sandbox,__construct)
{
	php_runkit_sandbox_object *objval = PHP_RUNKIT_SANDBOX_FETCHBOX(this_ptr);
	zval *options = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|a", &options) == FAILURE) {
		RETURN_NULL();
	}

	objval->context = tsrm_new_interpreter_context();
	objval->disable_functions = NULL;
	objval->disable_classes = NULL;
	objval->output_handler = NULL;

	PHP_RUNKIT_SANDBOX_BEGIN(objval)
		objval->parent_context = prior_context;

		zend_alter_ini_entry("implicit_flush", sizeof("implicit_flush") , "1", 1, PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);
		zend_alter_ini_entry("max_execution_time", sizeof("max_execution_time") , "0", 1, PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);
		if (options) {
			/* Override a select subset of .ini options for increased restriction in the sandbox */
			php_runkit_sandbox_ini_override(objval, Z_ARRVAL_P(options));
		}

		SG(headers_sent) = 1;
		SG(request_info).no_headers = 1;
		SG(options) = SAPI_OPTION_NO_CHDIR;
		RUNKIT_G(current_sandbox) = objval; /* Needs to be set before RINIT */
		php_request_startup();
		RUNKIT_G(current_sandbox) = objval; /* But gets reset during RINIT -- Bad design on my part */
		PG(during_request_startup) = 0;
	PHP_RUNKIT_SANDBOX_END(objval)

	/* Prime the sandbox to be played in */
	objval->active = 1;

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto Runkit_Sandbox::__call(mixed function_name, array args)
	Call User Function */
PHP_METHOD(Runkit_Sandbox,__call)
{
	zval *func_name, *args, *retval = NULL;
	php_runkit_sandbox_object *objval;
	int bailed_out = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "za", &func_name, &args) == FAILURE) {
		RETURN_NULL();
	}

	objval = PHP_RUNKIT_SANDBOX_FETCHBOX(this_ptr);
	if (!objval->active) {
		php_error_docref(NULL, E_WARNING, "Current sandbox is no longer active");
		RETURN_NULL();
	}

	PHP_RUNKIT_SANDBOX_BEGIN(objval)
	{
		char *name = NULL;

		zend_first_try {
			if (!RUNKIT_IS_CALLABLE(func_name, IS_CALLABLE_CHECK_NO_ACCESS, &name)) {
				php_error_docref1(NULL, name, E_WARNING, "Function not defined");
				if (name) {
					efree(name);
				}
				PHP_RUNKIT_SANDBOX_ABORT(objval)
				RETURN_FALSE;
			}

			php_runkit_sandbox_call_int(func_name, &name, &retval, args, return_value, prior_context);
		} zend_catch {
			bailed_out = 1;
			objval->active = 0;
		} zend_end_try();
	}
	PHP_RUNKIT_SANDBOX_END(objval)

	if (bailed_out) {
		php_error_docref(NULL, E_WARNING, "Failed calling sandbox function");
		RETURN_FALSE;
	}

	PHP_SANDBOX_CROSS_SCOPE_ZVAL_COPY_CTOR(return_value);

	if (retval) {
		PHP_RUNKIT_SANDBOX_BEGIN(objval)
		zval_ptr_dtor(&retval);
		PHP_RUNKIT_SANDBOX_END(objval)
	}
}
/* }}} */

/* {{{ php_runkit_sandbox_include_or_eval
 */
static void php_runkit_sandbox_include_or_eval(INTERNAL_FUNCTION_PARAMETERS, int type, int once)
{
	php_runkit_sandbox_object *objval;
	zval *zcode;
	int bailed_out = 0;
	zval *retval = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &zcode) == FAILURE) {
		RETURN_FALSE;
	}

	convert_to_string(zcode);

	objval = PHP_RUNKIT_SANDBOX_FETCHBOX(this_ptr);
	if (!objval->active) {
		php_error_docref(NULL, E_WARNING, "Current sandbox is no longer active");
		RETURN_NULL();
	}

	RETVAL_NULL();

	PHP_RUNKIT_SANDBOX_BEGIN(objval)
		zend_first_try {
			zend_op_array *op_array = NULL;
			int already_included = 0;

			op_array = php_runkit_sandbox_include_or_eval_int(return_value, zcode, type, once, &already_included);

			if (op_array) {
				EG(return_value_ptr_ptr) = &retval;
				EG(active_op_array) = op_array;

				zend_execute(op_array);

				if (retval) {
					*return_value = *retval;
				} else {
					RETVAL_TRUE;
				}

				destroy_op_array(op_array);
				efree(op_array);
			} else if ((type != ZEND_INCLUDE) && !already_included) {
				/* include can fail to parse peacefully,
				 * require and eval should die on failure
				 */
				objval->active = 0;
				bailed_out = 1;
			}
		} zend_catch {
            /* It's impossible to know what caused the failure, just deactive the sandbox now */
			objval->active = 0;
			bailed_out = 1;
		} zend_end_try();
	PHP_RUNKIT_SANDBOX_END(objval)

	if (bailed_out) {
		php_error_docref(NULL, E_WARNING, "Error executing sandbox code");
		RETURN_FALSE;
	}

	PHP_SANDBOX_CROSS_SCOPE_ZVAL_COPY_CTOR(return_value);

	/* Don't confuse the memory manager */
	if (retval) {
		PHP_RUNKIT_SANDBOX_BEGIN(objval)
		zval_ptr_dtor(&retval);
		PHP_RUNKIT_SANDBOX_END(objval)
	}
}
/* }}} */

/* {{{ proto Runkit_Sandbox::eval(string phpcode)
	Evaluate php code within the sandbox environment */
PHP_METHOD(Runkit_Sandbox,eval)
{
	php_runkit_sandbox_include_or_eval(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_EVAL, 0);
}
/* }}} */

/* {{{ proto Runkit_Sandbox::include(string filename)
	Evaluate php code from a file within the sandbox environment */
PHP_METHOD(Runkit_Sandbox,include)
{
	php_runkit_sandbox_include_or_eval(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_INCLUDE, 0);
}
/* }}} */

/* {{{ proto Runkit_Sandbox::include_once(string filename)
	Evaluate php code from a file within the sandbox environment */
PHP_METHOD(Runkit_Sandbox,include_once)
{
	php_runkit_sandbox_include_or_eval(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_INCLUDE, 1);
}
/* }}} */

/* {{{ proto Runkit_Sandbox::require(string filename)
	Evaluate php code from a file within the sandbox environment */
PHP_METHOD(Runkit_Sandbox,require)
{
	php_runkit_sandbox_include_or_eval(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_REQUIRE, 0);
}
/* }}} */

/* {{{ proto Runkit_Sandbox::require_once(string filename)
	Evaluate php code from a file within the sandbox environment */
PHP_METHOD(Runkit_Sandbox,require_once)
{
	php_runkit_sandbox_include_or_eval(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_REQUIRE, 1);
}
/* }}} */

/* {{{ proto null Runkit_Sandbox::echo(mixed var[, mixed var[, ... ]])
	Echo through the sandbox
	The only thing which distinguished this from a non-sandboxed echo
	is that content gets processed through the sandbox's output_handler (if present) */
PHP_METHOD(Runkit_Sandbox,echo)
{
	php_runkit_sandbox_object *objval;
	zval ***argv;
	int i, argc = ZEND_NUM_ARGS();

	argv = emalloc(sizeof(zval**) * argc);
	if (zend_get_parameters_array_ex(argc, argv) == FAILURE) {
		efree(argv);
		/* Big problems... */
		RETURN_NULL();
	}

	for(i = 0; i < argc; i++) {
		/* Prepare for output */
		convert_to_string(*(argv[i]));
	}

	objval = PHP_RUNKIT_SANDBOX_FETCHBOX(this_ptr);
	if (!objval->active) {
		efree(argv);
		php_error_docref(NULL, E_WARNING, "Current sandbox is no longer active");
		RETURN_NULL();
	}

	PHP_RUNKIT_SANDBOX_BEGIN(objval)
		zend_try {
			for(i = 0; i < argc; i++) {
				PHPWRITE(Z_STRVAL_PP(argv[i]),Z_STRLEN_PP(argv[i]));
			}
		} zend_catch {
			objval->active = 0;
		} zend_end_try();
	PHP_RUNKIT_SANDBOX_END(objval)

	efree(argv);

	RETURN_NULL();
}
/* }}} */

/* {{{ proto bool Runkit_Sandbox::print(mixed var)
	Echo through the sandbox
	The only thing which distinguished this from a non-sandboxed echo
	is that content gets processed through the sandbox's output_handler (if present) */
PHP_METHOD(Runkit_Sandbox,print)
{
	php_runkit_sandbox_object *objval;
	char *str;
	int len;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &str, &len) == FAILURE) {
		RETURN_FALSE;
	}

	objval = PHP_RUNKIT_SANDBOX_FETCHBOX(this_ptr);
	if (!objval->active) {
		php_error_docref(NULL, E_WARNING, "Current sandbox is no longer active");
		RETURN_NULL();
	}

	PHP_RUNKIT_SANDBOX_BEGIN(objval)
		zend_try {
			PHPWRITE(str,len);
		} zend_catch {
			objval->active = 0;
		} zend_end_try();
	PHP_RUNKIT_SANDBOX_END(objval)

	RETURN_BOOL(len > 1 || (len == 1 && str[0] != '0'));
}
/* }}} */

/* {{{ proto void Runkit_Sandbox::die(mixed message)
	MALIAS(exit)
	Terminate a sandbox instance */
PHP_METHOD(Runkit_Sandbox,die)
{
	php_runkit_sandbox_object *objval;
	zval *message = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &message) == FAILURE) {
		RETURN_FALSE;
	}

	RETVAL_NULL();

	if (message && Z_TYPE_P(message) != IS_LONG) {
		convert_to_string(message);
	}

	objval = PHP_RUNKIT_SANDBOX_FETCHBOX(this_ptr);
	if (!objval->active) {
		php_error_docref(NULL, E_WARNING, "Current sandbox is no longer active");
		return;
	}

	PHP_RUNKIT_SANDBOX_BEGIN(objval)
		zend_try {
			if (message) {
				if (Z_TYPE_P(message) == IS_LONG) {
					EG(exit_status) = Z_LVAL_P(message);
				} else {
					PHPWRITE(Z_STRVAL_P(message), Z_STRLEN_P(message));
				}
			}
			zend_bailout();
		} zend_catch {
			/* goes without saying doesn't it? */
			objval->active = 0;
		} zend_end_try();
	PHP_RUNKIT_SANDBOX_END(objval)
}
/* }}} */

/* *********************
   * Property Handlers *
   ********************* */

/* {{{ php_runkit_sandbox_read_property
	read_property handler */
static zval *php_runkit_sandbox_read_property(zval *object, zval *member, int type
	, const zend_literal *key)
{
	php_runkit_sandbox_object *objval = PHP_RUNKIT_SANDBOX_FETCHBOX(object);
	zval tmp_member;
	zval retval;
	int prop_found = 0;

	if (!objval->active) {
		php_error_docref(NULL, E_WARNING, "Current sandbox is no longer active");
		return EG(uninitialized_zval_ptr);
	}

	PHP_RUNKIT_ZVAL_CONVERT_TO_STRING_IF_NEEDED(member, tmp_member);

	PHP_RUNKIT_SANDBOX_BEGIN(objval)
		zend_try {
			zval **value;

			if ((value = zend_hash_str_find(&EG(symbol_table), Z_STRVAL_P(member), Z_STRLEN_P(member) + 1, (void*)&value)) != NULL) {
				retval = **value;
				prop_found = 1;
			}
		} zend_catch {
			/* Almost certainly impossible... */
			objval->active = 0;
		} zend_end_try();
	PHP_RUNKIT_SANDBOX_END(objval)

	if (member == &tmp_member) {
		zval_dtor(member);
	}

	return php_runkit_sandbox_return_property_value(prop_found, &retval);
}
/* }}} */

/* {{{ php_runkit_sandbox_write_property
	write_property handler */
static void php_runkit_sandbox_write_property(zval *object, zval *member, zval *value
	, const zend_literal *key)
{
	php_runkit_sandbox_object *objval = PHP_RUNKIT_SANDBOX_FETCHBOX(object);
	zval tmp_member;

	if (!objval->active) {
		php_error_docref(NULL, E_WARNING, "Current sandbox is no longer active");
		return;
	}

	PHP_RUNKIT_ZVAL_CONVERT_TO_STRING_IF_NEEDED(member, tmp_member);

	PHP_RUNKIT_SANDBOX_BEGIN(objval)
		zend_try {
			zval *copyval;

			MAKE_STD_ZVAL(copyval);
			*copyval = *value;
			PHP_SANDBOX_CROSS_SCOPE_ZVAL_COPY_CTOR(copyval);
			ZEND_SET_SYMBOL(&EG(symbol_table), Z_STRVAL_P(member), copyval);
		} zend_catch {
			/* An emalloc() could bailout */
			objval->active = 0;
		} zend_end_try();
	PHP_RUNKIT_SANDBOX_END(objval)

	if (member == &tmp_member) {
		zval_dtor(member);
	}
}
/* }}} */

/* {{{ php_runkit_sandbox_has_property
	has_property handler */
static int php_runkit_sandbox_has_property(zval *object, zval *member, int has_set_exists
	, const zend_literal *key)
{
	php_runkit_sandbox_object* objval = PHP_RUNKIT_SANDBOX_FETCHBOX(object);
	zval member_copy;
	int result = 0;

	if (!objval) {
		return 0;
	}
	/* It's okay to read the symbol table post bailout */

	PHP_RUNKIT_ZVAL_CONVERT_TO_STRING_IF_NEEDED(member, member_copy);

	PHP_RUNKIT_SANDBOX_BEGIN(objval)
		php_runkit_sandbox_has_property_int(has_set_exists, member);
	PHP_RUNKIT_SANDBOX_END(objval)

	if (member == &member_copy) {
		zval_dtor(member);
	}

	return result;
}
/* }}} */

/* {{{ php_runkit_sandbox_unset_property
	unset_property handler */
static void php_runkit_sandbox_unset_property(zval *object, zval *member
	, const zend_literal *key)
{
	php_runkit_sandbox_object *objval;
	zval member_copy;

	objval = PHP_RUNKIT_SANDBOX_FETCHBOX(object);
	if (!objval) {
		return;
	}
	if (!objval->active) {
		php_error_docref(NULL, E_WARNING, "Current sandbox is no longer active");
		return;
	}

	PHP_RUNKIT_ZVAL_CONVERT_TO_STRING_IF_NEEDED(member, member_copy);

	PHP_RUNKIT_SANDBOX_BEGIN(objval)
		zend_hash_del(&EG(symbol_table), Z_STRVAL_P(member), Z_STRLEN_P(member) + 1);
	PHP_RUNKIT_SANDBOX_END(objval)

	if (member == &member_copy) {
		zval_dtor(member);
	}
}
/* }}} */

/* **************
   * SAPI Wedge *
   ************** */

static sapi_module_struct php_runkit_sandbox_original_sapi;

/* {{{ php_runkit_sandbox_sapi_ub_write
 * Wrap the caller's output handler with a mechanism for switching contexts
 */
static int php_runkit_sandbox_sapi_ub_write(const char *str, uint str_length)
{
	php_runkit_sandbox_object *objval = RUNKIT_G(current_sandbox);
	int bytes_written;

	if (!str_length) {
		return 0;
	}

	if (!objval) {
		/* Not in a sandbox -- Use genuine sapi.ub_write handler */
		if (php_runkit_sandbox_original_sapi.ub_write) {
			return php_runkit_sandbox_original_sapi.ub_write(str, str_length);
		} else {
			/* Ignore data, no real SAPI handler to pass it to */
			return str_length;
		}
	}

	tsrm_set_interpreter_context(objval->parent_context);
	{
		zval **output_buffer[1], *bufcopy, *retval = NULL;

		if (!objval->output_handler ||
			!RUNKIT_IS_CALLABLE(objval->output_handler, IS_CALLABLE_CHECK_NO_ACCESS, NULL)) {
			/* No hander, or invalid handler, pass up the line... */
			bytes_written = PHPWRITE(str, str_length);

			tsrm_set_interpreter_context(objval->context);

			return bytes_written;
		}

		MAKE_STD_ZVAL(bufcopy);
		ZVAL_STRINGL(bufcopy, (char*)str, str_length, 1);
		output_buffer[0] = &bufcopy;

		if (call_user_function_ex(EG(function_table), NULL, objval->output_handler, &retval, 1, output_buffer, 0, NULL) == SUCCESS) {
			if (retval) {
				if (Z_TYPE_P(retval) != IS_NULL) {
					convert_to_string(retval);
					PHPWRITE(Z_STRVAL_P(retval), Z_STRLEN_P(retval));
				}
				zval_ptr_dtor(&retval);
				bytes_written = str_length;
			} else {
				bytes_written = 0;
			}
		} else {
			php_error_docref("runkit.sandbox", E_WARNING, "Unable to call output buffer callback");
			bytes_written = 0;
		}

		if (bufcopy) {
			zval_ptr_dtor(&bufcopy);
		}
	}
	tsrm_set_interpreter_context(objval->context);

	return bytes_written;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_flush
 */
static void php_runkit_sandbox_sapi_flush(void *server_context)
{
	php_runkit_sandbox_object *objval;

	objval = RUNKIT_G(current_sandbox);

	if (!objval) {
		/* Not in a sandbox -- Use genuine sapi.flush handler */
		if (php_runkit_sandbox_original_sapi.flush) {
			php_runkit_sandbox_original_sapi.flush(server_context);
		}
		return;
	}

	tsrm_set_interpreter_context(objval->parent_context);
	{
		zval *retval = NULL, **args[1];

		if (!objval->output_handler ||
			!RUNKIT_IS_CALLABLE(objval->output_handler, IS_CALLABLE_CHECK_NO_ACCESS, NULL)) {
			/* No hander, or invalid handler, pass up the line... */
			if (php_runkit_sandbox_original_sapi.flush) {
				php_runkit_sandbox_original_sapi.flush(server_context);
			}
			tsrm_set_interpreter_context(objval->context);
			return;
		}

		args[0] = &EG(uninitialized_zval_ptr);

		if (call_user_function_ex(EG(function_table), NULL, objval->output_handler, &retval, 1, args, 0, NULL) == SUCCESS) {
			if (retval) {
				if (Z_TYPE_P(retval) != IS_NULL) {
					convert_to_string(retval);
					PHPWRITE(Z_STRVAL_P(retval), Z_STRLEN_P(retval));
				}
				zval_ptr_dtor(&retval);
			}
		} else {
			php_error_docref("runkit.sandbox", E_WARNING, "Unable to call output buffer callback");
		}
	}
	tsrm_set_interpreter_context(objval->context);

	return;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_get_stat
 */
static struct stat *php_runkit_sandbox_sapi_get_stat()
{
	php_runkit_sandbox_object *objval = RUNKIT_G(current_sandbox);
	struct stat *ret;

	if (objval) {
		tsrm_set_interpreter_context(objval->parent_context);
		{
			ret = php_runkit_sandbox_original_sapi.get_stat();
		}
		tsrm_set_interpreter_context(objval->context);
	} else {
		ret = php_runkit_sandbox_original_sapi.get_stat();
	}

	return ret;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_getenv
 */
static char *php_runkit_sandbox_sapi_getenv(char *name, size_t name_len)
{
	php_runkit_sandbox_object *objval = RUNKIT_G(current_sandbox);
	char *ret;

	if (objval) {
		tsrm_set_interpreter_context(objval->parent_context);
		{
			ret = php_runkit_sandbox_original_sapi.getenv(name, name_len);
		}
		tsrm_set_interpreter_context(objval->context);
	} else {
		ret = php_runkit_sandbox_original_sapi.getenv(name, name_len);
	}

	return ret;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_sapi_error
 */
static void php_runkit_sandbox_sapi_sapi_error(int type, const char *error_msg, ...)
{
	char *message;
	va_list args;
	php_runkit_sandbox_object *objval;

	objval = RUNKIT_G(current_sandbox);

	va_start(args, error_msg);
	vspprintf(&message, 0, error_msg, args);
	va_end(args);

	if (objval) {
		tsrm_set_interpreter_context(objval->parent_context);
		{
			php_runkit_sandbox_original_sapi.sapi_error(type, "%s", message);
		}
	} else {
		php_runkit_sandbox_original_sapi.sapi_error(type, "%s", message);
	}

	efree(message);

	return;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_header_handler
 * Ignore headers when in a subrequest
 */
static int php_runkit_sandbox_sapi_header_handler(sapi_header_struct *sapi_header,
                                                  sapi_header_op_enum op,
                                                  sapi_headers_struct *sapi_headers)
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		return php_runkit_sandbox_original_sapi.header_handler(sapi_header,
                                                               op,
                                                               sapi_headers);
	}

	/* Otherwise ignore headers -- TODO: Provide a way for the calling scope to receive these a la output handler */
	return SAPI_HEADER_ADD;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_send_headers
 * Pretend to send headers
 */
static int php_runkit_sandbox_sapi_send_headers(sapi_headers_struct *sapi_headers)
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		return php_runkit_sandbox_original_sapi.send_headers(sapi_headers);
	}

	/* Do nothing */
	return SAPI_HEADER_SENT_SUCCESSFULLY;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_send_header
 * Pretend to send header
 */
static void php_runkit_sandbox_sapi_send_header(sapi_header_struct *sapi_header, void *server_context)
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		php_runkit_sandbox_sapi_send_header(sapi_header, server_context);
		return;
	}

	/* Do nothing */
	return;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_read_post
 */
static int php_runkit_sandbox_sapi_read_post(char *buffer, uint count_bytes)
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		return php_runkit_sandbox_original_sapi.read_post(buffer, count_bytes);
	}

	/* return nothing */
	return 0;
}
/* }}} */

/* {{( php_runkit_sandbox_sapi_read_cookies
 * Pretend to read cookies
 */
static char *php_runkit_sandbox_sapi_read_cookies()
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		return php_runkit_sandbox_original_sapi.read_cookies();
	}

	/* Do nothing */
	return NULL;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_register_server_variables
 */
static void php_runkit_sandbox_sapi_register_server_variables(zval *track_vars_array)
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		php_runkit_sandbox_original_sapi.register_server_variables(track_vars_array);
	}

	/* Do nothing */
	return;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_log_message
 */
static void php_runkit_sandbox_sapi_log_message(char *message)
{

	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		php_runkit_sandbox_original_sapi.log_message(message);
		return;
	}

	/* Do nothing */
	return;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_get_request_time
 */
static double php_runkit_sandbox_sapi_get_request_time()
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		return php_runkit_sandbox_original_sapi.get_request_time();
	}

	/* Parrot what main/SAPI.c does */
	if (!SG(global_request_time)) {
		SG(global_request_time) = time(0);
	}
	return SG(global_request_time);
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_block_interruptions
 */
static void php_runkit_sandbox_sapi_block_interruptions(void)
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		php_runkit_sandbox_original_sapi.block_interruptions();
		return;
	}

	/* Do nothing */
	return;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_unblock_interruptions
 */
static void php_runkit_sandbox_sapi_unblock_interruptions(void)
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		php_runkit_sandbox_original_sapi.unblock_interruptions();
		return;
	}

	/* Do nothing */
	return;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_default_post_reader
 */
static void php_runkit_sandbox_sapi_default_post_reader()
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		php_runkit_sandbox_original_sapi.default_post_reader();
		return;
	}

	/* Do nothing */
	return;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_get_fd
 */
static int php_runkit_sandbox_sapi_get_fd(int *fd)
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		return php_runkit_sandbox_original_sapi.get_fd(fd);
	}

	/* Do nothing */
	return FAILURE;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_force_http_10
 */
static int php_runkit_sandbox_sapi_force_http_10()
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		return php_runkit_sandbox_original_sapi.force_http_10();
	}

	/* Do nothing */
	return FAILURE;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_get_target_uid
 */
static int php_runkit_sandbox_sapi_get_target_uid(uid_t *uid)
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		return php_runkit_sandbox_original_sapi.get_target_uid(uid);
	}

	/* Do nothing */
	return FAILURE;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_get_target_gid
 */
static int php_runkit_sandbox_sapi_get_target_gid(uid_t *gid)
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		return php_runkit_sandbox_original_sapi.get_target_gid(gid);
	}

	/* Do nothing */
	return FAILURE;
}
/* }}} */

/* {{{ php_runkit_sandbox_sapi_input_filter
 */
static unsigned int php_runkit_sandbox_sapi_input_filter(int arg, char *var, char **val, unsigned int val_len, unsigned int *new_val_len)
{
	if (!RUNKIT_G(current_sandbox)) {
		/* Not in a sandbox use SAPI's actual handler */
		return php_runkit_sandbox_original_sapi.input_filter(arg, var, val, val_len, new_val_len);
	}

	/* Parrot php_default_input_filter */
	if (new_val_len) {
		*new_val_len = val_len;
	}

	return 1;
}
/* }}} */

/* ********************
   * Output Buffering *
   ******************** */

/* {{{ proto mixed runkit_sandbox_output_handler(Runkit_Sandbox sandbox[, mixed callback])
	Returns the output handler which was active prior to calling this method,
	or false if no output handler is active

	If no callback is passed, the current output handler is not changed
	If a non-true output handler is passed(NULL,false,0,0.0,'',array()), output handling is turned off

	If an error occurs (such as callback is not callable), NULL is returned

	DEPRECATED!  THIS WILL BE REMOVED PRIOR TO THE RELEASE OF RUNKIT VERSION 1.0!!! */
PHP_FUNCTION(runkit_sandbox_output_handler)
{
	zval *sandbox;
	zval *callback = NULL;
	php_runkit_sandbox_object *objval;
	char *name = NULL;
	int callback_is_true = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "O|z", &sandbox, php_runkit_sandbox_class_entry, &callback) == FAILURE) {
		RETURN_NULL();
	}
	php_error_docref(NULL, E_NOTICE, "Use of runkit_sandbox_output_handler() is deprecated.  Use $sandbox['output_handler'] instead.");

	objval = PHP_RUNKIT_SANDBOX_FETCHBOX(sandbox);
	if (!objval->active) {
		php_error_docref(NULL, E_WARNING, "Current sandbox is no longer active");
		RETURN_NULL();
	}

	if (callback) {
		zval callback_copy = *callback;

		zval_copy_ctor(&callback_copy);
		callback_copy.RUNKIT_IS_REF = 0;
		callback_copy.RUNKIT_REFCOUNT = 1;
		callback_is_true = zval_is_true(&callback_copy);
		zval_dtor(&callback_copy);
	}

	if (callback && callback_is_true &&
		!RUNKIT_IS_CALLABLE(callback, IS_CALLABLE_CHECK_NO_ACCESS, &name)) {
		php_error_docref1(NULL, name, E_WARNING, "Second argument (%s) is expected to be a valid callback", name);
		if (name) {
			efree(name);
		}
		RETURN_FALSE;
	}
	if (name) {
		efree(name);
	}

	if (objval->output_handler && return_value_used) {
		*return_value = *objval->output_handler;
		zval_copy_ctor(return_value);
		return_value->RUNKIT_REFCOUNT = 1;
		return_value->RUNKIT_IS_REF = 0;
	} else {
		RETVAL_FALSE;
	}

	if (!callback) {
		return;
	}

	if (objval->output_handler) {
		zval_ptr_dtor(&objval->output_handler);
		objval->output_handler = NULL;
	}

	if (callback && callback_is_true) {
		zval *cb = callback;
		if (callback->RUNKIT_IS_REF) {
			MAKE_STD_ZVAL(cb);
			*cb = *callback;
			zval_copy_ctor(cb);
			cb->RUNKIT_REFCOUNT = 0;
			cb->RUNKIT_IS_REF = 0;
		}
		cb->RUNKIT_REFCOUNT++;
		objval->output_handler = cb;
	}
}
/* }}} */

/* **********************
   * Dimension Handlers *
   ********************** */

#define PHP_RUNKIT_SANDBOX_SETTING_ENTRY(name, get, set)	{ #name, sizeof(#name) - 1, (zval *(*)(php_runkit_sandbox_object *objval)) get, (int (*)(php_runkit_sandbox_object *objval, zval *value)) set },
#define PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RW(name)			{ #name, sizeof(#name) - 1, (zval *(*)(php_runkit_sandbox_object *objval)) php_runkit_sandbox_ ## name ## _getter, (int (*)(php_runkit_sandbox_object *objval, zval *value)) php_runkit_sandbox_ ## name ## _setter },
#define PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RD(name)	{ #name, sizeof(#name) - 1, (zval *(*)(php_runkit_sandbox_object *objval)) php_runkit_sandbox_ ## name ## _getter, NULL },
#define PHP_RUNKIT_SANDBOX_SETTING_ENTRY_WR(name)	{ #name, sizeof(#name) - 1, NULL, (int (*)(php_runkit_sandbox_object *objval, zval *value)) php_runkit_sandbox_ ## name ## _setter },
#define PHP_RUNKIT_SANDBOX_SETTING_SETTER(name)		static void php_runkit_sandbox_ ## name ## _setter(php_runkit_sandbox_object *objval, zval *value)
#define PHP_RUNKIT_SANDBOX_SETTING_GETTER(name)		static zval* php_runkit_sandbox_ ## name ## _getter(php_runkit_sandbox_object *objval)

#define PHP_RUNKIT_SANDBOX_SETTING_BOOL_WR(name) \
PHP_RUNKIT_SANDBOX_SETTING_SETTER(name) \
{ \
	zval copyval = *value; \
\
	zval_copy_ctor(&copyval); \
	convert_to_boolean(&copyval); \
	objval->name = Z_BVAL(copyval) ? 1 : 0; \
}
#define PHP_RUNKIT_SANDBOX_SETTING_BOOL_RD(name) \
PHP_RUNKIT_SANDBOX_SETTING_GETTER(name) \
{ \
	zval *retval; \
\
	ALLOC_ZVAL(retval); \
	Z_TYPE_P(retval) = IS_BOOL; \
	Z_LVAL_P(retval) = objval->name; \
	retval->RUNKIT_REFCOUNT = 0; \
	retval->RUNKIT_IS_REF = 0; \
\
	return retval; \
}
#define PHP_RUNKIT_SANDBOX_SETTING_BOOL_RW(name) \
PHP_RUNKIT_SANDBOX_SETTING_BOOL_WR(name) \
PHP_RUNKIT_SANDBOX_SETTING_BOOL_RD(name)

PHP_RUNKIT_SANDBOX_SETTING_GETTER(output_handler)
{
	if (!objval->output_handler) {
		return EG(uninitialized_zval_ptr);
	}

	return objval->output_handler;
}

PHP_RUNKIT_SANDBOX_SETTING_SETTER(output_handler)
{
	if (!RUNKIT_IS_CALLABLE(value, IS_CALLABLE_CHECK_NO_ACCESS, NULL)) {
		php_error_docref(NULL, E_WARNING, "output_handler is not a valid callback is expected to be a valid callback");
	}

	if (objval->output_handler) {
		zval_ptr_dtor(&objval->output_handler);
	}

	value->RUNKIT_REFCOUNT++;
	objval->output_handler = value;
}

PHP_RUNKIT_SANDBOX_SETTING_GETTER(parent_scope)
{
	zval *retval;

	MAKE_STD_ZVAL(retval);
	if (objval->parent_scope == 0 &&
		objval->parent_scope_name) {
		ZVAL_STRINGL(retval, objval->parent_scope_name, objval->parent_scope_namelen, 1);
	} else {
		ZVAL_LONG(retval, objval->parent_scope);
	}
	retval->RUNKIT_REFCOUNT = 0;

	return retval;
}

PHP_RUNKIT_SANDBOX_SETTING_SETTER(parent_scope)
{
	zval copyval = *value;

	if (Z_TYPE_P(value) == IS_STRING) {
		/* Variable in global symbol_table */
		objval->parent_scope = 0;
		if (objval->parent_scope_name) {
			efree(objval->parent_scope_name);
		}
		objval->parent_scope_name = estrndup(Z_STRVAL_P(value), Z_STRLEN_P(value) + 1);
		objval->parent_scope_namelen = Z_STRLEN_P(value);

		return;
	}

	if (objval->parent_scope_name) {
		efree(objval->parent_scope_name);
		objval->parent_scope_name = NULL;
	}
	objval->parent_scope_namelen = 0;

	zval_copy_ctor(&copyval);
	convert_to_long(&copyval);
	if (Z_LVAL(copyval) < 0) {
		php_error_docref(NULL, E_WARNING, "Invalid scope, assuming 0");
		objval->parent_scope = 0;
		return;
	}

	/* Assumes that such a deep scope *will* exist when a var is resolved
	 * If the scopes don't go that deep, var will be grabbed from the global scoep
	 */
	objval->parent_scope = Z_LVAL(copyval);
}

/* Boolean declarations */
PHP_RUNKIT_SANDBOX_SETTING_BOOL_RD(active)
PHP_RUNKIT_SANDBOX_SETTING_BOOL_RW(parent_access)
PHP_RUNKIT_SANDBOX_SETTING_BOOL_RW(parent_read)
PHP_RUNKIT_SANDBOX_SETTING_BOOL_RW(parent_write)
PHP_RUNKIT_SANDBOX_SETTING_BOOL_RW(parent_eval)
PHP_RUNKIT_SANDBOX_SETTING_BOOL_RW(parent_include)
PHP_RUNKIT_SANDBOX_SETTING_BOOL_RW(parent_echo)
PHP_RUNKIT_SANDBOX_SETTING_BOOL_RW(parent_call)
PHP_RUNKIT_SANDBOX_SETTING_BOOL_RW(parent_die)

struct _php_runkit_sandbox_settings {
	char *name;
	int name_len;
	zval *(*getter)(php_runkit_sandbox_object *objval);
	int (*setter)(php_runkit_sandbox_object *objval, zval *value);
};

struct _php_runkit_sandbox_settings php_runkit_sandbox_settings[] = {
	PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RW(output_handler)
	/* Boolean settings */
	PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RD(active)
	PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RW(parent_access)
	PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RW(parent_read)
	PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RW(parent_write)
	PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RW(parent_eval)
	PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RW(parent_include)
	PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RW(parent_echo)
	PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RW(parent_call)
	PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RW(parent_scope)
	PHP_RUNKIT_SANDBOX_SETTING_ENTRY_RW(parent_die)
	{ NULL , 0, NULL, NULL }
};

static int php_runkit_sandbox_setting_lookup(char *setting, int setting_len) {
	struct _php_runkit_sandbox_settings *s = php_runkit_sandbox_settings;
	int i;

	for(i = 0; s[i].name; i++) {
		if (s[i].name_len == setting_len &&
			memcmp(s[i].name, setting, setting_len) == 0) {
			return i;
		}
	}

	return -1;
}

/* {{{ php_runkit_sandbox_read_dimension
	read_dimension Handler */
static zval *php_runkit_sandbox_read_dimension(zval *object, zval *member, int type)
{
	php_runkit_sandbox_object *objval;
	zval member_copy;
	int setting_id;

	objval = PHP_RUNKIT_SANDBOX_FETCHBOX(object);
	if (!objval) {
		return EG(uninitialized_zval_ptr);
	}

	PHP_RUNKIT_ZVAL_CONVERT_TO_STRING_IF_NEEDED(member, member_copy);

	setting_id = php_runkit_sandbox_setting_lookup(Z_STRVAL_P(member), Z_STRLEN_P(member));

	if (member == &member_copy) {
		zval_dtor(member);
	}

	if (setting_id < 0 || !php_runkit_sandbox_settings[setting_id].getter) {
		/* No such setting */
		return EG(uninitialized_zval_ptr);
	}

	return php_runkit_sandbox_settings[setting_id].getter(objval);
}
/* }}} */

/* {{{ php_runkit_sandbox_write_dimension
	write_dimension Handler */
static void php_runkit_sandbox_write_dimension(zval *object, zval *member, zval *value)
{
	php_runkit_sandbox_object *objval;
	zval member_copy;
	int setting_id;

	objval = PHP_RUNKIT_SANDBOX_FETCHBOX(object);
	if (!objval) {
		return;
	}

	PHP_RUNKIT_ZVAL_CONVERT_TO_STRING_IF_NEEDED(member, member_copy);

	setting_id = php_runkit_sandbox_setting_lookup(Z_STRVAL_P(member), Z_STRLEN_P(member));

	if (member == &member_copy) {
		zval_dtor(member);
	}

	if (setting_id < 0 || !php_runkit_sandbox_settings[setting_id].setter) {
		/* No such setting */
		return;
	}

	php_runkit_sandbox_settings[setting_id].setter(objval, value);
}
/* }}} */

/* {{{ php_runkit_sandbox_has_dimension
	has_dimension Handler */
static int php_runkit_sandbox_has_dimension(zval *object, zval *member, int check_empty)
{
	php_runkit_sandbox_object *objval;
	zval member_copy;
	int setting_id;

	objval = PHP_RUNKIT_SANDBOX_FETCHBOX(object);
	if (!objval) {
		return 0;
	}

	PHP_RUNKIT_ZVAL_CONVERT_TO_STRING_IF_NEEDED(member, member_copy);

	setting_id = php_runkit_sandbox_setting_lookup(Z_STRVAL_P(member), Z_STRLEN_P(member));

	if (member == &member_copy) {
		zval_dtor(member);
	}

	if (setting_id < 0) {
		/* No such setting */
		return 0;
	}

	/* write only offsets are considered non-existent */
	return php_runkit_sandbox_settings[setting_id].getter ? 1 : 0;
}
/* }}} */

/* ********************
   * Class Definition *
   ******************** */

ZEND_BEGIN_ARG_INFO_EX(arginfo_runkit_sandbox__call, 0, 0, 2)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
ZEND_END_ARG_INFO()

static zend_function_entry php_runkit_sandbox_functions[] = {
	PHP_ME(Runkit_Sandbox,		__construct,				NULL,								ZEND_ACC_PUBLIC		| ZEND_ACC_CTOR)
	PHP_ME(Runkit_Sandbox,		__call,						arginfo_runkit_sandbox__call,		ZEND_ACC_PUBLIC)
	/* Language Constructs */
	PHP_ME(Runkit_Sandbox,		eval,						NULL,								ZEND_ACC_PUBLIC)
	PHP_ME(Runkit_Sandbox,		include,					NULL,								ZEND_ACC_PUBLIC)
	PHP_ME(Runkit_Sandbox,		include_once,				NULL,								ZEND_ACC_PUBLIC)
	PHP_ME(Runkit_Sandbox,		require,					NULL,								ZEND_ACC_PUBLIC)
	PHP_ME(Runkit_Sandbox,		require_once,				NULL,								ZEND_ACC_PUBLIC)
	PHP_ME(Runkit_Sandbox,		echo,						NULL,								ZEND_ACC_PUBLIC)
	PHP_ME(Runkit_Sandbox,		print,						NULL,								ZEND_ACC_PUBLIC)
	PHP_ME(Runkit_Sandbox,		die,						NULL,								ZEND_ACC_PUBLIC)
	PHP_MALIAS(Runkit_Sandbox,	exit,	die,				NULL,								ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

static void php_runkit_sandbox_dtor(php_runkit_sandbox_object *objval)
{
	void *prior_context;


	prior_context = tsrm_set_interpreter_context(objval->context);
	{
		if (objval->disable_functions) {
			efree(objval->disable_functions);
		}

		if (objval->disable_classes) {
			efree(objval->disable_classes);
		}

		php_request_shutdown();
	}
	tsrm_set_interpreter_context(NULL);
	tsrm_free_interpreter_context(objval->context);
	tsrm_set_interpreter_context(prior_context);

	if (objval->output_handler) {
		zval_ptr_dtor(&objval->output_handler);
	}
	if (objval->parent_scope_name) {
		efree(objval->parent_scope_name);
	}

	zend_hash_destroy(objval->obj.properties);
	FREE_HASHTABLE(objval->obj.properties);

	efree(objval);
}

static zend_object_value php_runkit_sandbox_ctor(zend_class_entry *ce)
{
	php_runkit_sandbox_object *objval;
	zend_object_value retval;

	objval = ecalloc(1, sizeof(php_runkit_sandbox_object));
	objval->obj.ce = ce;
	ALLOC_HASHTABLE(objval->obj.properties);
	zend_hash_init(objval->obj.properties, 0, NULL, ZVAL_PTR_DTOR, 0);

	retval.handle = zend_objects_store_put(objval, NULL, php_runkit_sandbox_dtor, NULL);
	retval.handlers = &php_runkit_sandbox_object_handlers;

	return retval;
}

#define PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(method)	if (sapi_module.method) { sapi_module.method = php_runkit_sandbox_sapi_##method; }
int php_runkit_init_sandbox(INIT_FUNC_ARGS)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, PHP_RUNKIT_SANDBOX_CLASSNAME, php_runkit_sandbox_functions);
	php_runkit_sandbox_class_entry = zend_register_internal_class(&ce);
	php_runkit_sandbox_class_entry->create_object = php_runkit_sandbox_ctor;

	/* Make a new object handler struct with a couple minor changes */
	memcpy(&php_runkit_sandbox_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_runkit_sandbox_object_handlers.read_property			= php_runkit_sandbox_read_property;
	php_runkit_sandbox_object_handlers.write_property			= php_runkit_sandbox_write_property;
	php_runkit_sandbox_object_handlers.has_property				= php_runkit_sandbox_has_property;
	php_runkit_sandbox_object_handlers.unset_property			= php_runkit_sandbox_unset_property;

	/* Dimension access allow introspection and run-time tweaking */
	php_runkit_sandbox_object_handlers.read_dimension			= php_runkit_sandbox_read_dimension;
	php_runkit_sandbox_object_handlers.write_dimension			= php_runkit_sandbox_write_dimension;
	php_runkit_sandbox_object_handlers.has_dimension			= php_runkit_sandbox_has_dimension;
	php_runkit_sandbox_object_handlers.unset_dimension			= NULL;

	/* ZE has no concept of modifying properties in place via zval** across contexts */
	php_runkit_sandbox_object_handlers.get_property_ptr_ptr		= NULL;

	/* Wedge ourselves in front of the SAPI */
	memcpy(&php_runkit_sandbox_original_sapi,					&sapi_module,				sizeof(sapi_module_struct));
	/* startup -- Not important since it's already fired */
	/* shutdown -- Not important since we'll be out of the way soon enough */
	/* activate -- Okay to run as-is */
	/* deactivate -- Okay to run as-is */
	sapi_module.ub_write										= php_runkit_sandbox_sapi_ub_write;
	sapi_module.flush											= php_runkit_sandbox_sapi_flush;
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(get_stat)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(getenv)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(sapi_error)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(header_handler)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(send_headers)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(send_header)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(read_post)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(read_cookies)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(register_server_variables)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(log_message)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(get_request_time)
	/* php_ini_path_override */
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(block_interruptions)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(unblock_interruptions)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(default_post_reader)
	/* php_ini_ignore */
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(get_fd)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(force_http_10)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(get_target_uid)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(get_target_gid)
	PHP_RUNKIT_SANDBOX_SAPI_OVERRIDE(input_filter)
	/* ini_defaults -- Not important since it's already fired */
	/* phpinfo_as_text */

	return SUCCESS;
}

int php_runkit_shutdown_sandbox(SHUTDOWN_FUNC_ARGS)
{
	/* Give direct control back to the SAPI */
	memcpy(&sapi_module,								&php_runkit_sandbox_original_sapi,	sizeof(sapi_module_struct));

	return SUCCESS;
}
#endif // #if 0

/* ***********************
   * Lint Implementation *
   *********************** */


/* {{{ php_runkit_lint_compile
	Central helper function for runkit_lint() and runkit_lint_file() */
static void php_runkit_lint_compile(INTERNAL_FUNCTION_PARAMETERS, int filemode)
{
	void *context, *prior_context;
	zval *zcode;
	zend_execute_data *old_execute_data;
	zend_execute_data *old_current_execute_data;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &zcode) == FAILURE) {
		RETURN_FALSE;
	}

	old_execute_data = EX(call);
	old_current_execute_data = EG(current_execute_data);
	printf("EX(call)=%llx\n", (long long)old_execute_data);
	if (EX(call)) {
	printf("EX(call)->prev_execute_data=%llx\n", (long long)EX(call)->prev_execute_data);
	}
	printf("EG(current_execute_data)=%llx\n", (long long)old_current_execute_data);
	EG(current_execute_data) = NULL;

	convert_to_string(zcode);
	printf("vm_stack_top=%llx\n", (long long)(EG(vm_stack_top)));
	printf("vm_stack_end=%llx\n", (long long)(EG(vm_stack_end)));
	printf("vm_stack    =%llx\n", (long long)(EG(vm_stack)));
	printf("exception   =%llx\n", (long long)(EG(exception)));
	printf("&exception  =%llx\n", (long long)(&EG(exception)));
	printf("_tsrm_ls_cache=%llx\n", (long long)tsrm_get_ls_cache());
	printf("_tsrm_ls_cache[globals_id-1]=%llx\n", (long long)((*((void ***) tsrm_get_ls_cache()))[executor_globals_id-1]));
	printf("_tsrm_ls_cache[globals_id-1]->exception=%llx\n", (long long)((((zend_executor_globals *) (*((void ***) tsrm_get_ls_cache()))[((executor_globals_id)-1)])->exception)));
	printf("&_tsrm_ls_cache[globals_id-1]->exception=%llx\n", (long long)&((((zend_executor_globals *) (*((void ***) tsrm_get_ls_cache()))[((executor_globals_id)-1)])->exception)));

	context = tsrm_new_interpreter_context();
	prior_context = tsrm_set_interpreter_context(context);
	// {
	// 	php_request_startup();
	// 	PG(during_request_startup) = 0;  // TODO find reason for workaround
	// 	printf("######## Switched to another stack\n");
	// 	printf("vm_stack_top=%llx\n", (long long)(EG(vm_stack_top))); fflush(stdout);
	// 	printf("vm_stack_end=%llx\n", (long long)(EG(vm_stack_end))); fflush(stdout);
	// 	printf("vm_stack    =%llx\n", (long long)(EG(vm_stack))); fflush(stdout);
	// 	printf("exception   =%llx\n", (long long)(EG(exception))); fflush(stdout);
	// 	printf("&exception  =%llx\n", (long long)(&EG(exception))); fflush(stdout);
	// 	printf("_tsrm_ls_cache=%llx\n", (long long)tsrm_get_ls_cache());
	// 	printf("_tsrm_ls_cache[globals_id-1]=%llx\n", (long long)((*((void ***) tsrm_get_ls_cache()))[executor_globals_id-1]));
	// 	printf("_tsrm_ls_cache[globals_id-1]->exception=%llx\n", (long long)((((zend_executor_globals *) (*((void ***) tsrm_get_ls_cache()))[((executor_globals_id)-1)])->exception)));
	// 	printf("&_tsrm_ls_cache[globals_id-1]->exception=%llx\n", (long long)&((((zend_executor_globals *) (*((void ***) tsrm_get_ls_cache()))[((executor_globals_id)-1)])->exception)));
	// 	printf("######## end stack info\n");

	// 	zend_first_try {
	// 		char *eval_desc;
	// 		zend_op_array *op_array;
	// 		zend_object *exception = EG(exception);
	// 		printf("before try\nexception    =%llx\n", (long long)(exception));
	// 		printf("msg    ='''%s'''\n", ZSTR_VAL(Z_STR(*zcode)));

	// 		if (filemode) {
	// 			op_array = compile_filename(ZEND_INCLUDE, zcode);
	// 		} else {
	// 			// TODO: prepend `?>` to emulate the old runkit behavior.
	// 			eval_desc = zend_make_compiled_string_description("runkit_lint test compile");
	// 			op_array = compile_string(zcode, eval_desc);
	// 			exception = EG(exception);
	// 			if (exception) {
	// 				zval tmp_obj;
	// 				printf("after try\nexception    =%llx\n", (long long)(exception));
	// 				ZVAL_OBJ(&tmp_obj, exception);
	// 				php_var_dump(&tmp_obj, 1);
	// 				printf("In zend_catch, freeing object\n");
	// 				fflush(stdout);
	// 				EG(exception) = 0;
	// 				zend_object_release(exception);
	// 			}
	// 			efree(eval_desc);
	// 		}

	// 		if (op_array)  {
	// 			RETVAL_TRUE;
	// 			destroy_op_array(op_array);
	// 			efree(op_array);
	// 		} else {
	// 			RETVAL_FALSE;
	// 		}
	// 	} zend_catch {
	// 		// TODO: is this right?
	// 		// TODO: free the object?
	// 		RETVAL_FALSE;
	// 	} zend_end_try();

	// 	// TODO: catch the exception, destroy it before calling shutdown?
	// 	php_request_shutdown(NULL);
	// }

	// printf("After exception\n");
	// printf("vm_stack_top=%llx\n", (long long)(EG(vm_stack_top)));
	// printf("vm_stack_end=%llx\n", (long long)(EG(vm_stack_end)));
	// printf("vm_stack    =%llx\n", (long long)(EG(vm_stack)));
	// printf("exception   =%llx\n", (long long)(EG(exception)));
	// printf("&exception  =%llx\n", (long long)(&EG(exception)));
	// fflush(stdout);
	tsrm_set_interpreter_context(NULL);
	tsrm_free_interpreter_context(context);
	tsrm_set_interpreter_context(prior_context);
	// TODO: Mock the sapi as well? I'm not familiar with thread safe code
	printf("after\n");
	printf("vm_stack_top=%llx\n", (long long)(EG(vm_stack_top)));
	printf("vm_stack_end=%llx\n", (long long)(EG(vm_stack_end)));
	printf("vm_stack    =%llx\n", (long long)(EG(vm_stack)));
	printf("exception    =%llx\n", (long long)(EG(exception)));
	printf("&exception  =%llx\n", (long long)(&EG(exception)));
	// Should this invalidate it?
	printf("_tsrm_ls_cache=%llx\n", (long long)tsrm_get_ls_cache());
	printf("_tsrm_ls_cache[globals_id-1]=%llx\n", (long long)((*((void ***) tsrm_get_ls_cache()))[executor_globals_id-1]));
	printf("_tsrm_ls_cache[globals_id-1]->exception=%llx\n", (long long)((((zend_executor_globals *) (*((void ***) tsrm_get_ls_cache()))[((executor_globals_id)-1)])->exception)));
	printf("&_tsrm_ls_cache[globals_id-1]->exception=%llx\n", (long long)&((((zend_executor_globals *) (*((void ***) tsrm_get_ls_cache()))[((executor_globals_id)-1)])->exception)));
	fflush(stdout);

	EX(call) = old_execute_data;
	EG(current_execute_data) = old_current_execute_data;
	printf("EX(call)=%llx\n", (long long)EX(call));
	if (EX(call)) {
	printf("EX(call)->prev_execute_data=%llx\n", (long long)EX(call)->prev_execute_data);
	}
	printf("EG(current_execute_data)=%llx\n", (long long)EG(current_execute_data));
	printf("EG(vm_stack_top)=%llx\n", (long long)EG(vm_stack_top));
	printf("&exception  =%llx\n", (long long)(&EG(exception)));
	printf("exception  =%llx\n", (long long)(EG(exception)));
}
/* }}} */

// The implementation of compile_string changed. It expects raw PHP without tags instead of PHP contents of tags.
// zend_language_scanner.c doesn't expose enough to create a stable implementation, unless one were to duplicate it.

/* {{{ proto bool runkit_lint(string code)
	Attempts to compile a string of code within a sub-interpreter */
PHP_FUNCTION(runkit_lint)
{
	php_runkit_lint_compile(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto bool runkit_lint_file(string filename)
	Attempts to compile a file within a sub-interpreter */
PHP_FUNCTION(runkit_lint_file)
{
	php_runkit_lint_compile(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

#endif /* PHP_RUNKIT_SANDBOX */

