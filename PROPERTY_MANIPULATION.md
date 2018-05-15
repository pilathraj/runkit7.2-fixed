As of php 7.0, it's possible to modify the defaults of existing properties.
However, adding or removing new properties is no longer feasible for the following reasons.

1. The property manipulation code still has bugs, and the "offset" used is in bytes as of php7, but still treated as an index in this code.
2. As of php7's new zval layout, The only way to "add" a default property would be to realloc() every single one
   of the `zend_object`s that are instances of that class (to make room for another property).
   This would break php internals and possibly extensions.
   A possible other way way would be to change the API to `runkit_default_property_modify($className, $propertyName, $value, $flags = TODO)`
   (with a precondition $propertyName already existed)
   The old way properties of objects were stored was as a pointer to an array.
   In php7, it's part of `zend_object` itself, similar to what is described in https://gcc.gnu.org/onlinedocs/gcc/Zero-Length.html (1-length, with an UNDEF value at the end)
3. It should be possible to meet many uses by modifying constructors with `runkit_method_move` and `runkit_method_add`,
   or using ReflectionProperty for getting and setting private properties.
   https://secure.php.net/manual/en/reflectionproperty.setaccessible.php (sets accessibility only for ReflectionProperty)
   https://secure.php.net/manual/en/reflectionproperty.setvalue.php
   https://secure.php.net/manual/en/reflectionproperty.getvalue.php
