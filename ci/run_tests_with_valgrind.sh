#!/bin/sh
# This script is used to run tests and detect memory leaks using valgrind.
# Running with valgrind is really slow.
unset TRAVIS
export REPORT_EXIT_STATUS=1
export TESTS='--show-diff -m'

if [ ! -x /usr/bin/valgrind ]; then
	echo "Valgrind is not installed!" 1>&2
	exit 1
fi

echo "\n\n"
echo "################################################################################"
echo "# Running with valgrind enabled                                                #"
echo "################################################################################\n\n"
yes n | make test
