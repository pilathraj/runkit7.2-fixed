--TEST--
add old-style parent ctor by importing
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION || !function_exists('runkit_import')) print "skip"; ?>
--FILE--
<?php

class Test {
}

class FOO_test extends test {
}

class FOO_test_Child extends FOO_test {
}

runkit_import("runkit_add_old_style_ctor_by_importing.inc", RUNKIT_IMPORT_CLASS_METHODS);
$a = new test;
$a = new foo_test;
$a = new FOO_test_Child;

echo "==DONE==\n";
?>
--EXPECTF--
Deprecated: Methods with the same name as their class will not be constructors in a future version of PHP; Test has a deprecated constructor in %srunkit_add_old_style_ctor_by_importing.inc on line 3
string(15) "new constructor"
string(15) "new constructor"
string(15) "new constructor"
==DONE==
