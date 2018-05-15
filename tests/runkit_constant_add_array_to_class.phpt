--TEST--
runkit_constant_add() function can add simple arrays to classes
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
--FILE--
<?php
class A { }
runkit_constant_add('A::FOO', ["BAR"]);
var_export(A::FOO);
echo "\n";
runkit_constant_redefine('A::FOO', [['key' => "BAR"]]);
var_export(A::FOO);
?>
--EXPECT--
array (
  0 => 'BAR',
)
array (
  0 => 
  array (
    'key' => 'BAR',
  ),
)
