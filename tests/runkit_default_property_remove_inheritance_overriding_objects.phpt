--TEST--
runkit_default_property_remove() remove properties with inheritance overriding objects
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
    public $publicProperty = 1;
    private $privateProperty = "a";
    protected $protectedProperty = "b";
    private static $staticProperty = "s";
    public $removedProperty = "r";
}

class RunkitSubClass extends RunkitClass {
    public $publicProperty = 2;
    private $privateProperty = "aa";
    protected $protectedProperty = "bb";
    protected $staticProperty = "ss";
    function getPrivate() {return $this->privateProperty;}
}
class RunkitSubSubClass extends RunkitSubClass {
    protected $protectedProperty = "cc";
}

ini_set('display_errors', 1);
ini_set('error_reporting', E_ALL);

$className = 'RunkitClass';
$obj = new RunkitSubSubClass();

runkit_default_property_remove($className, 'publicProperty', TRUE);
runkit_default_property_remove($className, 'privateProperty', TRUE);
runkit_default_property_remove($className, 'protectedProperty', TRUE);
runkit_default_property_remove('RunkitSubClass', 'removedProperty', TRUE);
runkit_default_property_remove($className, 'removedProperty', TRUE);
$out = print_r($obj, true);
print $out;
print_r($obj->getPrivate());
?>
--EXPECTF--
RunkitSubSubClass Object
(
    [protectedProperty:protected] => cc
    [publicProperty] => 2
    [privateProperty%sprivate] => aa
    [staticProperty:protected] => ss
)
aa
