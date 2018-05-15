--TEST--
runkit_function_add() function should detect invalid return types passed in as a string
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
--INI--
display_errors=on
--FILE--
<?php declare(strict_types=1);
ini_set('error_reporting', (string)(E_ALL));

$retval = runkit_function_add('runkit_function', 'string $a, $valid=false', 'return $valid ? $a : new stdClass();', false, '/** doc comment */', 'string#');
printf("runkit_function_add returned: %s\n", var_export($retval, true));
printf("Function exists: %s\n", var_export(function_exists('runkit_function'), true));
?>
--EXPECTF--
Warning: runkit_function_add(): Return type should match regex %s in %s on line %d
runkit_function_add returned: false
Function exists: false
