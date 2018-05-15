--TEST--
runkit_import() Importing file with syntax error in it
--SKIPIF--
<?php if(!extension_loaded("runkit") || !RUNKIT_FEATURE_MANIPULATION) print "skip";
      if(!function_exists('runkit_import')) print "skip";
?>
--FILE--
<?php
try {
    runkit_import(dirname(__FILE__) . '/runkit_import_with_syntax_error.inc');
} catch (ParseError $e) {
    printf("Caught ParseError at %s:%d: %s\n", $e->getFile(), $e->getLine(), $e->getMessage());
}
echo "After syntax error";
?>
--EXPECTF--
Caught ParseError at %srunkit_import_with_syntax_error.inc:2: syntax error, unexpected ')'
After syntax error
