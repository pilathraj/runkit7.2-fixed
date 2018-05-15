--TEST--
reflection should pick up runkit arginfo
--SKIPIF--
<?php
if(!extension_loaded("runkit") || !function_exists('runkit_superglobals') || !PHP_RUNKIT_MANIPULATION) print "skip";
?>
--INI--
error_reporting=E_ALL
--FILE--
<?php
foreach ([
	'runkit_constant_add',
	'runkit_constant_redefine',
	'runkit_constant_remove',
	'runkit_function_add',
	'runkit_function_copy',
	'runkit_function_redefine',
	'runkit_function_remove',
	'runkit_function_rename',
	'runkit_method_add',
	'runkit_method_copy',
	'runkit_method_redefine',
	'runkit_method_remove',
	'runkit_method_rename',
	'runkit_object_id',
	'runkit_superglobals',
	'runkit_zval_inspect',
] as $name) {
	$function = new ReflectionFunction($name);
	printf("%-30s: %d to %d args: (%s)\n",
		$name,
		$function->getNumberOfRequiredParameters(),
		$function->getNumberOfParameters(),
	   	implode(', ', array_map(function(ReflectionParameter $param) {
			return '$' . $param->getName();
		}, $function->getParameters()))
	);
}


?>
--EXPECT--
runkit_constant_add           : 2 to 3 args: ($constname, $value, $newVisibility)
runkit_constant_redefine      : 2 to 3 args: ($constname, $value, $newVisibility)
runkit_constant_remove        : 1 to 1 args: ($constname)
runkit_function_add           : 2 to 7 args: ($funcname, $arglist_or_closure, $code_or_doc_comment, $return_by_reference, $doc_comment, $return_type, $is_strict)
runkit_function_copy          : 2 to 2 args: ($funcname, $targetname)
runkit_function_redefine      : 3 to 7 args: ($funcname, $arglist_or_closure, $code_or_doc_comment, $return_by_reference, $doc_comment, $return_type, $is_strict)
runkit_function_remove        : 1 to 1 args: ($funcname)
runkit_function_rename        : 2 to 2 args: ($funcname, $newname)
runkit_method_add             : 3 to 8 args: ($classname, $methodname, $arglist_or_closure, $code_or_flags, $flags_or_doc_comment, $doc_comment, $return_type, $is_strict)
runkit_method_copy            : 3 to 4 args: ($dClass, $dMethod, $sClass, $sMethod)
runkit_method_redefine        : 3 to 8 args: ($classname, $methodname, $arglist_or_closure, $code_or_flags, $flags_or_doc_comment, $doc_comment, $return_type, $is_strict)
runkit_method_remove          : 2 to 2 args: ($classname, $methodname)
runkit_method_rename          : 3 to 3 args: ($classname, $methodname, $newname)
runkit_object_id              : 1 to 1 args: ($obj)
runkit_superglobals           : 0 to 0 args: ()
runkit_zval_inspect           : 1 to 1 args: ($value)
