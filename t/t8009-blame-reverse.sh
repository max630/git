#!/bin/sh

test_description='git blame reverse'
. ./test-lib.sh

test_expect_success setup '
	test_commit A0 file.t line0 &&
	test_commit A1 &&
	git reset --hard A0 &&
	test_commit B1 &&
	test_commit B2 file.t line0changed &&
	git reset --hard A1 &&
	test_merge A2 B2 &&
	git reset --hard A1 &&
	test_commit C1 &&
	git reset --hard A2 &&
	test_merge A3 C1
	'

test_expect_failure 'blame --reverse finds B1, not C1' '
	git blame --porcelain --reverse A0..A3 -- file.t >actual_full &&
	head -1 <actual_full | sed -e "sX .*XX" >actual &&
	git rev-parse B1 >expect &&
	test_cmp expect actual
	'

test_expect_failure 'blame --reverse --first-parent finds A1' '
	git blame --porcelain --reverse --first-parent A0..A3 -- file.t >actual_full &&
	head -1 <actual_full | sed -e "sX .*XX" >actual &&
	git rev-parse A1 >expect &&
	test_cmp expect actual
	'

test_done
