--TEST--
runkit.superglobal setting creates superglobals that can be referenced multiple ways.
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
<?php if(!extension_loaded("session")) print "skip - This test assumes \$_SESSION will exist, but the session extension isn't enabled/installed"; ?>
--INI--
display_errors=on
runkit.superglobal=foo
--FILE--
<?php
ini_set('error_reporting', E_ALL);

function capture_runkit_superglobals_dump() {
    ob_start();
    debug_zval_dump(runkit_superglobals());
    return ob_get_clean();
}

class FooClass {
    public $prop;
    public function __construct($value) {
        $this->prop = $value;
    }

    public function bar() {
        debug_print_backtrace();
        var_dump($GLOBALS['foo']);
        var_dump($foo);
        // Verify that runkit_superglobals properly reference counts string keys, call it twice.
        $result = capture_runkit_superglobals_dump();
        echo $result;
        debug_zval_dump(array_keys($GLOBALS));
        $result2 = capture_runkit_superglobals_dump();
        echo "result === result2: ";
        $same = $result === $result2;
        var_dump($same);
        if (!$same) {
            echo "result2\n";
            var_dump($result2);
        }
    }
}

function initfoo() {
    $foo = new FooClass('value');
}

function usefoo() {
    $foo->bar();
}
initfoo();
usefoo();
?>
--EXPECTF--
#0  FooClass->bar() called at [%s:%d]
#1  usefoo() called at [%s:%d]
object(FooClass)#1 (1) {
  ["prop"]=>
  string(5) "value"
}
object(FooClass)#1 (1) {
  ["prop"]=>
  string(5) "value"
}
array(10) refcount(1){
  [0]=>
  string(7) "GLOBALS" refcount(%d)
  [1]=>
  string(4) "_GET" refcount(%d)
  [2]=>
  string(5) "_POST" refcount(%d)
  [3]=>
  string(7) "_COOKIE" refcount(%d)
  [4]=>
  string(7) "_SERVER" refcount(%d)
  [5]=>
  string(4) "_ENV" refcount(%d)
  [6]=>
  string(8) "_REQUEST" refcount(%d)
  [7]=>
  string(6) "_FILES" refcount(%d)
  [8]=>
  string(8) "_SESSION" refcount(%d)
  [9]=>
  string(3) "foo" refcount(%d)
}
array(9) refcount(%d){
  [0]=>
  string(4) "_GET" refcount(%d)
  [1]=>
  string(5) "_POST" refcount(%d)
  [2]=>
  string(7) "_COOKIE" refcount(%d)
  [3]=>
  string(6) "_FILES" refcount(%d)
  [4]=>
  string(4) "argv" refcount(%d)
  [5]=>
  string(4) "argc" refcount(%d)
  [6]=>
  string(7) "_SERVER" refcount(%d)
  [7]=>
  string(7) "GLOBALS" refcount(%d)
  [8]=>
  string(3) "foo" refcount(%d)
}
result === result2: bool(true)
