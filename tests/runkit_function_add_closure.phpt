--TEST--
runkit_method_add() function with closure
--SKIPIF--
<?php
	if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip";
?>
--INI--
display_errors=on
--FILE--
<?php

class test {
	public function run() {
		$c = 'use';
		$d = 'ref_use';
		runkit_function_add('runkit_function',
			function($a, $b = "bar") use ($c, &$d) {
				static $is="is";
				global $g;
				echo "a $is $a\nb $is $b\n";
				echo "c $is $c\nd $is $d\n";
				echo "g $is $g\n";
				$d .= ' modified';
				echo '$this is';
				try {
					var_dump($this);
				} catch (Error $e) {
					echo "\n";
					// In PHP 7.1, they treated $this more consistently. It was also possible to declare variables called $this in php <= 7.0
					// Since this is a function created by runkit_function_add, not a method,
					// it is guaranteed that it is not an object context (A function written normally would also throw an Error, so this is expected)
					if (PHP_VERSION_ID < 70100) {
						throw $e;
					}
					if ($e->getMessage() !== 'Using $this when not in object context') {
						throw $e;
					}
					printf("(In php7, this is a thrown Error, not a )Notice: Undefined variable: this in %s on line %d\n", $e->getFile(), $e->getLine());
					var_dump(NULL);  // Dump null to match --EXPECT--
				}
			}
		);
		runkit_function('foo', 'bar');
		echo "d after call is $d\n";
	}
}
$g = 'global';
$t = new test();
$t->run();
runkit_function('foo','bar');
?>
--EXPECTREGEX--
a is foo
b is bar
c is use
d is ref_use
g is global
\$this is
.*Notice: Undefined variable: this in .* on line \d+
NULL
d after call is ref_use modified
a is foo
b is bar
c is use
d is ref_use modified
g is global
\$this is
.*Notice: Undefined variable: this in .* on line \d+
NULL
