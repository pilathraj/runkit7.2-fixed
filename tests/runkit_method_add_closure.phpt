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
ini_set('error_reporting', E_ALL & (~E_DEPRECATED) & (~E_STRICT));

class runkit_class {}

class test {
	public function run() {
		$c = 'use';
		$d = 'ref_use';
		runkit_method_add('runkit_class', 'runkit_method',
			function($a, $b = "bar") use ($c, &$d) {
			static $is="is";
			global $g;
			echo "a $is $a\nb $is $b\n";
			echo "c $is $c\nd $is $d\n";
			echo "g $is $g\n";
			$d .= ' modified';
			echo '$this=';
			try {
				var_dump($this);
			} catch(Error $e) {
				// PHP 7.1 would also throw an Error if this was the actual implementation of runkit_method.
				printf("\nError: %s\n", $e->getMessage());
				printf("NULL\n");
			}
		});
		runkit_class::runkit_method('foo', 'bar');
		echo "d after call is $d\n";
	}
}
$g = 'global';
$t = new test();
$t->run();
$rc = new runkit_class();
$rc->runkit_method('foo','bar');
?>
--EXPECTREGEX--
a is foo
b is bar
c is use
d is ref_use
g is global
\$this=
(Notice: Undefined variable: this in .* on line [0-9]+|Error: Using \$this when not in object context)
NULL
d after call is ref_use modified
a is foo
b is bar
c is use
d is ref_use modified
g is global
\$this=object\(runkit_class\)#2 \(0\) {
}
