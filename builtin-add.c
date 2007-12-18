/*
 * "git add" builtin command
 *
 * Copyright (C) 2006 Linus Torvalds
 */
#include "cache.h"
#include "builtin.h"
#include "dir.h"
#include "exec_cmd.h"
#include "cache-tree.h"
#include "diff.h"
#include "diffcore.h"
#include "commit.h"
#include "revision.h"

static const char builtin_add_usage[] =
"git-add [-n] [-v] [-f] [--interactive | -i] [-u] [--refresh] [--] <filepattern>...";

static int take_worktree_changes;

static void prune_directory(struct dir_struct *dir, const char **pathspec, int prefix)
{
	char *seen;
	int i, specs;
	struct dir_entry **src, **dst;

	for (specs = 0; pathspec[specs];  specs++)
		/* nothing */;
	seen = xcalloc(specs, 1);

	src = dst = dir->entries;
	i = dir->nr;
	while (--i >= 0) {
		struct dir_entry *entry = *src++;
		if (match_pathspec(pathspec, entry->name, entry->len,
				   prefix, seen))
			*dst++ = entry;
	}
	dir->nr = dst - dir->entries;

	for (i = 0; i < specs; i++) {
		if (!seen[i] && !file_exists(pathspec[i]))
			die("pathspec '%s' did not match any files",
					pathspec[i]);
	}
        free(seen);
}

static void fill_directory(struct dir_struct *dir, const char **pathspec,
		int ignored_too)
{
	const char *path, *base;
	int baselen;

	/* Set up the default git porcelain excludes */
	memset(dir, 0, sizeof(*dir));
	if (!ignored_too) {
		dir->collect_ignored = 1;
		setup_standard_excludes(dir);
	}

	/*
	 * Calculate common prefix for the pathspec, and
	 * use that to optimize the directory walk
	 */
	baselen = common_prefix(pathspec);
	path = ".";
	base = "";
	if (baselen) {
		char *common = xmalloc(baselen + 1);
		memcpy(common, *pathspec, baselen);
		common[baselen] = 0;
		path = base = common;
	}

	/* Read the directory and prune it */
	read_directory(dir, path, base, baselen, pathspec);
	if (pathspec)
		prune_directory(dir, pathspec, baselen);
}

static void update_callback(struct diff_queue_struct *q,
			    struct diff_options *opt, void *cbdata)
{
	int i, verbose;

	verbose = *((int *)cbdata);
	for (i = 0; i < q->nr; i++) {
		struct diff_filepair *p = q->queue[i];
		const char *path = p->one->path;
		switch (p->status) {
		default:
			die("unexpected diff status %c", p->status);
		case DIFF_STATUS_UNMERGED:
		case DIFF_STATUS_MODIFIED:
		case DIFF_STATUS_TYPE_CHANGED:
			add_file_to_cache(path, verbose);
			break;
		case DIFF_STATUS_DELETED:
			remove_file_from_cache(path);
			cache_tree_invalidate_path(active_cache_tree, path);
			if (verbose)
				printf("remove '%s'\n", path);
			break;
		}
	}
}

static void update(int verbose, const char *prefix, const char **files)
{
	struct rev_info rev;
	init_revisions(&rev, prefix);
	setup_revisions(0, NULL, &rev, NULL);
	rev.prune_data = get_pathspec(prefix, files);
	rev.diffopt.output_format = DIFF_FORMAT_CALLBACK;
	rev.diffopt.format_callback = update_callback;
	rev.diffopt.format_callback_data = &verbose;
	if (read_cache() < 0)
		die("index file corrupt");
	run_diff_files(&rev, DIFF_RACY_IS_MODIFIED);
}

static void refresh(int verbose, const char **pathspec)
{
	char *seen;
	int i, specs;

	for (specs = 0; pathspec[specs];  specs++)
		/* nothing */;
	seen = xcalloc(specs, 1);
	if (read_cache() < 0)
		die("index file corrupt");
	refresh_index(&the_index, verbose ? 0 : REFRESH_QUIET, pathspec, seen);
	for (i = 0; i < specs; i++) {
		if (!seen[i])
			die("pathspec '%s' did not match any files", pathspec[i]);
	}
        free(seen);
}

static struct lock_file lock_file;

static const char ignore_error[] =
"The following paths are ignored by one of your .gitignore files:\n";

int cmd_add(int argc, const char **argv, const char *prefix)
{
	int i, newfd;
	int verbose = 0, show_only = 0, ignored_too = 0, refresh_only = 0;
	const char **pathspec;
	struct dir_struct dir;
	int add_interactive = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp("--interactive", argv[i]) ||
		    !strcmp("-i", argv[i]))
			add_interactive++;
	}
	if (add_interactive) {
		const char *args[] = { "add--interactive", NULL };

		if (add_interactive != 1 || argc != 2)
			die("add --interactive does not take any parameters");
		execv_git_cmd(args);
		exit(1);
	}

	git_config(git_default_config);

	newfd = hold_locked_index(&lock_file, 1);

	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];

		if (arg[0] != '-')
			break;
		if (!strcmp(arg, "--")) {
			i++;
			break;
		}
		if (!strcmp(arg, "-n")) {
			show_only = 1;
			continue;
		}
		if (!strcmp(arg, "-f")) {
			ignored_too = 1;
			continue;
		}
		if (!strcmp(arg, "-v")) {
			verbose = 1;
			continue;
		}
		if (!strcmp(arg, "-u")) {
			take_worktree_changes = 1;
			continue;
		}
		if (!strcmp(arg, "--refresh")) {
			refresh_only = 1;
			continue;
		}
		usage(builtin_add_usage);
	}

	if (take_worktree_changes) {
		update(verbose, prefix, argv + i);
		goto finish;
	}

	if (argc <= i) {
		fprintf(stderr, "Nothing specified, nothing added.\n");
		fprintf(stderr, "Maybe you wanted to say 'git add .'?\n");
		return 0;
	}
	pathspec = get_pathspec(prefix, argv + i);

	if (refresh_only) {
		refresh(verbose, pathspec);
		goto finish;
	}

	fill_directory(&dir, pathspec, ignored_too);

	if (show_only) {
		const char *sep = "", *eof = "";
		for (i = 0; i < dir.nr; i++) {
			printf("%s%s", sep, dir.entries[i]->name);
			sep = " ";
			eof = "\n";
		}
		fputs(eof, stdout);
		return 0;
	}

	if (read_cache() < 0)
		die("index file corrupt");

	if (dir.ignored_nr) {
		fprintf(stderr, ignore_error);
		for (i = 0; i < dir.ignored_nr; i++) {
			fprintf(stderr, "%s\n", dir.ignored[i]->name);
		}
		fprintf(stderr, "Use -f if you really want to add them.\n");
		die("no files added");
	}

	for (i = 0; i < dir.nr; i++)
		add_file_to_cache(dir.entries[i]->name, verbose);

 finish:
	if (active_cache_changed) {
		if (write_cache(newfd, active_cache, active_nr) ||
		    close(newfd) || commit_locked_index(&lock_file))
			die("Unable to write new index file");
	}

	return 0;
}
