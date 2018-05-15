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
  | Modified for php7 "runkit7" by Tyson Andre<tysonandre775@hotmail.com>|
  +----------------------------------------------------------------------+
*/

#ifndef RUNKIT_H
#define RUNKIT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "Zend/zend_closures.h"

#include "Zend/zend_interfaces.h"

#if PHP_WIN32
# include "win32/php_stdint.h"
# if defined(_MSC_VER) && _MSC_VER >= 1800
#  include <stdbool.h>
# else
#  ifndef inline
#  define inline __inline
#  endif

#  ifndef __cplusplus
#   if !0
typedef enum { false = 0, true = 1 } _Bool;
#  define bool _Bool
#   endif
#  else
typedef bool _Bool;
#  define bool _Bool
#  endif

#  define false 0
#  define true 1
#  define __bool_true_false_are_defined 1
# endif /* __MSC_VER */
# ifdef _DEBUG
#  include <crtdbg.h>
# endif
#endif /* PHP_WIN32 */

#ifdef __GNUC__
// Make it easy to verify that format strings are correct in recent versions of gcc.
extern PHPAPI ZEND_COLD void php_error_docref(const char *docref, int type, const char *format, ...)
	__attribute__((format (printf, 3, 4)));
#endif
#ifdef DEBUGGING
#define debug_printf(...) printf(__VA_ARGS__)
static inline void* _debug_emalloc(void* data, int bytes, char* file, int line) {
	debug_printf("%llx: Wrapped emalloc %d bytes, %s:%d\n", (long long) data, bytes, file, line);
	return data;
}
#undef emalloc
#define emalloc(size) _debug_emalloc(_emalloc((size) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC), (size), __FILE__, __LINE__)
#define zend_string_release(s) do{ debug_printf("%llx: Pretending to zend_string_release %s\n", (long long) (s), ZSTR_VAL((s)));} while(0)

#undef efree
#undef pefree
#define efree(m) do {debug_printf("%llx: efree at %s:%d\n", (long long) (m), __FILE__, __LINE__); _efree((m) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC); } while(0)
#define pefree(m, persistent) do {if ((persistent)) {debug_printf("%llx: (pe)free at %s:%d\n", (long long) (m), __FILE__, __LINE__); free((m));} else { efree(m); }} while(0)
#else
#define debug_printf(...) do { } while(0)
#endif

#define PHP_RUNKIT_VERSION					"1.0.5b1"
#define PHP_RUNKIT_SANDBOX_CLASSNAME		"Runkit_Sandbox"
#define PHP_RUNKIT_SANDBOX_PARENT_CLASSNAME	"Runkit_Sandbox_Parent"

#define PHP_RUNKIT_IMPORT_FUNCTIONS                         0x0001
#define PHP_RUNKIT_IMPORT_CLASS_METHODS                     0x0002
#define PHP_RUNKIT_IMPORT_CLASS_CONSTS                      0x0004
#define PHP_RUNKIT_IMPORT_CLASS_PROPS                       0x0008
#define PHP_RUNKIT_IMPORT_CLASS_STATIC_PROPS                0x0010
#define PHP_RUNKIT_IMPORT_CLASSES                           (PHP_RUNKIT_IMPORT_CLASS_METHODS|PHP_RUNKIT_IMPORT_CLASS_CONSTS|\
                                                             PHP_RUNKIT_IMPORT_CLASS_PROPS|PHP_RUNKIT_IMPORT_CLASS_STATIC_PROPS)
#define PHP_RUNKIT_IMPORT_OVERRIDE                          0x0020
#define PHP_RUNKIT_OVERRIDE_OBJECTS                         0x8000

/* Hardcoded. TODO should not be. */
#define PHP_RUNKIT_SUPERGLOBALS

#ifdef PHP_RUNKIT_SPL_OBJECT_ID
#if PHP_VERSION_ID < 70200
#define PHP_RUNKIT_PROVIDES_SPL_OBJECT_ID
#endif
#endif

/* The TSRM interpreter patch required by runkit_sandbox was added in 5.1, but this package includes diffs for older versions
 * Those diffs include an additional #define to indicate that they've been applied
 */
#ifdef PHP_RUNKIT_FEATURE_SANDBOX
#if (defined ZTS) && (defined PHP_RUNKIT_FEATURE_SANDBOX)
#define PHP_RUNKIT_SANDBOX
#else
// debugging code
#error Feature not enabled
#endif
#endif

#ifdef PHP_RUNKIT_FEATURE_MODIFY
#define PHP_RUNKIT_MANIPULATION
// TODO: Enable these macros once the corresponding functions/files compile and pass some of the tests.
// TODO: Clean up these macros once the corresponding functions/files are 100% correct.
#define PHP_RUNKIT_MANIPULATION_IMPORT
#define PHP_RUNKIT_MANIPULATION_PROPERTIES
#define PHP_RUNKIT_MANIPULATION_CLASSES
#else
#error DEBUG - RUNKIT_FEATURE_MODIFY DISABLED
#endif

#ifdef PHP_RUNKIT_MANIPULATION
#include "Zend/zend_object_handlers.h"
#endif

#if PHP_VERSION_ID >= 70100
#define RUNKIT_CONST_FLAGS_DC(access_type) , zend_long access_type
#define RUNKIT_CONST_FLAGS_CC(access_type) , access_type
#define RUNKIT_CONST_FETCH(access_type) access_type
#else
#define RUNKIT_CONST_FLAGS_DC(access_type)
#define RUNKIT_CONST_FLAGS_CC(access_type)
#define RUNKIT_CONST_FETCH(access_type) ZEND_ACC_PUBLIC
#endif

PHP_MINIT_FUNCTION(runkit);
PHP_MSHUTDOWN_FUNCTION(runkit);
PHP_RINIT_FUNCTION(runkit);
PHP_RSHUTDOWN_FUNCTION(runkit);
PHP_MINFO_FUNCTION(runkit);

PHP_FUNCTION(runkit_object_id);
#ifdef PHP_RUNKIT_PROVIDES_SPL_OBJECT_ID
PHP_FUNCTION(spl_object_id);
#endif

#ifdef PHP_RUNKIT_MANIPULATION
PHP_FUNCTION(runkit_function_add);
PHP_FUNCTION(runkit_function_remove);
PHP_FUNCTION(runkit_function_rename);
PHP_FUNCTION(runkit_function_redefine);
PHP_FUNCTION(runkit_function_copy);
PHP_FUNCTION(runkit_method_add);
PHP_FUNCTION(runkit_method_redefine);
PHP_FUNCTION(runkit_method_remove);
PHP_FUNCTION(runkit_method_rename);
PHP_FUNCTION(runkit_method_copy);
PHP_FUNCTION(runkit_constant_redefine);
PHP_FUNCTION(runkit_constant_remove);
PHP_FUNCTION(runkit_constant_add);
// 1. The property manipulation code still has bugs, and the "offset" used is in bytes as of php7, but still treated as an index in this code.
// 2. As of php7's new zval layout, The only way to "add" a default property would be to realloc() every single one
//    of the zend_objects that are instances of that class (to make room for another property).
//    This would break php internals and possibly extensions.
//    A possible other way way would be to change the API to "runkit_default_property_modify($className, $propertyName, $value, $flags = TODO)"
//    (with a precondition $propertyName already existed)
// 3. It should be possible to meet many uses by modifying constructors with runkit_method_move and runkit_method_add,
//    or using ReflectionProperty for fetching.
//    https://secure.php.net/manual/en/reflectionproperty.setaccessible.php (sets accessibility only for ReflectionProperty)
//    https://secure.php.net/manual/en/reflectionproperty.setvalue.php
//    https://secure.php.net/manual/en/reflectionproperty.getvalue.php
//
// PHP_FUNCTION(runkit_default_property_add);
// PHP_FUNCTION(runkit_default_property_remove);
#ifdef PHP_RUNKIT_MANIPULATION_CLASSES
// PHP_FUNCTION(runkit_class_emancipate);
// PHP_FUNCTION(runkit_class_adopt);
#endif
// PHP_FUNCTION(runkit_import);
#ifdef PHP_RUNKIT_MANIPULATION_PROPERTIES
PHP_FUNCTION(runkit_default_property_add);
PHP_FUNCTION(runkit_default_property_remove);
#endif
// TODO
#ifdef PHP_RUNKIT_MANIPULATION_IMPORT
PHP_FUNCTION(runkit_import);
#endif /* PHP_RUNKIT_MANIPULATION_IMPORT */
#endif /* PHP_RUNKIT_MANIPULATION */

#ifdef PHP_RUNKIT_SANDBOX
PHP_FUNCTION(runkit_sandbox_output_handler);
PHP_FUNCTION(runkit_lint);
PHP_FUNCTION(runkit_lint_file);

typedef struct _php_runkit_sandbox_object php_runkit_sandbox_object;
#endif /* PHP_RUNKIT_SANDBOX */

#ifdef PHP_RUNKIT_MANIPULATION
// typedef struct _php_runkit_default_class_members_list_element php_runkit_default_class_members_list_element;
#endif

#if defined(PHP_RUNKIT_SUPERGLOBALS) || defined(PHP_RUNKIT_SANDBOX) || defined(PHP_RUNKIT_MANIPULATION)
ZEND_BEGIN_MODULE_GLOBALS(runkit)
#ifdef PHP_RUNKIT_SUPERGLOBALS
	HashTable *superglobals;
#endif
#ifdef PHP_RUNKIT_SANDBOX
	php_runkit_sandbox_object *current_sandbox;
#endif
#ifdef PHP_RUNKIT_MANIPULATION
	HashTable *misplaced_internal_functions;
	HashTable *replaced_internal_functions;
	// php_runkit_default_class_members_list_element *removed_default_class_members;
	zend_bool internal_override;
	const char *name_str, *removed_method_str, *removed_function_str, *removed_parameter_str;
	zend_function *removed_function, *removed_method;
	zend_bool module_moved_to_front;
#endif
ZEND_END_MODULE_GLOBALS(runkit)
#endif

extern ZEND_DECLARE_MODULE_GLOBALS(runkit)

#ifdef ZTS
#define		RUNKIT_G(v)		TSRMG(runkit_globals_id, zend_runkit_globals *, v)
#else
#define		RUNKIT_G(v)		(runkit_globals.v)
#endif

#define RUNKIT_IS_CALLABLE(cb_zv, flags, cb_sp) zend_is_callable((cb_zv), (flags), (cb_sp))
#define RUNKIT_FILE_HANDLE_DTOR(pHandle)        zend_file_handle_dtor((pHandle))

#ifdef ZEND_ACC_RETURN_REFERENCE
#     define PHP_RUNKIT_ACC_RETURN_REFERENCE         ZEND_ACC_RETURN_REFERENCE
#else
#     define PHP_RUNKIT_ACC_RETURN_REFERENCE         0x4000000
#endif

#ifndef ALLOC_PERMANENT_ZVAL
# define ALLOC_PERMANENT_ZVAL(z) \
    (z) = (zval*)malloc(sizeof(zval))
#endif

#ifdef PHP_RUNKIT_MANIPULATION
#if !defined(zend_hash_add_or_update)
/* Why doesn't ZE2 define this? */
#define zend_hash_add_or_update(ht, key, data, mode) \
        _zend_hash_add_or_update((ht), (key), (data), (mode) ZEND_FILE_LINE_CC)
#endif
// Similar to zend_hash.h zend_hash_add_ptr
static inline void *runkit_zend_hash_add_or_update_ptr(HashTable *ht, zend_string *key, void *pData, uint32_t flag) {
	zval tmp, *zv;
	ZVAL_PTR(&tmp, pData);
	zv = zend_hash_add_or_update(ht, key, &tmp, flag);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

#ifndef Z_ADDREF_P
#     define Z_ADDREF_P(x)                           ZVAL_ADDREF(x)
#endif

#ifndef IS_CONSTANT_AST
#define IS_CONSTANT_AST IS_CONSTANT_ARRAY
#endif

/* runkit_functions.c */
#define RUNKIT_TEMP_FUNCNAME  "__runkit_temporary_function__"
int php_runkit_check_call_stack(zend_op_array *op_array);
void php_runkit_clear_all_functions_runtime_cache();
void php_runkit_fix_all_hardcoded_stack_sizes(zend_string *called_name_lower, zend_function *called_f);

void php_runkit_remove_function_from_reflection_objects(zend_function *fe);
// void php_runkit_function_copy_ctor(zend_function *fe, zend_string *newname, char orig_fe_type);
zend_function* php_runkit_function_clone(zend_function *fe, zend_string *newname, char orig_fe_type);
void php_runkit_function_dtor(zend_function *fe);
int php_runkit_remove_from_function_table(HashTable *function_table, zend_string *func_lower);
void* php_runkit_update_function_table(HashTable *function_table, zend_string *func_lower, zend_function *f);
int php_runkit_generate_lambda_method(const zend_string *arguments, const zend_string *return_type, const zend_bool is_strict, const zend_string *phpcode,
                                      zend_function **pfe, zend_bool return_ref, zend_bool is_static);
int php_runkit_generate_lambda_function(const zend_string *arguments, const zend_string *return_type, const zend_bool is_strict, const zend_string *phpcode,
                                      zend_function **pfe, zend_bool return_ref);
int php_runkit_cleanup_lambda_method();
int php_runkit_cleanup_lambda_function();
int php_runkit_destroy_misplaced_functions(zval *pDest);
void php_runkit_restore_internal_function(zend_string *fname_lower, zend_function *f);

/* runkit_methods.c */
zend_class_entry *php_runkit_fetch_class(zend_string* classname);
zend_class_entry *php_runkit_fetch_class_int(zend_string *classname);
void php_runkit_clean_children_methods(zend_class_entry *ce, zend_class_entry *ancestor_class, zend_class_entry *parent_class, zend_string *fname_lower, zend_function *orig_cfe);
void php_runkit_clean_children_methods_foreach(HashTable *ht, zend_class_entry *ancestor_class, zend_class_entry *parent_class, zend_string *fname_lower, zend_function *orig_cfe);
void php_runkit_update_children_methods(zend_class_entry *ce, zend_class_entry *ancestor_class, zend_class_entry *parent_class, zend_function *fe, zend_string *fname_lower, zend_function *orig_fe);
void php_runkit_update_children_methods_foreach(HashTable *ht, zend_class_entry *ancestor_class, zend_class_entry *parent_class, zend_function *fe, zend_string *fname_lower, zend_function *orig_fe);
int php_runkit_fetch_interface(zend_string *classname, zend_class_entry **pce);

/* Redundant unless 7.1 changes - string may no longer apply */
#define PHP_RUNKIT_FUNCTION_ADD_REF(f)	function_add_ref(f)
#define php_runkit_locate_scope(ce, fe, methodname_lower)   fe->common.scope
#define PHP_RUNKIT_STRTOLOWER(param)			php_u_strtolower(param, &param##_len, UG(default_locale))
#define PHP_RUNKIT_STRING_LEN(param,addtl)		(param##_type == IS_UNICODE ? UBYTES(param##_len + (addtl)) : (param##_len + (addtl)))
#define PHP_RUNKIT_STRING_TYPE(param)			(param##_type)
#define RUNKIT_HASH_FIND(hash,param,ppvar)		zend_u_hash_find(hash, param##_type, (UChar *)param, param##_len + 1, (void*)ppvar)
#define RUNKIT_HASH_EXISTS(hash,param)		zend_u_hash_exists(hash, param##_type, (UChar *)param, param##_len + 1)

#define PHP_RUNKIT_NOT_ENOUGH_MEMORY_ERROR php_error_docref(NULL, E_ERROR, "Not enough memory")

/* runkit_constants.c */
void php_runkit_update_children_consts(zend_class_entry *ce, zend_class_entry *parent_class, zval *c, zend_string *cname RUNKIT_CONST_FLAGS_DC(access_type));
void php_runkit_update_children_consts_foreach(HashTable *ht, zend_class_entry *parent_class, zval *c, zend_string *cname RUNKIT_CONST_FLAGS_DC(access_type));

/* runkit_classes.c */
int php_runkit_class_copy(zend_class_entry *src, zend_string *classname);

/* runkit_props.c */
void php_runkit_update_children_def_props(zend_class_entry *ce, zend_class_entry *parent_class, zval *p, zend_string *pname, int access_type, zend_class_entry *definer_class, int override, int override_in_objects);
int php_runkit_def_prop_add_int(zend_class_entry *ce, zend_string* propname, zval *copyval, long visibility,
                                zend_string* doc_comment, zend_class_entry *definer_class, int override,
                                int override_in_objects);
int php_runkit_def_prop_remove_int(zend_class_entry *ce, zend_string* propname, zend_class_entry *definer_class,
                                   zend_bool was_static, zend_bool remove_from_objects, zend_property_info *parent_property);

typedef struct _zend_closure {
    zend_object    std;
    zend_function  func;
    HashTable     *debug_info;
} zend_closure;

typedef struct _parsed_return_type {
	zend_string *return_type;
	zend_bool   valid;
} parsed_return_type;

typedef struct _parsed_is_strict {
	zend_bool   overridden;
	zend_bool   is_strict;
	zend_bool   valid;
} parsed_is_strict;

/* Disabled because of changes in the way properties are handled */
/*
struct _php_runkit_default_class_members_list_element {
    zend_class_entry* ce;
    zend_bool is_static;
    int offset;
    php_runkit_default_class_members_list_element *next;
};
*/

/* {{{ php_runkit_default_class_members_list_add */
/*
static inline void php_runkit_default_class_members_list_add(php_runkit_default_class_members_list_element **head,
                                                             zend_class_entry* ce, zend_bool is_static,
                                                             int offset) {
	php_runkit_default_class_members_list_element *new_el = emalloc(sizeof(php_runkit_default_class_members_list_element));
	if (new_el) {
		new_el->ce = ce;
		new_el->is_static = is_static;
		new_el->offset = offset;
		new_el->next = *head;
		*head = new_el;
	}
}
*/
/* }}} */

/* {{{ php_runkit_modify_function_doc_comment */
/** Replace the doc comment of the copied/created function with that of the original */
static inline void php_runkit_modify_function_doc_comment(zend_function *fe, zend_string* doc_comment) {
	if (fe->type == ZEND_USER_FUNCTION) {
		if (doc_comment) {
			// TODO: Fix memory leak warnings related to doc comments for created/renamed functions.
			zend_string *tmp = fe->op_array.doc_comment;
			zend_string_addref(doc_comment);
			fe->op_array.doc_comment = doc_comment;
			if (tmp) {
				zend_string_delref(tmp);
			}
		}
	}
}
/* }}} */

/* This macro iterates through all instances of objects. */
#define PHP_RUNKIT_ITERATE_THROUGH_OBJECTS_STORE_BEGIN(i) { \
	if (EG(objects_store).object_buckets) { \
		for (i = 1; i < EG(objects_store).top ; i++) { \
			if (EG(objects_store).object_buckets[i] && \
			   IS_OBJ_VALID(EG(objects_store).object_buckets[i]) && (!(GC_FLAGS(EG(objects_store).object_buckets[i]) & IS_OBJ_DESTRUCTOR_CALLED))) { \
				zend_object *object; \
				object = EG(objects_store).object_buckets[i];


#define PHP_RUNKIT_ITERATE_THROUGH_OBJECTS_STORE_END \
			} \
		}\
	} \
}

#define PHP_RUNKIT_FREE_INTERNAL_FUNCTION_NAME(fe) \
	if ((fe)->type == ZEND_INTERNAL_FUNCTION && (fe)->internal_function.function_name) { \
		zend_string_release((fe)->internal_function.function_name); \
		(fe)->internal_function.function_name = NULL; \
	}

#endif /* PHP_RUNKIT_MANIPULATION */

#ifdef PHP_RUNKIT_SANDBOX
/* runkit_sandbox.c */
int php_runkit_init_sandbox(INIT_FUNC_ARGS);
int php_runkit_shutdown_sandbox(SHUTDOWN_FUNC_ARGS);

/* runkit_sandbox_parent.c */
int php_runkit_init_sandbox_parent(INIT_FUNC_ARGS);
int php_runkit_shutdown_sandbox_parent(SHUTDOWN_FUNC_ARGS);
int php_runkit_sandbox_array_deep_copy(zval **value, int num_args, va_list args, zend_hash_key *hash_key);

struct _php_runkit_sandbox_object {
	zend_object obj;

	void *context, *parent_context;

	char *disable_functions;
	char *disable_classes;
	zval *output_handler;					/* points to function which lives in the parent_context */

	unsigned char bailed_out_in_eval;		/* Patricide is an ugly thing.  Especially when it leaves bailout address mis-set */

	unsigned char active;					/* A bailout will set this to 0 */
	unsigned char parent_access;			/* May Runkit_Sandbox_Parent be instantiated/used? */
	unsigned char parent_read;				/* May parent vars be read? */
	unsigned char parent_write;				/* May parent vars be written to? */
	unsigned char parent_eval;				/* May arbitrary code be run in the parent? */
	unsigned char parent_include;			/* May arbitrary code be included in the parent? (includes require(), and *_once()) */
	unsigned char parent_echo;				/* May content be echoed from the parent scope? */
	unsigned char parent_call;				/* May functions in the parent scope be called? */
	unsigned char parent_die;				/* Are $PARENT->die() / $PARENT->exit() enabled? */
	unsigned long parent_scope;				/* 0 == Global, 1 == Active, 2 == Active->prior, 3 == Active->prior->prior, etc... */

	char *parent_scope_name;				/* Combines with parent_scope to refer to a named array as a symbol table */
	int parent_scope_namelen;
};


/* TODO: It'd be nice if objects and resources could make it across... */
#define PHP_SANDBOX_CROSS_SCOPE_ZVAL_COPY_CTOR(pzv) \
{ \
	switch (Z_TYPE_P(pzv)) { \
		case IS_RESOURCE: \
		case IS_OBJECT: \
			php_error_docref(NULL, E_WARNING, "Unable to translate resource, or object variable to current context."); \
			ZVAL_NULL(pzv); \
			break; \
		case IS_ARRAY: \
		{ \
			HashTable *original_hashtable = Z_ARRVAL_P(pzv); \
			array_init(pzv); \
			zend_hash_apply_with_arguments(original_hashtable, (apply_func_args_t)php_runkit_sandbox_array_deep_copy, 1, Z_ARRVAL_P(pzv)); \
			break; \
		} \
		default: \
			zval_copy_ctor(pzv); \
	} \
	if (Z_REFCOUNTED_P(pzf)) \
		Z_SET_REFCOUNT(pzv, 1); \
		/*(pzv)->RUNKIT_IS_REF = 0; // I think I can get rid of that, since IS_REFERENCE is now part of Z_TYPE?*/ \
	} \
}
#endif /* PHP_RUNKIT_SANDBOX */

#ifdef PHP_RUNKIT_MANIPULATION

// Split pnname into classname and pnname, if it contains the string "::"
// FIXME: Need to correctly do reference tracking for the original and created strings.
#define PHP_RUNKIT_SPLIT_PN(classname, pnname) { \
	char *colon; \
	if (ZSTR_LEN(pnname) > 3 && (colon = memchr(ZSTR_VAL(pnname), ':', ZSTR_LEN(pnname) - 2)) && (colon[1] == ':')) { \
		classname = zend_string_init(ZSTR_VAL(pnname), colon - ZSTR_VAL(pnname), 0); \
		pnname = zend_string_init(colon + 2, ZSTR_LEN(pnname) - (ZSTR_LEN(classname) + 2), 0); \
	} else { \
		classname = NULL; \
	} \
}

// If the input string was split into two allocated zend_strings, then decrement the refcount of both of those strings.
#define PHP_RUNKIT_SPLIT_PN_CLEANUP(classname, pnname) \
	if (classname != NULL) { \
		zend_string_release(classname); \
		zend_string_release(constname); \
	}

// If lcmname is one of the magic method names(e.g. __get, __construct), then override the magic method function entry for the class entry ce (And its subclasses)
void PHP_RUNKIT_ADD_MAGIC_METHOD(zend_class_entry *ce, zend_string* lcmname, zend_function *fe, const zend_function *orig_fe);

// If lcmname is one of the magic method names(e.g. __get, __construct), and being removed,
// then override the magic method function entry for the class entry ce (And its subclasses)
void PHP_RUNKIT_DEL_MAGIC_METHOD(zend_class_entry *ce, const zend_function *fe);

// Sets type flags for ce and its subclasses' class entries so that the VM knows to check for magic methods.
void ensure_all_objects_of_class_have_magic_methods(zend_class_entry *ce);

/* {{{ php_runkit_parse_doc_comment_arg */
/* Validate that the provided doc comment is a string or null */
inline static zend_string* php_runkit_parse_doc_comment_arg(int argc, zval *args, int arg_pos) {
	if (argc > arg_pos) {
		if (Z_TYPE(args[arg_pos]) == IS_STRING) {
			/* Return doc comment without increasing reference count. */
			return Z_STR(args[arg_pos]);
		} else if (Z_TYPE(args[arg_pos]) != IS_NULL) {
			php_error_docref(NULL, E_WARNING, "Doc comment should be a string or NULL");
		}
	}
	return NULL;
}
/* }}} */

/* {{{ php_runkit_is_valid_return_type */
inline static zend_bool php_runkit_is_valid_return_type(const zend_string *return_type) {
	const char *it = ZSTR_VAL(return_type);
	const char * const end = it + ZSTR_LEN(return_type);
#if PHP_VERSION_ID >= 70100
	if (it >= end) {
		return 0;
	}
	if (*it == '?') {
		it++;
	}
#endif
	if (it >= end) {
		return 0;
	}
	if (*it == '\\') {
		it++;
	}
	if (it >= end) {
		return 0;
	}
	// The format of a valid class identifier is documented at https://secure.php.net/manual/en/language.oop5.basic.php
	while (1) {
		unsigned char c = *it;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 128) {
			for (++it; it < end; ++it) {
				c = *it;
				if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 128) {
					continue;
				}
				if (c == '\\') {
					break;
				}
				return 0;
			}
			if (c == '\\') {
				if (it + 1 == end) {  // "\A\" is invalid
					return 0;
				}
				++it;
				continue;
			}
			return 1;
		}
		return 0;
	}
}
/* }}} */

/* {{{ php_runkit_parse_return_type_arg */
inline static parsed_return_type php_runkit_parse_return_type_arg(int argc, zval *args, int arg_pos) {
	parsed_return_type retval;
	retval.return_type = NULL;
	retval.valid = 1;
	if (argc <= arg_pos) {
		return retval;
	}
	if (Z_TYPE(args[arg_pos]) == IS_STRING) {
		// Return return type without increasing reference count.
		zend_string *return_type = Z_STR(args[arg_pos]);
		if (ZSTR_LEN(return_type) == 0) {
			// NOTE: "" and NULL may mean different things in the future - "" could mean remove the return type (if one exists).
			return retval;
		}
		if (php_runkit_is_valid_return_type(return_type)) {
			retval.return_type = return_type;
			return retval;
		}
#if PHP_VERSION_ID >= 70100
		php_error_docref(NULL, E_WARNING, "Return type should match regex ^\\??[a-zA-Z_\\x7f-\\xff][a-zA-Z0-9_\\x7f-\\xff]*(\\\\[a-zA-Z_\\x7f-\\xff][a-zA-Z0-9_\\x7f-\\xff]*)*$");
#else
		php_error_docref(NULL, E_WARNING, "Return type should match regex ^[a-zA-Z_\\x7f-\\xff][a-zA-Z0-9_\\x7f-\\xff]*(\\\\[a-zA-Z_\\x7f-\\xff][a-zA-Z0-9_\\x7f-\\xff]*)$");
#endif
		retval.valid = 0;
		return retval;
	} else if (Z_TYPE(args[arg_pos]) != IS_NULL) {
		php_error_docref(NULL, E_WARNING, "Return type should be a string or NULL");
		retval.valid = 0;
	}
	return retval;
}
/* }}} */

/* {{{ php_runkit_parse_return_type_arg */
inline static parsed_is_strict php_runkit_parse_is_strict_arg(int argc, zval *args, int arg_pos) {
	parsed_is_strict retval;
	retval.is_strict = 0;
	retval.overridden = 0;
	retval.valid = 1;
	if (argc <= arg_pos) {
		return retval;
	}
	if (Z_TYPE(args[arg_pos]) == IS_TRUE || Z_TYPE(args[arg_pos]) == IS_FALSE) {
		// Return return type without increasing reference count.
		retval.is_strict = Z_TYPE(args[arg_pos]) == IS_TRUE;
		retval.overridden = 1;
		return retval;
	} else if (Z_TYPE(args[arg_pos]) != IS_NULL) {
		php_error_docref(NULL, E_WARNING, "is_strict should be a boolean or NULL");
		retval.valid = 0;
	}
	return retval;
}

/* {{{ php_runkit_parse_args_to_zvals */
inline static zend_bool php_runkit_parse_args_to_zvals(int argc, zval **pargs) {
	*pargs = (zval *) emalloc(argc * sizeof(zval));
	if (*pargs == NULL) {
		PHP_RUNKIT_NOT_ENOUGH_MEMORY_ERROR;
		return 0;
	}
	if (zend_get_parameters_array_ex(argc, *pargs) == FAILURE) {
		php_error_docref(NULL, E_ERROR, "Internal error occured while parsing arguments");
		efree(*pargs);
		return 0;
	}
	return 1;
}
/* }}} */

#define PHP_RUNKIT_BODY_ERROR_MSG "%s's body should be either a closure or two strings"

/** Create an interned string. Useful for ensuring that method names, etc.
    won't be freed on request shutdown before runkit has a chance to free it. */
/* NOTE: In the case of internal overrides and fpm, we want the "permanent" string (the string originally associated
 * with an internal function or method), which won't be garbage collected. */
inline static zend_string* zend_string_to_interned(zend_string* original) {
	zend_string *interned_name = zend_new_interned_string(original);
	if (interned_name != original) {
		zend_string_release(original);
		return interned_name;
	}
	return original;
}

/* {{{ zend_bool php_runkit_parse_function_arg */
/** Parses either multiple strings (1. function args, 2. body 3. (optional) return type), or a Closure. */
inline static zend_bool php_runkit_parse_function_arg(int argc, zval *args, int arg_pos, zend_function **fe, zend_string** arguments, zend_string** phpcode, long *opt_arg_pos, char *type) {
	// If is successful, it returns true, advances opt_arg_pos. There are two was this could succeed
	// 1. *fe = zend_function extracted from a closure.
	// 2. *arguments, *phpcode = strings extracted from arguments
	if (Z_TYPE(args[arg_pos]) == IS_OBJECT && Z_OBJCE(args[arg_pos]) == zend_ce_closure) {
		*fe = (zend_function *) zend_get_closure_method_def(&(args[arg_pos]));
	} else if (Z_TYPE(args[arg_pos]) == IS_STRING) {
		(*opt_arg_pos)++;
		*arguments = Z_STR(args[arg_pos]);
		if (argc < arg_pos+2 || Z_TYPE(args[arg_pos+1]) != IS_STRING) {
			php_error_docref(NULL, E_ERROR, PHP_RUNKIT_BODY_ERROR_MSG, type);
			return 0;
		}
		*phpcode = Z_STR(args[arg_pos+1]);
	} else {
		php_error_docref(NULL, E_ERROR, PHP_RUNKIT_BODY_ERROR_MSG, type);
		return 0;
	}
	return 1;
}
/* }}} */

// TODO: redundant and part of an unsupported feature
#	define PHP_RUNKIT_DESTROY_FUNCTION(fe) 	destroy_zend_function(fe);

// TODO: move to a separate file.
void php_runkit_update_reflection_object_name(zend_object* object, int handle, const char* name);

	// These struct definitions must be identical to those in ext/reflection/php_reflection.c

	/* Struct for properties */
	typedef struct _property_reference {
		zend_class_entry *ce;
		zend_property_info prop;
	} property_reference;

	/* Struct for parameters */
	typedef struct _parameter_reference {
		uint32_t offset;
		uint32_t required;
		struct _zend_arg_info *arg_info;
		zend_function *fptr;
	} parameter_reference;

	typedef enum {
		REF_TYPE_OTHER,      /* Must be 0 */
		REF_TYPE_FUNCTION,
		REF_TYPE_GENERATOR,
		REF_TYPE_PARAMETER,
		REF_TYPE_TYPE,
		REF_TYPE_PROPERTY,
		REF_TYPE_DYNAMIC_PROPERTY
	} reflection_type_t;

	/* Struct for reflection objects */
	typedef struct {
		zval dummy; /* holder for the second property */
		zval obj;
		void *ptr;
		zend_class_entry *ce;
		reflection_type_t ref_type;
		unsigned int ignore_visibility:1;
		zend_object zo;
	} reflection_object;
#endif /* PHP_RUNKIT_MANIPULATION */

#ifdef PHP_RUNKIT_SANDBOX
// TODO: Figure out what the php7 equivalent of zend_object_store_bucket and zend_object_handle are.
/* {{{ php_runkit_zend_object_store_get_obj */
inline static zend_object *php_runkit_zend_object_store_get(const zval *zobject)
{
	// Note: Object handle may be removed from _zend_resource in the future.
	int handle = Z_OBJ_HANDLE_P(zobject);
	return EG(objects_store).object_buckets[handle];
}
/* }}} */
#endif

#endif	/* RUNKIT_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
