#!/bin/sh

export REPORT_EXIT_STATUS=1
export TESTS=--show-diff

make test
