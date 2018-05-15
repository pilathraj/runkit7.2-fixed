--TEST--
runkit_function_remove() function
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
--FILE--
<?php
function runkitSample() {
	echo "Function Exists\n";
}

$name = 'runkitSample';
runkitSample();
runkit_function_remove($name);
if (!function_exists('runkitSample')) {
	echo "Function Removed\n";
}
echo $name, "\n";
runkitSample();
?>
--EXPECTF--
Function Exists
Function Removed
runkitSample

Fatal error: Uncaught Error: Call to undefined function runkit%sample() in %s:%d
Stack trace:
#0 {main}
  thrown in %s on line %d
