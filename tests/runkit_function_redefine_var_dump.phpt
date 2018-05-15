--TEST--
runkit_method_redefine() function with closure
--SKIPIF--
<?php
	if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip";
?>
--INI--
display_errors=on
--FILE--
<?php

function runkit_function() {}

class test {
	public function run() {
		runkit_function_redefine('runkit_function',
			function($a, $b = "bar") {
				echo "undefined is";
				var_dump($undefined);
			}
		);
		runkit_function('foo', 'bar');
		echo "Done";
	}
}
$t = new test();
$t->run();
?>
--EXPECTF--
undefined is
Notice: Undefined variable: undefined in %s on line %d
NULL
Done
