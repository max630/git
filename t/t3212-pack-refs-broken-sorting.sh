#!/bin/sh

test_description='tests for the falsely sorted refs'
. ./test-lib.sh

test_expect_success 'setup' '
	git commit --allow-empty -m commit &&
	for num in $(test_seq 10)
	do
		git branch b$(printf "%02d" $num) || break
	done &&
	git pack-refs --all &&
	head_object=$(git rev-parse HEAD) &&
	printf "$head_object refs/heads/b00\\n" >>.git/packed-refs &&
	git branch b11
'

test_expect_success 'off-order branch not found' '
	! git show-ref --verify --quiet refs/heads/b00
'

test_expect_success 'subsequent pack-refs fails' '
	! git pack-refs --all
'

test_done
