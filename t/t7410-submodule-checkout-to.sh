#!/bin/sh

test_description='Combination of submodules and multiple workdirs'

. ./test-lib.sh

base_path=$(pwd -P)

test_expect_success 'setup: make origin' \
    'mkdir -p origin/sub && ( cd origin/sub && git init &&
	echo file1 >file1 &&
	git add file1 &&
	git commit -m file1 ) &&
    mkdir -p origin/main && ( cd origin/main && git init &&
	git submodule add ../sub &&
	git commit -m "add sub" ) &&
    ( cd origin/sub &&
	echo file1updated >file1 &&
	git add file1 &&
	git commit -m "file1 updated" ) &&
    ( cd origin/main/sub && git pull ) &&
    ( cd origin/main &&
	git add sub &&
	git commit -m "sub updated" )'

test_expect_success 'setup: clone' \
    'mkdir clone && ( cd clone &&
	git clone --recursive "$base_path/origin/main")'

rev1_hash_main=$(git --git-dir=origin/main/.git show --pretty=format:%h -q "HEAD~1")
rev1_hash_sub=$(git --git-dir=origin/sub/.git show --pretty=format:%h -q "HEAD~1")

test_expect_success 'checkout main' \
    'mkdir default_checkout &&
    (cd clone/main &&
	git checkout --to "$base_path/default_checkout/main" "$rev1_hash_main")'

# TODO: this must never work without --recursive
test_expect_failure 'can see submodule diffs just after checkout' \
    '(cd default_checkout/main && git diff --submodule master"^!" | grep "file1 updated")'

test_expect_success 'checkout main and initialize independed clones' \
    'mkdir fully_cloned_submodule &&
    (cd clone/main &&
	git checkout --to "$base_path/fully_cloned_submodule/main" "$rev1_hash_main") &&
    (cd fully_cloned_submodule/main && git submodule update)'

# TODO: thus must require update --init
test_expect_success 'can see submodule diffs after independed cloning' \
    '(cd fully_cloned_submodule/main && git diff --submodule master"^!" | grep "file1 updated")'

# TODO: this is not needed to do now regularly, rather being a special case
# 1. manual checkout should point to somewhere else at all
# 2. no need to make that gitlink
test_expect_success 'checkout sub manually' \
    'mkdir linked_submodule &&
    (cd clone/main &&
	git checkout --to "$base_path/linked_submodule/main" "$rev1_hash_main") &&
    (cd clone/main/sub &&
	git checkout --to "$base_path/linked_submodule/main/sub" "$rev1_hash_sub") &&
    mkdir clone/main/.git/worktrees/main/modules &&
	echo "gitdir: ../../modules/sub/worktrees/sub" > clone/main/.git/worktrees/main/modules/sub'

test_expect_success 'can see submodule diffs after manual checkout of linked submodule' \
    '(cd linked_submodule/main && git diff --submodule master"^!" | grep "file1 updated")'

test_expect_success 'clone non-recursively and update to linked superpproject' \
    'mkdir clone_norec && ( cd clone_norec &&
	git clone "$base_path/origin/main" &&
	cd main &&
	git checkout --to "$base_path/worktree_with_submodule/main" "$rev1_hash_main") &&
    (cd worktree_with_submodule/main &&
	git submodule update --init)'

test_expect_success 'can see submodule diffs in worktree with independently updated submodule' \
    '(cd worktree_with_submodule/main && git diff --submodule master"^!" | grep "file1 updated")'

test_expect_success 'init submodule in main repository back' \
    '( cd clone_norec/main && git submodule update --init)'

test_expect_success 'can see submodule diffs in main repository which initalized after linked' \
    '(cd clone_norec/main && git diff --submodule master"^!" | grep "file1 updated")'

test_expect_success 'linked worktree is uptodate after chanages in main' \
    '(cd clone_norec/main && git checkout --detach master~1 && git submodule update) &&
    (cd worktree_with_submodule/main &&
	git status --porcelain >../../actual &&
	: >../../expected &&
	test_cmp ../../expected ../../actual)'

test_expect_success 'init another repository to test adding' \
    'mkdir -p add_area/repo &&
    (cd add_area/repo &&
	git init &&
	git commit --allow-empty -m main_commit &&
	git branch b2 &&
	git checkout --to ../worktree b2)'

test_expect_success 'add sub&history' \
    '(cd add_area/worktree &&
	git submodule add ../../origin/sub sub &&
	(cd sub && git checkout --detach "$rev1_hash_sub") &&
	git add sub &&
	git commit -m sub_added &&
	(cd sub && git checkout --detach origin/master) &&
	git add sub &&
	git commit -m sub_changed)'

test_expect_success 'inquire history after adding' \
    '(cd add_area/worktree &&
	git diff --submodule b2"^!" | grep "file1 updated")'

test_expect_success 'init submodule in main' \
    '(cd add_area/repo &&
	git reset --hard b2~1 &&
	git submodule update --init)'

test_expect_success 'linked worktree is uptodate after changes in original after adding' \
    '(cd add_area/worktree &&
	git status --porcelain >../../actual &&
	: >../../expected &&
	test_cmp ../../expected ../../actual)'

deinit_start()
{
    rm -rf deinit_area && \
    mkdir deinit_area && \
    ( cd deinit_area && \
	git clone "$base_path/origin/main" && \
	cd main && \
	git checkout --to ../worktree --detach HEAD )
}

deinit_init()
{
    (cd "deinit_area/$1" && git submodule update --init)
}

deinit_deinit()
{
    (cd "deinit_area/$1" && git submodule deinit sub)
}

deinit_not_checked_out()
{
    (cd "deinit_area/$1" && git diff --submodule master"^!" >actual && grep -q "not checked out" actual)
}

deinit_checked_out()
{
    (cd "deinit_area/$1" && git diff --submodule master"^!" >actual && grep -q "file1 updated" actual)
}

# TODO:
# 1. check that directories are cleaned and index is removed for main (?)
# 2. .git/config should be removed only after last owrktree is. Is it visible at all?
test_expect_success 'deinit main only - create' \
    'deinit_start && deinit_init main && deinit_deinit main'

test_expect_success 'deinit main only - not checked out' \
    'deinit_not_checked_out main'

test_expect_success 'deinit worktree only - create' \
    'deinit_init && deinit_init worktree && deinit_deinit worktree'

test_expect_success 'deinit worktree only - not checked out' \
    'deinit_not_checked_out worktree'

test_expect_success 'deinit from both, worktree then main - create' \
    'deinit_init && deinit_init main && deinit_init worktree && deinit_deinit worktree'

test_expect_success 'deinit from both, worktree then main - update' \
    '(cd deinit_area/main && git submodule update) &&
    (cd deinit_area/worktree && git submodule update)'

test_expect_success 'deinit from both, worktree then main - check#1' \
    'deinit_not_checked_out worktree && deinit_checked_out main'

test_expect_success 'deinit from both, worktree then main - deinit main' \
    'deinit_deinit main'

test_expect_success 'deinit from both, worktree then main - check#2' \
    'deinit_not_checked_out worktree && deinit_not_checked_out main'

test_expect_success 'deinit from both, main then worktree - create' \
    'deinit_init && deinit_init main && deinit_init worktree && deinit_deinit main'

test_expect_success 'deinit from both, worktree then main - update' \
    '(cd deinit_area/main && git submodule update) &&
    (cd deinit_area/worktree && git submodule update)'

test_expect_success 'deinit from both, main then worktree - check#1' \
    'deinit_not_checked_out main && deinit_checked_out worktree'

test_expect_success 'deinit from both, main then worktree - deinit worktree' \
    'deinit_deinit worktree'

test_expect_success 'deinit from both, main then worktree - check#2' \
    'deinit_not_checked_out worktree && deinit_not_checked_out main'

test_done