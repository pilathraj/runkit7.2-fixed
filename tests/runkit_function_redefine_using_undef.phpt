--TEST--
runkit_function_redefine() function with undefined 
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
--FILE--
<?php

function runkit_function() {}

runkit_function_redefine('runkit_function',
	function($a, $b = "bar") {
		echo "var_dump\n";
		var_dump($missing);
	}
);
echo "call\n";
runkit_function('foo');
echo "second call\n";
runkit_function('foo', 'bar');
echo "Done\n";
?>
--EXPECTF--
call
var_dump

Notice: Undefined variable: missing in %s on line %d
NULL
second call
var_dump

Notice: Undefined variable: missing in %s on line %d
NULL
Done
