--TEST--
runkit_method_redefine() function and runkit_method_remove(), with variadic functions with return values
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
--INI--
display_errors=on
--FILE--
<?php
// This is the same as runkit_method_variadic.phpt, with return types added to the original redefined function
function create_mock($className, $originalName, $temporaryName) {
    if (!runkit_method_copy($className, $temporaryName, $className, $originalName))
        throw new RuntimeException($className . '::' . $originalName . ' runkit_method_copy create_mock');
    if (!runkit_method_remove($className, $originalName))
        throw new RuntimeException($className . '::' . $originalName . ' runkit_method_remove create_mock');
    if (!runkit_method_add($className, $originalName, '', 'printf("In mock: %s\n", serialize(func_get_args()));return null;', RUNKIT_ACC_STATIC))
        throw new RuntimeException($className . '::' . $originalName . ' runkit_method_add create_mock');
}

function remove_mock($className, $originalName, $temporaryName) {
    if (!runkit_method_remove($className, $originalName))
        throw new RuntimeException($className . '::' . $originalName . ' runkit_method_remove1 remove_mock');
    if (!runkit_method_copy($className, $originalName, $className, $temporaryName))
        throw new RuntimeException($className . '::' . $originalName . ' runkit_method_copy remove_mock');
    if (!runkit_method_remove($className, $temporaryName))
        throw new RuntimeException($className . '::' . $originalName . ' runkit_method_remove2 remove_mock');
}

class FooImpl {
    public function methodName($arg) {
        return 33 + $arg;
    }
}

class foo {
    public static function bar($method, ...$args) : string {
        global $impl;
        if ($impl === null) {
            throw new RuntimeException('No $impl');
        }

        $id = self::getter();
        return $id . call_user_func_array(array($impl, $method), $args);
    }

    public static function getter() {
        return 'VALUE';
    }
}

function main() {
    ini_set('error_reporting', E_ALL | E_STRICT);
    global $impl;
    $impl = new FooImpl();
    printf("Before mock: %s\n", var_export(foo::bar('methodName', 0), true));
    create_mock('foo', 'bar', 'bar0000001123');
    printf("After mock: %s\n", var_export(foo::bar('methodName', 0), true));
    remove_mock('foo', 'bar', 'bar0000001123');
    printf("After unmock: %s\n", var_export(foo::bar('methodName', 100), true));
}
main();
?>
--EXPECT--
Before mock: 'VALUE33'
In mock: a:2:{i:0;s:10:"methodName";i:1;i:0;}
After mock: NULL
After unmock: 'VALUE133'
