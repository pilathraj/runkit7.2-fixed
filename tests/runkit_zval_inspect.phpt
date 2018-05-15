--TEST--
runkit_zval_inspect() function
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
--INI--
error_reporting=E_ALL
display_errors=on
--FILE--
<?php
$a = 1;

var_dump(runkit_zval_inspect($a));
?>
--EXPECTF--
array(2) {
  ["address"]=>
  string(%d) "%s"
  ["type"]=>
  int(4)
}
