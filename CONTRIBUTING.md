# Contributing

Issues are tracked through [Github Issues](https://github.com/runkit7/runkit7/issues).
Please check if there are similar issues via github search before filing an issue.

Pull requests are welcome, test cases covering bug fixes or new functionality are preferred.

If reporting a crash, please include a minimal file (or link to a project) that can reproduce the issue, as well as the output of `php --version` and `php-config`. (And php.ini settings)

Issues labelled with [help wanted](https://github.com/runkit7/runkit7/labels/help%20wanted) are probably good first issues.

## Project layout

Headers are found in `php_runkit*.h`
Source files are found in `runkit*.c`

`config.m4` (Unix/Linux) and `config.w32` (windows) are used to generate `./configure` and the make file
`package.xml` contains the configuration for generating a pecl package. It should be updated when new tests or source files are added.

Some code still hasn't been ported from `config.m4` to `config.w32`. This code is disabled via C preprocessor macros (e.g. sandbox, etc.). See https://github.com/runkit7/runkit7#unsupported-apis-for-php7

## Running tests

Tests can be found in [tests/](./tests/).

```bash
phpize
./configure
make clean test
```

### PHP Extension development



For those unfamiliar with PHP5 extension writing:

- [PHP Internals book](http://www.phpinternalsbook.com/index.html#php-7) - Describes how to write extensions *for PHP7*
    (And has another section describing [PHP5 code](http://www.phpinternalsbook.com/index.html#php-5))
- [Upgrading PHP extensions from PHP5 to NG](https://wiki.php.net/phpng-upgrading)
- [PHPNG Implementation Details](https://wiki.php.net/phpng-int)


The representation of internal values(`zval`s) has changed between PHP5 and PHP7, along with the way refcounting is done.

- https://nikic.github.io/2015/05/05/Internal-value-representation-in-PHP-7-part-1.html (Useful background for coding in C)
- https://nikic.github.io/2015/06/19/Internal-value-representation-in-PHP-7-part-2.html
- https://nikic.github.io/2017/04/14/PHP-7-Virtual-machine.html (Useful for understanding runkit function/method manipulation)

This now uses `zend_string`.
I changed the code to use `zend_string` wherever possible to be consistent.
This is not strictly necessary.

Notes on `HashTable`s

- https://nikic.github.io/2014/12/22/PHPs-new-hashtable-implementation.html
- HashTables no longer use linked lists. They use an array of `Bucket`s instead, and use collision chaining.
  (TODO: implement `php_runkit_hash_move_to_front`)
- The new versions of `zend_hash_*` take `zend_string` pointers instead of pairs of `char*
- Most `zend_hash_*` now have equivalent `zend_hash_str_*` methods.
  (If I remember correctly, `zend_hash_str_*` methods now taken `strlen` as the length instead of `strlen+1`)
- To add/retrieve pointers from a `zend_hash`, there are now `zend_hash_*_ptr` methods.
  Depending on the table being used, these may call destructor functions when pointers are removed.

Changes to the internal representation of `HashTable`s require a lot of code changes.

Miscellaneous notes on differences betwen PHP5 and PHP7

- zend opcode, opline, and `zend_function`s have changed in PHP7.
- Stack frame layout has changed.
- Reflection data structures changed.
- And so on: https://wiki.php.net/phpng-upgrading (Upgrading extensions from PHP5 to PHP7)
- https://github.com/php/php-src/blob/PHP-7.0/UPGRADING - Describes changes to PHP that can be seen by PHP programmers. (E.g. backwards incompatible changes, deprecated functionality, new language features, etc.)
