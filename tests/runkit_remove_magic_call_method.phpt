--TEST--
removing magic __call method
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip";
?>
--FILE--
<?php
class Test {
    function __call($m, $args) {echo '__call';}
}

$a = new Test();
$a->method();
runkit_method_remove("Test", "__call");
$a->method();
?>
--EXPECTF--
__call
Fatal error: Uncaught Error: Call to undefined method Test::method() in %s:%d
Stack trace:
#0 {main}
  thrown in %s on line %d
