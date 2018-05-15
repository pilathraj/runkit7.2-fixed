--TEST--
removing magic __callstatic method
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip";
?>
--FILE--
<?php
class Test {
    static function __callstatic($m, $args) {echo '__callstatic';}
}

Test::method();
runkit_method_remove("Test", "__callstatic");
Test::method();
?>
--EXPECTF--
__callstatic
Fatal error: Uncaught Error: Call to undefined method Test::method() in %s:%d
Stack trace:
#0 {main}
  thrown in %s on line %d
