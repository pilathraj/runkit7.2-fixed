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
	public static function run() {
		$c = 'use';
		$d = 'ref_use';
		runkit_function_redefine('runkit_function',
			function($a, $b = "bar") use ($c, &$d) {
				static $is="is";
				global $g;
				echo "a $is $a\nb $is $b\n";
				echo "c $is $c\nd $is $d\n";
				echo "g $is $g\n";
				$d .= ' modified';
			}
		);
		runkit_function('foo', 'bar');
		echo "d after call is $d\n";
	}
}
$g = 'global';
test::run();
runkit_function('foo','bar');
?>
--EXPECTF--
a is foo
b is bar
c is use
d is ref_use
g is global
d after call is ref_use modified
a is foo
b is bar
c is use
d is ref_use modified
g is global
