--TEST--
runkit_default_property_remove() function
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip";
	  if(!function_exists('runkit_default_property_remove')) print "skip";
?>
--INI--
error_reporting=E_ALL
display_errors=On
--FILE--
<?php
class RunkitClass {
    public static $publicProperty = 1;
    public static $publicproperty = 2;
}

ini_set('display_errors', 1);
ini_set('error_reporting', E_ALL);

$a = new RunkitClass();
$className = 'RunkitClass';
printf("%s\n", $a->publicproperty);
printf("%s\n", isset($a->publicproperty));
runkit_default_property_remove($className, 'publicproperty');
$b = new RunkitClass();
printf("%s\n", $b->publicproperty);
printf("%s\n", $a->publicProperty);
--EXPECTF--
2
1

1
