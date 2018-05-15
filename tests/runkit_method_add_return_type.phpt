--TEST--
runkit_method_add() function should accept valid return types passed in as a string
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
--INI--
display_errors=on
--FILE--
<?php declare(strict_types=1);
ini_set('error_reporting', (string)(E_ALL));

class runkit_class {
}

runkit_method_add('runkit_class', 'runkit_method', 'string $a, $valid=false', 'return $valid ? $a : new stdClass();', RUNKIT_ACC_STATIC, '/** doc comment */', 'string');
printf("Returned %s\n", runkit_class::runkit_method('foo', true));
try {
	printf("Returned %s\n", runkit_class::runkit_method('notastring', false));
} catch (TypeError $e) {
	printf("TypeError: %s", $e->getMessage());
}
?>
--EXPECT--
Returned foo
TypeError: Return value of runkit_class::runkit_method() must be of the type string, object returned
