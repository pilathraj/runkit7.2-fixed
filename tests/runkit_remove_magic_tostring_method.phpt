--TEST--
removing magic __tostring method
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip";
?>
--FILE--
<?php
class Test {
    function __tostring() {echo '__tostring';}
}

$a = new Test();
(string) $a;
runkit_method_remove("Test", "__tostring");
(string) $a;
?>
--EXPECTF--
__tostring
%s fatal error: Method Test::__toString() must return a string value in %s on line %d
