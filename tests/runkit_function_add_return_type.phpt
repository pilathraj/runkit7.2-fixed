--TEST--
runkit_function_add() function should accept valid return types passed in as a string
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
--INI--
display_errors=on
--FILE--
<?php declare(strict_types=1);
ini_set('error_reporting', (string)(E_ALL));

runkit_function_add('runkit_function', 'string $a, $valid=false', 'return $valid ? $a : new stdClass();', false, '/** doc comment */', 'string');
printf("Returned %s\n", runkit_function('foo', true));
try {
	printf("Returned %s\n", runkit_function('notastring', false));
} catch (TypeError $e) {
	printf("TypeError: %s", $e->getMessage());
}
?>
--EXPECT--
Returned foo
TypeError: Return value of runkit_function() must be of the type string, object returned
