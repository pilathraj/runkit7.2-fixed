#ifndef PHP_RUNKIT_ZEND_EXECUTE_API_H
#define PHP_RUNKIT_ZEND_EXECUTE_API_H
#include "Zend/zend.h"
int runkit_forward_call_user_function(zend_function *fbc, zend_function *fbc_inner, INTERNAL_FUNCTION_PARAMETERS);
#endif
