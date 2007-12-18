#include "builtin.h"
#include "cache.h"

static const char git_config_set_usage[] =
"git-config [ --global | --system | [ -f | --file ] config-file ] [ --bool | --int ] [ -z | --null ] [--get | --get-all | --get-regexp | --replace-all | --add | --unset | --unset-all] name [value [value_regex]] | --rename-section old_name new_name | --remove-section name | --list";

static char *key;
static regex_t *key_regexp;
static regex_t *regexp;
static int show_keys;
static int use_key_regexp;
static int do_all;
static int do_not_match;
static int seen;
static char delim = '=';
static char key_delim = ' ';
static char term = '\n';
static enum { T_RAW, T_INT, T_BOOL } type = T_RAW;

static int show_all_config(const char *key_, const char *value_)
{
	if (value_)
		printf("%s%c%s%c", key_, delim, value_, term);
	else
		printf("%s%c", key_, term);
	return 0;
}

static int show_config(const char* key_, const char* value_)
{
	char value[256];
	const char *vptr = value;
	int dup_error = 0;

	if (!use_key_regexp && strcmp(key_, key))
		return 0;
	if (use_key_regexp && regexec(key_regexp, key_, 0, NULL, 0))
		return 0;
	if (regexp != NULL &&
			 (do_not_match ^
			  regexec(regexp, (value_?value_:""), 0, NULL, 0)))
		return 0;

	if (show_keys) {
		if (value_)
			printf("%s%c", key_, key_delim);
		else
			printf("%s", key_);
	}
	if (seen && !do_all)
		dup_error = 1;
	if (type == T_INT)
		sprintf(value, "%d", git_config_int(key_, value_?value_:""));
	else if (type == T_BOOL)
		vptr = git_config_bool(key_, value_) ? "true" : "false";
	else
		vptr = value_?value_:"";
	seen++;
	if (dup_error) {
		error("More than one value for the key %s: %s",
				key_, vptr);
	}
	else
		printf("%s%c", vptr, term);

	return 0;
}

static int get_value(const char* key_, const char* regex_)
{
	int ret = -1;
	char *tl;
	char *global = NULL, *repo_config = NULL;
	const char *system_wide = NULL, *local;

	local = getenv(CONFIG_ENVIRONMENT);
	if (!local) {
		const char *home = getenv("HOME");
		local = getenv(CONFIG_LOCAL_ENVIRONMENT);
		if (!local)
			local = repo_config = xstrdup(git_path("config"));
		if (home)
			global = xstrdup(mkpath("%s/.gitconfig", home));
		system_wide = ETC_GITCONFIG;
	}

	key = xstrdup(key_);
	for (tl=key+strlen(key)-1; tl >= key && *tl != '.'; --tl)
		*tl = tolower(*tl);
	for (tl=key; *tl && *tl != '.'; ++tl)
		*tl = tolower(*tl);

	if (use_key_regexp) {
		key_regexp = (regex_t*)xmalloc(sizeof(regex_t));
		if (regcomp(key_regexp, key, REG_EXTENDED)) {
			fprintf(stderr, "Invalid key pattern: %s\n", key_);
			goto free_strings;
		}
	}

	if (regex_) {
		if (regex_[0] == '!') {
			do_not_match = 1;
			regex_++;
		}

		regexp = (regex_t*)xmalloc(sizeof(regex_t));
		if (regcomp(regexp, regex_, REG_EXTENDED)) {
			fprintf(stderr, "Invalid pattern: %s\n", regex_);
			goto free_strings;
		}
	}

	if (do_all && system_wide)
		git_config_from_file(show_config, system_wide);
	if (do_all && global)
		git_config_from_file(show_config, global);
	git_config_from_file(show_config, local);
	if (!do_all && !seen && global)
		git_config_from_file(show_config, global);
	if (!do_all && !seen && system_wide)
		git_config_from_file(show_config, system_wide);

	free(key);
	if (regexp) {
		regfree(regexp);
		free(regexp);
	}

	if (do_all)
		ret = !seen;
	else
		ret = (seen == 1) ? 0 : seen > 1 ? 2 : 1;

free_strings:
	free(repo_config);
	free(global);
	return ret;
}

char *normalize_value(const char *key, const char *value)
{
	char *normalized;

	if (!value)
		return NULL;

	if (type == T_RAW)
		normalized = xstrdup(value);
	else {
		normalized = xmalloc(64);
		if (type == T_INT) {
			int v = git_config_int(key, value);
			sprintf(normalized, "%d", v);
		}
		else if (type == T_BOOL)
			sprintf(normalized, "%s",
				git_config_bool(key, value) ? "true" : "false");
	}

	return normalized;
}

int cmd_config(int argc, const char **argv, const char *prefix)
{
	int nongit = 0;
	char* value;
	const char *file = setup_git_directory_gently(&nongit);

	while (1 < argc) {
		if (!strcmp(argv[1], "--int"))
			type = T_INT;
		else if (!strcmp(argv[1], "--bool"))
			type = T_BOOL;
		else if (!strcmp(argv[1], "--list") || !strcmp(argv[1], "-l")) {
			if (argc != 2)
				usage(git_config_set_usage);
			if (git_config(show_all_config) < 0 && file && errno)
				die("unable to read config file %s: %s", file,
				    strerror(errno));
			return 0;
		}
		else if (!strcmp(argv[1], "--global")) {
			char *home = getenv("HOME");
			if (home) {
				char *user_config = xstrdup(mkpath("%s/.gitconfig", home));
				setenv(CONFIG_ENVIRONMENT, user_config, 1);
				free(user_config);
			} else {
				die("$HOME not set");
			}
		}
		else if (!strcmp(argv[1], "--system"))
			setenv(CONFIG_ENVIRONMENT, ETC_GITCONFIG, 1);
		else if (!strcmp(argv[1], "--file") || !strcmp(argv[1], "-f")) {
			if (argc < 3)
				usage(git_config_set_usage);
			if (!is_absolute_path(argv[2]) && file)
				file = prefix_filename(file, strlen(file),
						       argv[2]);
			else
				file = argv[2];
			setenv(CONFIG_ENVIRONMENT, file, 1);
			argc--;
			argv++;
		}
		else if (!strcmp(argv[1], "--null") || !strcmp(argv[1], "-z")) {
			term = '\0';
			delim = '\n';
			key_delim = '\n';
		}
		else if (!strcmp(argv[1], "--rename-section")) {
			int ret;
			if (argc != 4)
				usage(git_config_set_usage);
			ret = git_config_rename_section(argv[2], argv[3]);
			if (ret < 0)
				return ret;
			if (ret == 0) {
				fprintf(stderr, "No such section!\n");
				return 1;
			}
			return 0;
		}
		else if (!strcmp(argv[1], "--remove-section")) {
			int ret;
			if (argc != 3)
				usage(git_config_set_usage);
			ret = git_config_rename_section(argv[2], NULL);
			if (ret < 0)
				return ret;
			if (ret == 0) {
				fprintf(stderr, "No such section!\n");
				return 1;
			}
			return 0;
		}
		else
			break;
		argc--;
		argv++;
	}

	switch (argc) {
	case 2:
		return get_value(argv[1], NULL);
	case 3:
		if (!strcmp(argv[1], "--unset"))
			return git_config_set(argv[2], NULL);
		else if (!strcmp(argv[1], "--unset-all"))
			return git_config_set_multivar(argv[2], NULL, NULL, 1);
		else if (!strcmp(argv[1], "--get"))
			return get_value(argv[2], NULL);
		else if (!strcmp(argv[1], "--get-all")) {
			do_all = 1;
			return get_value(argv[2], NULL);
		} else if (!strcmp(argv[1], "--get-regexp")) {
			show_keys = 1;
			use_key_regexp = 1;
			do_all = 1;
			return get_value(argv[2], NULL);
		} else {
			value = normalize_value(argv[1], argv[2]);
			return git_config_set(argv[1], value);
		}
	case 4:
		if (!strcmp(argv[1], "--unset"))
			return git_config_set_multivar(argv[2], NULL, argv[3], 0);
		else if (!strcmp(argv[1], "--unset-all"))
			return git_config_set_multivar(argv[2], NULL, argv[3], 1);
		else if (!strcmp(argv[1], "--get"))
			return get_value(argv[2], argv[3]);
		else if (!strcmp(argv[1], "--get-all")) {
			do_all = 1;
			return get_value(argv[2], argv[3]);
		} else if (!strcmp(argv[1], "--get-regexp")) {
			show_keys = 1;
			use_key_regexp = 1;
			do_all = 1;
			return get_value(argv[2], argv[3]);
		} else if (!strcmp(argv[1], "--add")) {
			value = normalize_value(argv[2], argv[3]);
			return git_config_set_multivar(argv[2], value, "^$", 0);
		} else if (!strcmp(argv[1], "--replace-all")) {
			value = normalize_value(argv[2], argv[3]);
			return git_config_set_multivar(argv[2], value, NULL, 1);
		} else {
			value = normalize_value(argv[1], argv[2]);
			return git_config_set_multivar(argv[1], value, argv[3], 0);
		}
	case 5:
		if (!strcmp(argv[1], "--replace-all")) {
			value = normalize_value(argv[2], argv[3]);
			return git_config_set_multivar(argv[2], value, argv[4], 1);
		}
	case 1:
	default:
		usage(git_config_set_usage);
	}
	return 0;
}
