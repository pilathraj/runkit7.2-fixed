--TEST--
runkit_function_redefine() function and runkit_function_remove(), with variadic functions and strict mode.
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip"; ?>
--INI--
display_errors=on
--FILE--
<?php
function create_mock(string $originalName, string $temporaryName, string $return_type, bool $is_strict) {
    if (!runkit_function_copy($originalName, $temporaryName))
        throw new RuntimeException($originalName . ' runkit_method_copy create_mock');
    if (!runkit_function_remove($originalName))
        throw new RuntimeException($originalName . ' runkit_method_remove create_mock');
    $src = 'printf("In mock: %s\n", serialize(func_get_args()));return "42";';
    if (!runkit_function_add($originalName, '', $src, false, '', $return_type, $is_strict))
        throw new RuntimeException($originalName . ' runkit_method_add create_mock');
}

function remove_mock(string $originalName, string $temporaryName) {
    if (!runkit_function_remove($originalName))
        throw new RuntimeException($originalName . ' runkit_function_remove1 remove_mock');
    if (!runkit_function_copy($temporaryName, $originalName))
        throw new RuntimeException($originalName . ' runkit_function_copy remove_mock');
    if (!runkit_function_remove($temporaryName))
        throw new RuntimeException($originalName . ' runkit_function_remove2 remove_mock');
}

function methodName($arg) {
    return 33 + $arg;
}

function bar($method, ...$args) {
    $id = getter();
    return $id . call_user_func_array($method, $args);
}

function getter() {
    return 'VALUE';
}

function main() {
    ini_set('error_reporting', E_ALL | E_STRICT);
    printf("Before mock: %s\n", var_export(bar('methodName', 0), true));
    create_mock('bar', 'bar0000001123', 'string', true);
    printf("After mock: %s\n", var_export(bar('methodName', 0), true));
    remove_mock('bar', 'bar0000001123');
    printf("After unmock: %s\n", var_export(bar('methodName', 100), true));
    // should throw
    create_mock('bar', 'bar0000001123', 'int', true);
    try {
        printf("After mock: %s\n", var_export(bar('methodName', 0), true));
    } catch (TypeError $e) {
        printf("When strict mode was true, this threw an exception: %s\n", $e->getMessage());
    }
    remove_mock('bar', 'bar0000001123');
    printf("In weak mode\n");
    printf("After unmock: %s\n", var_export(bar('methodName', 100), true));
    create_mock('bar', 'bar0000001123', 'int', false);
    printf("After mock: %s\n", var_export(bar('methodName', 0), true));
    remove_mock('bar', 'bar0000001123');
    printf("After unmock: %s\n", var_export(bar('methodName', 100), true));
}
main();
?>
--EXPECT--
Before mock: 'VALUE33'
In mock: a:2:{i:0;s:10:"methodName";i:1;i:0;}
After mock: '42'
After unmock: 'VALUE133'
In mock: a:2:{i:0;s:10:"methodName";i:1;i:0;}
When strict mode was true, this threw an exception: Return value of bar() must be of the type integer, string returned
In weak mode
After unmock: 'VALUE133'
In mock: a:2:{i:0;s:10:"methodName";i:1;i:0;}
After mock: 42
After unmock: 'VALUE133'
