--TEST--
runkit_constant_redefine() function redefines class constants
--SKIPIF--
<?php
	if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip";
?>
--FILE--
<?php
// For this test case to actually work (The php compiler, not opcache, does this optimization), the code would need to be compiled with ZEND_COMPILE_NO_CONSTANT_SUBSTITUTION | ZEND_COMPILE_NO_PERSISTENT_CONSTANT_SUBSTITUTION
// That requires either patching php-src or triggering recompilation of code in affected class and in subclasses.
class TestClass {
	const FOO = 'foo';
}

$const = 'TestClass::FOO';
var_dump($const);
runkit_constant_redefine($const, 'bar');
var_dump($const, TestClass::FOO);
?>
--EXPECT--
string(14) "TestClass::FOO"
string(14) "TestClass::FOO"
string(3) "bar"
--XFAIL--
Expected to fail because PHP VM inlines class constants as their value
within the same file when it is safe to do so.
