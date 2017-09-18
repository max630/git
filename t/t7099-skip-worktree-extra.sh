#!/bin/sh

test_description=TODO

. ./test-lib.sh

test_expect_success 'setup history' '
	mkdir subdir &&
	touch f1 f2 subdir/f3 subdir/f4 &&
	git add . &&
	git commit -m init &&
	git tag init &&
	echo modified >f1 &&
	echo modified >subdir/f3 &&
	git commit -m change f1 subdir/f3 &&
	git tag change
'

test_expect_success 'checkout empty init' '
	git reset --hard init &&
	git read-tree --empty &&
	git clean -dfx &&
	git ls-tree -z -r HEAD | git update-index -z --index-info &&
	git ls-files -z|git update-index --skip-worktree -z --stdin
'

test_expect_success 'verify empty init: no files' '
	test_path_is_missing f1 &&
	test_path_is_missing f2 &&
	test_path_is_missing subdir/f3 &&
	test_path_is_missing subdir/f4
'

test_expect_success 'verify empty init: no changes' '
	git status --porcelain -uno >expect &&
	test_must_be_empty expect
'

test_expect_success 'cleanup' 'rm expect'

test_expect_success 'merge' '
	git merge --no-ff --no-edit change
'

test_expect_success 'verify empty init: only changed files' '
	test_path_is_missing f2 &&
	test_path_is_missing subdir/f4
'

test_done
