--TEST--
runkit_constant_add() function can add simple arrays
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
--FILE--
<?php
runkit_constant_add('FOO', ["BAR"]);
var_export(FOO);
echo "\n";
runkit_constant_redefine('FOO', [['key' => "BAR"]]);
var_export(FOO);
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
