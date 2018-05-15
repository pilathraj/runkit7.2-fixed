--TEST--
runkit_method_redefine() function and runkit_method_remove(), with variadic functions and strict mode.
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
--INI--
display_errors=on
--FILE--
<?php
function create_mock(string $className, string $originalName, string $temporaryName, $return_type, bool $is_strict) {
    if (!runkit_method_copy($className, $temporaryName, $className, $originalName))
        throw new RuntimeException($className . '::' . $originalName . ' runkit_method_copy create_mock');
    if (!runkit_method_remove($className, $originalName))
        throw new RuntimeException($className . '::' . $originalName . ' runkit_method_remove create_mock');
    $src = 'printf("In mock: %s\n", serialize(func_get_args()));return "42";';
    if (!runkit_method_add($className, $originalName, '', $src, RUNKIT_ACC_STATIC, '', $return_type, $is_strict))
        throw new RuntimeException($className . '::' . $originalName . ' runkit_method_add create_mock');
}

function remove_mock(string $className, string $originalName, string $temporaryName) {
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
    public static function bar($method, ...$args) {
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
    create_mock('foo', 'bar', 'bar0000001123', 'string', true);
    printf("After mock: %s\n", var_export(foo::bar('methodName', 0), true));
    remove_mock('foo', 'bar', 'bar0000001123');
    printf("After unmock: %s\n", var_export(foo::bar('methodName', 100), true));
    // should throw
    create_mock('foo', 'bar', 'bar0000001123', 'int', true);
    try {
        printf("After mock: %s\n", var_export(foo::bar('methodName', 0), true));
    } catch (TypeError $e) {
        printf("When strict mode was true, this threw an exception: %s\n", $e->getMessage());
    }
    remove_mock('foo', 'bar', 'bar0000001123');
    printf("In weak mode\n");
    printf("After unmock: %s\n", var_export(foo::bar('methodName', 100), true));
    create_mock('foo', 'bar', 'bar0000001123', 'int', false);
    printf("After mock: %s\n", var_export(foo::bar('methodName', 0), true));
    remove_mock('foo', 'bar', 'bar0000001123');
    printf("After unmock: %s\n", var_export(foo::bar('methodName', 100), true));
}
main();
?>
--EXPECT--
Before mock: 'VALUE33'
In mock: a:2:{i:0;s:10:"methodName";i:1;i:0;}
After mock: '42'
After unmock: 'VALUE133'
In mock: a:2:{i:0;s:10:"methodName";i:1;i:0;}
When strict mode was true, this threw an exception: Return value of foo::bar() must be of the type integer, string returned
In weak mode
After unmock: 'VALUE133'
In mock: a:2:{i:0;s:10:"methodName";i:1;i:0;}
After mock: 42
After unmock: 'VALUE133'
