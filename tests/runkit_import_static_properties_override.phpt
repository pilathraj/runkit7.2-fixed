--TEST--
runkit_import() Importing and overriding non-static and static properties with static properties
--SKIPIF--
<?php
	if (!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) {
		echo "skip";
	}
	if (!function_exists('runkit_import')) {
		echo "skip";
	}
?>
--INI--
display_errors=on
--FILE--
<?php
class Test {
    public $n = 1;
    public static $s = 2;
    private $i = 3;
    protected static $o = 4;

    public function getPrivate() { return Test::$i; }
    public function getProtected() { return Test::$o; }
}

ini_set('error_reporting', E_ALL & ~E_DEPRECATED & ~E_STRICT);

$t = new Test;
runkit_import(dirname(__FILE__) . '/runkit_import_static_properties_override.inc', RUNKIT_IMPORT_CLASS_STATIC_PROPS | RUNKIT_IMPORT_OVERRIDE);
$t = new Test;
var_dump($t->n);
var_dump(Test::$n);
var_dump(Test::$s);
var_dump(Test::getPrivate());
var_dump(Test::getProtected());
?>
--EXPECTF--

Notice: runkit_import(): Making Test::i public to remove it from class without objects overriding in %s on line %d

Notice: Undefined property: Test::$n in %s on line %d
NULL
int(3)
int(4)
int(5)
int(6)
--XFAIL--
Using PHP_RUNKIT_IMPORT_OVERRIDE in combination with PHP_RUNKIT_IMPORT_CLASS_STATIC_PROPS is not supported, may be supported in the future.
