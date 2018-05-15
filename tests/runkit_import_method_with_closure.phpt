--TEST--
runkit_import() Importing and overriding method with closure
--SKIPIF--
<?php
    if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip";
    if(!function_exists('runkit_import')) print "skip";
?>
--FILE--
<?php
class Test {
    public function f() {
        $f = function() {
            echo "f\n";
        };
        $f();
    }
}

$t = new Test;
$t->f();
runkit_import(dirname(__FILE__) . '/runkit_import_method_with_closure.inc', RUNKIT_IMPORT_CLASS_METHODS | RUNKIT_IMPORT_FUNCTIONS | RUNKIT_IMPORT_OVERRIDE);
$t->f();
runkit_import(dirname(__FILE__) . '/runkit_import_method_with_closure.inc', RUNKIT_IMPORT_CLASS_METHODS | RUNKIT_IMPORT_FUNCTIONS | RUNKIT_IMPORT_OVERRIDE);
$t->f();
?>
--EXPECT--
f
IMPORTED: f
IMPORTED: f
