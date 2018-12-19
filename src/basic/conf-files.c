/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf-files.h"
#include "def.h"
#include "dirent-util.h"
#include "fd-util.h"
#include "hashmap.h"
#include "log.h"
#include "macro.h"
#include "missing.h"
#include "path-util.h"
#include "string-util.h"
#include "strv.h"
#include "terminal-util.h"
#include "util.h"

static int files_add(Hashmap *h, const char *root, const char *path, const char *suffix) {
        _cleanup_closedir_ DIR *dir = NULL;
        const char *dirpath;
        struct dirent *de;
        int r;

        assert(path);
        assert(suffix);

        dirpath = prefix_roota(root, path);

        dir = opendir(dirpath);
        if (!dir) {
                if (errno == ENOENT)
                        return 0;
                return -errno;
        }

        FOREACH_DIRENT(de, dir, return -errno) {
                char *p;

                if (!dirent_is_file_with_suffix(de, suffix))
                        continue;

                p = strjoin(dirpath, "/", de->d_name, NULL);
                if (!p)
                        return -ENOMEM;

                r = hashmap_put(h, basename(p), p);
                if (r == -EEXIST) {
                        log_debug("Skipping overridden file: %s.", p);
                        free(p);
                } else if (r < 0) {
                        free(p);
                        return r;
                } else if (r == 0) {
                        log_debug("Duplicate file %s", p);
                        free(p);
                }
        }

        return 0;
}

static int base_cmp(const void *a, const void *b) {
        const char *s1, *s2;

        s1 = *(char * const *)a;
        s2 = *(char * const *)b;
        return strcmp(basename(s1), basename(s2));
}

static int conf_files_list_strv_internal(char ***strv, const char *suffix, const char *root, char **dirs) {
        _cleanup_hashmap_free_ Hashmap *fh = NULL;
        char **files, **p;
        int r;

        assert(strv);
        assert(suffix);

        /* This alters the dirs string array */
        if (!path_strv_resolve_uniq(dirs, root))
                return -ENOMEM;

        fh = hashmap_new(&string_hash_ops);
        if (!fh)
                return -ENOMEM;

        STRV_FOREACH(p, dirs) {
                r = files_add(fh, root, *p, suffix);
                if (r == -ENOMEM)
                        return r;
                if (r < 0)
                        log_debug_errno(r, "Failed to search for files in %s, ignoring: %m", *p);
        }

        files = hashmap_get_strv(fh);
        if (!files)
                return -ENOMEM;

        qsort_safe(files, hashmap_size(fh), sizeof(char *), base_cmp);
        *strv = files;

        return 0;
}

int conf_files_insert(char ***strv, const char *root, const char *dirs, const char *path) {
        /* Insert a path into strv, at the place honouring the usual sorting rules:
         * - we first compare by the basename
         * - and then we compare by dirname, allowing just one file with the given
         *   basename.
         * This means that we will
         * - add a new entry if basename(path) was not on the list,
         * - do nothing if an entry with higher priority was already present,
         * - do nothing if our new entry matches the existing entry,
         * - replace the existing entry if our new entry has higher priority.
         */
        char *t;
        unsigned i;
        int r;

        for (i = 0; i < strv_length(*strv); i++) {
                int c;

                c = base_cmp(*strv + i, &path);
                if (c == 0) {
                        char **dir;

                        /* Oh, there already is an entry with a matching name (the last component). */

                        STRV_FOREACH(dir, dirs) {
                                _cleanup_free_ char *rdir = NULL;
                                char *p1, *p2;

                                rdir = prefix_root(root, *dir);
                                if (!rdir)
                                        return -ENOMEM;

                                p1 = path_startswith((*strv)[i], rdir);
                                if (p1)
                                        /* Existing entry with higher priority
                                         * or same priority, no need to do anything. */
                                        return 0;

                                p2 = path_startswith(path, *dir);
                                if (p2) {
                                        /* Our new entry has higher priority */

                                        t = prefix_root(root, path);
                                        if (!t)
                                                return log_oom();

                                        return free_and_replace((*strv)[i], t);
                                }
                        }

                } else if (c > 0)
                        /* Following files have lower priority, let's go insert our
                         * new entry. */
                        break;

                /* … we are not there yet, let's continue */
        }

        /* The new file has lower priority than all the existing entries */
        t = prefix_root(root, path);
        if (!t)
                return log_oom();

        r = strv_insert(strv, i, t);
        if (r < 0)
                free(t);
        return r;
}

int conf_files_insert_nulstr(char ***strv, const char *root, const char *dirs, const char *path) {
        _cleanup_strv_free_ char **d = NULL;

        assert(strv);

        d = strv_split_nulstr(dirs);
        if (!d)
                return -ENOMEM;

        return conf_files_insert(strv, root, d, path);
}

int conf_files_list_strv(char ***strv, const char *suffix, const char *root, const char* const* dirs) {
        _cleanup_strv_free_ char **copy = NULL;

        assert(strv);
        assert(suffix);

        copy = strv_copy((char**) dirs);
        if (!copy)
                return -ENOMEM;

        return conf_files_list_strv_internal(strv, suffix, root, copy);
}

int conf_files_list(char ***strv, const char *suffix, const char *root, const char *dir, ...) {
        _cleanup_strv_free_ char **dirs = NULL;
        va_list ap;

        assert(strv);
        assert(suffix);

        va_start(ap, dir);
        dirs = strv_new_ap(dir, ap);
        va_end(ap);

        if (!dirs)
                return -ENOMEM;

        return conf_files_list_strv_internal(strv, suffix, root, dirs);
}

int conf_files_list_nulstr(char ***strv, const char *suffix, const char *root, const char *dirs) {
        _cleanup_strv_free_ char **d = NULL;

        assert(strv);
        assert(suffix);

        d = strv_split_nulstr(dirs);
        if (!d)
                return -ENOMEM;

        return conf_files_list_strv_internal(strv, suffix, root, d);
}

int conf_files_list_with_replacement(
                const char *root,
                char **config_dirs,
                const char *replacement,
                char ***files,
                char **replace_file) {

        _cleanup_strv_free_ char **f = NULL;
        _cleanup_free_ char *p = NULL;
        int r;

        assert(config_dirs);
        assert(files);
        assert(replace_file || !replacement);

        r = conf_files_list_strv(&f, ".conf", root, (const char* const*) config_dirs);
        if (r < 0)
                return log_error_errno(r, "Failed to enumerate config files: %m");

        if (replacement) {
                r = conf_files_insert(&f, root, config_dirs, replacement);
                if (r < 0)
                        return log_error_errno(r, "Failed to extend config file list: %m");

                p = prefix_root(root, replacement);
                if (!p)
                        return log_oom();
        }

        *files = TAKE_PTR(f);
        if (replace_file)
                *replace_file = TAKE_PTR(p);
        return 0;
}

int conf_files_cat(const char *name) {
        _cleanup_strv_free_ char **dirs = NULL, **files = NULL;
        const char *dir;
        char **t;
        int r;

        NULSTR_FOREACH(dir, CONF_PATHS_NULSTR("")) {
                assert(endswith(dir, "/"));
                r = strv_extendf(&dirs, "%s%s.d", dir, name);
                if (r < 0)
                        return log_error("Failed to build directory list: %m");
        }

        r = conf_files_list_strv(&files, ".conf", NULL, (const char* const*) dirs);
        if (r < 0)
                return log_error_errno(r, "Failed to query file list: %m");

        name = strjoina("/etc/", name);

        /* show */
        return cat_files(name, files, CAT_FLAGS_MAIN_FILE_OPTIONAL);
}
