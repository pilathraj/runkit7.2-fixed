--TEST--
runkit_method_add() function should detect invalid return types passed in as a string
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
--INI--
display_errors=on
--FILE--
<?php declare(strict_types=1);

ini_set('error_reporting', (string)(E_ALL));

class runkit_class {}

foreach (['string#', 'string', '\\stdClass', '\\\\stdClass', '\\stdClass\\', '\\A2\\Bc', '\\A2\\\\Bc', "\\A2\\Bc\x00", '\\2A\\Bc'] as $i => $returnType) {
    printf("testing return type %s\n", var_export($returnType, true));
    $methodName = 'runkit_method' . $i;

    $retval = runkit_method_add('runkit_class', $methodName, 'string $a, $valid=false', 'return $valid ? $a : new stdClass();', RUNKIT_ACC_STATIC, '/** doc comment */', $returnType);
    printf("runkit_method_add returned: %s\n", var_export($retval, true));
    printf("Method exists: %s\n", var_export(method_exists('runkit_class', $methodName), true));
}
?>
--EXPECTF--
testing return type 'string#'

Warning: runkit_method_add(): Return type should match regex %s in %s on line 11
runkit_method_add returned: false
Method exists: false
testing return type 'string'
runkit_method_add returned: true
Method exists: true
testing return type '\\stdClass'
runkit_method_add returned: true
Method exists: true
testing return type '\\\\stdClass'

Warning: runkit_method_add(): Return type should match regex %s in %s on line 11
runkit_method_add returned: false
Method exists: false
testing return type '\\stdClass\\'

Warning: runkit_method_add(): Return type should match regex %s in %s on line 11
runkit_method_add returned: false
Method exists: false
testing return type '\\A2\\Bc'
runkit_method_add returned: true
Method exists: true
testing return type '\\A2\\\\Bc'

Warning: runkit_method_add(): Return type should match regex %s in %s on line 11
runkit_method_add returned: false
Method exists: false
testing return type '\\A2\\Bc' . "\0" . ''

Warning: runkit_method_add(): Return type should match regex %s in %s on line 11
runkit_method_add returned: false
Method exists: false
testing return type '\\2A\\Bc'

Warning: runkit_method_add(): Return type should match regex %s in %s on line 11
runkit_method_add returned: false
Method exists: false
