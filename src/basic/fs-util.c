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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <linux/magic.h>
#include "alloc-util.h"
#include "dirent-util.h"
#include "fd-util.h"
#include "fileio.h"
#include "fs-util.h"
#include "log.h"
#include "macro.h"
#include "missing.h"
#include "mkdir.h"
#include "parse-util.h"
#include "path-util.h"
#include "stdio-util.h"
#include "string-util.h"
#include "strv.h"
#include "time-util.h"
#include "user-util.h"
#include "util.h"

int unlink_noerrno(const char *path) {
        PROTECT_ERRNO;
        int r;

        r = unlink(path);
        if (r < 0)
                return -errno;

        return 0;
}

int rmdir_parents(const char *path, const char *stop) {
        size_t l;
        int r = 0;

        assert(path);
        assert(stop);

        l = strlen(path);

        /* Skip trailing slashes */
        while (l > 0 && path[l-1] == '/')
                l--;

        while (l > 0) {
                char *t;

                /* Skip last component */
                while (l > 0 && path[l-1] != '/')
                        l--;

                /* Skip trailing slashes */
                while (l > 0 && path[l-1] == '/')
                        l--;

                if (l <= 0)
                        break;

                t = strndup(path, l);
                if (!t)
                        return -ENOMEM;

                if (path_startswith(stop, t)) {
                        free(t);
                        return 0;
                }

                r = rmdir(t);
                free(t);

                if (r < 0)
                        if (errno != ENOENT)
                                return -errno;
        }

        return 0;
}


int rename_noreplace(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) {
        struct stat buf;
        int ret;

        ret = renameat2(olddirfd, oldpath, newdirfd, newpath, RENAME_NOREPLACE);
        if (ret >= 0)
                return 0;

        /* renameat2() exists since Linux 3.15, btrfs added support for it later.
         * If it is not implemented, fallback to another method. */
        if (!IN_SET(errno, EINVAL, ENOSYS))
                return -errno;

        /* The link()/unlink() fallback does not work on directories. But
         * renameat() without RENAME_NOREPLACE gives the same semantics on
         * directories, except when newpath is an *empty* directory. This is
         * good enough. */
        ret = fstatat(olddirfd, oldpath, &buf, AT_SYMLINK_NOFOLLOW);
        if (ret >= 0 && S_ISDIR(buf.st_mode)) {
                ret = renameat(olddirfd, oldpath, newdirfd, newpath);
                return ret >= 0 ? 0 : -errno;
        }

        /* If it is not a directory, use the link()/unlink() fallback. */
        ret = linkat(olddirfd, oldpath, newdirfd, newpath, 0);
        if (ret < 0)
                return -errno;

        ret = unlinkat(olddirfd, oldpath, 0);
        if (ret < 0) {
                /* backup errno before the following unlinkat() alters it */
                ret = errno;
                (void) unlinkat(newdirfd, newpath, 0);
                errno = ret;
                return -errno;
        }

        return 0;
}

int readlinkat_malloc(int fd, const char *p, char **ret) {
        size_t l = 100;
        int r;

        assert(p);
        assert(ret);

        for (;;) {
                char *c;
                ssize_t n;

                c = new(char, l);
                if (!c)
                        return -ENOMEM;

                n = readlinkat(fd, p, c, l-1);
                if (n < 0) {
                        r = -errno;
                        free(c);
                        return r;
                }

                if ((size_t) n < l-1) {
                        c[n] = 0;
                        *ret = c;
                        return 0;
                }

                free(c);
                l *= 2;
        }
}

int readlink_malloc(const char *p, char **ret) {
        return readlinkat_malloc(AT_FDCWD, p, ret);
}

int readlink_value(const char *p, char **ret) {
        _cleanup_free_ char *link = NULL;
        char *value;
        int r;

        r = readlink_malloc(p, &link);
        if (r < 0)
                return r;

        value = basename(link);
        if (!value)
                return -ENOENT;

        value = strdup(value);
        if (!value)
                return -ENOMEM;

        *ret = value;

        return 0;
}

int readlink_and_make_absolute(const char *p, char **r) {
        _cleanup_free_ char *target = NULL;
        char *k;
        int j;

        assert(p);
        assert(r);

        j = readlink_malloc(p, &target);
        if (j < 0)
                return j;

        k = file_in_same_dir(p, target);
        if (!k)
                return -ENOMEM;

        *r = k;
        return 0;
}

int readlink_and_canonicalize(const char *p, char **r) {
        char *t, *s;
        int j;

        assert(p);
        assert(r);

        j = readlink_and_make_absolute(p, &t);
        if (j < 0)
                return j;

        s = canonicalize_file_name(t);
        if (s) {
                free(t);
                *r = s;
        } else
                *r = t;

        path_kill_slashes(*r);

        return 0;
}

int readlink_and_make_absolute_root(const char *root, const char *path, char **ret) {
        _cleanup_free_ char *target = NULL, *t = NULL;
        const char *full;
        int r;

        full = prefix_roota(root, path);
        r = readlink_malloc(full, &target);
        if (r < 0)
                return r;

        t = file_in_same_dir(path, target);
        if (!t)
                return -ENOMEM;

        *ret = t;
        t = NULL;

        return 0;
}

int chmod_and_chown(const char *path, mode_t mode, uid_t uid, gid_t gid) {
        assert(path);

        /* Under the assumption that we are running privileged we
         * first change the access mode and only then hand out
         * ownership to avoid a window where access is too open. */

        if (mode != MODE_INVALID)
                if (chmod(path, mode) < 0)
                        return -errno;

        if (uid != UID_INVALID || gid != GID_INVALID)
                if (chown(path, uid, gid) < 0)
                        return -errno;

        return 0;
}

int fchmod_umask(int fd, mode_t m) {
        mode_t u;
        int r;

        u = umask(0777);
        r = fchmod(fd, m & (~u)) < 0 ? -errno : 0;
        umask(u);

        return r;
}

int fchmod_opath(int fd, mode_t m) {
        char procfs_path[strlen("/proc/self/fd/") + DECIMAL_STR_MAX(int)];

        /* This function operates also on fd that might have been opened with
         * O_PATH. Indeed fchmodat() doesn't have the AT_EMPTY_PATH flag like
         * fchownat() does. */

        xsprintf(procfs_path, "/proc/self/fd/%i", fd);

        if (chmod(procfs_path, m) < 0)
                return -errno;

        return 0;
}

int fd_warn_permissions(const char *path, int fd) {
        struct stat st;

        if (fstat(fd, &st) < 0)
                return -errno;

        if (st.st_mode & 0111)
                log_warning("Configuration file %s is marked executable. Please remove executable permission bits. Proceeding anyway.", path);

        if (st.st_mode & 0002)
                log_warning("Configuration file %s is marked world-writable. Please remove world writability permission bits. Proceeding anyway.", path);

        if (getpid() == 1 && (st.st_mode & 0044) != 0044)
                log_warning("Configuration file %s is marked world-inaccessible. This has no effect as configuration data is accessible via APIs without restrictions. Proceeding anyway.", path);

        return 0;
}

int touch_file(const char *path, bool parents, usec_t stamp, uid_t uid, gid_t gid, mode_t mode) {
        _cleanup_close_ int fd;
        int r;

        assert(path);

        if (parents)
                mkdir_parents(path, 0755);

        fd = open(path, O_WRONLY|O_CREAT|O_CLOEXEC|O_NOCTTY,
                        (mode == 0 || mode == MODE_INVALID) ? 0644 : mode);
        if (fd < 0)
                return -errno;

        if (mode != MODE_INVALID) {
                r = fchmod(fd, mode);
                if (r < 0)
                        return -errno;
        }

        if (uid != UID_INVALID || gid != GID_INVALID) {
                r = fchown(fd, uid, gid);
                if (r < 0)
                        return -errno;
        }

        if (stamp != USEC_INFINITY) {
                struct timespec ts[2];

                timespec_store(&ts[0], stamp);
                ts[1] = ts[0];
                r = futimens(fd, ts);
        } else
                r = futimens(fd, NULL);
        if (r < 0)
                return -errno;

        return 0;
}

int touch(const char *path) {
        return touch_file(path, false, USEC_INFINITY, UID_INVALID, GID_INVALID, MODE_INVALID);
}

int symlink_idempotent(const char *from, const char *to) {
        _cleanup_free_ char *p = NULL;
        int r;

        assert(from);
        assert(to);

        if (symlink(from, to) < 0) {
                if (errno != EEXIST)
                        return -errno;

                r = readlink_malloc(to, &p);
                if (r < 0)
                        return r;

                if (!streq(p, from))
                        return -EINVAL;
        }

        return 0;
}

int symlink_atomic(const char *from, const char *to) {
        _cleanup_free_ char *t = NULL;
        int r;

        assert(from);
        assert(to);

        r = tempfn_random(to, NULL, &t);
        if (r < 0)
                return r;

        if (symlink(from, t) < 0)
                return -errno;

        if (rename(t, to) < 0) {
                unlink_noerrno(t);
                return -errno;
        }

        return 0;
}

int mknod_atomic(const char *path, mode_t mode, dev_t dev) {
        _cleanup_free_ char *t = NULL;
        int r;

        assert(path);

        r = tempfn_random(path, NULL, &t);
        if (r < 0)
                return r;

        if (mknod(t, mode, dev) < 0)
                return -errno;

        if (rename(t, path) < 0) {
                unlink_noerrno(t);
                return -errno;
        }

        return 0;
}

int mkfifo_atomic(const char *path, mode_t mode) {
        _cleanup_free_ char *t = NULL;
        int r;

        assert(path);

        r = tempfn_random(path, NULL, &t);
        if (r < 0)
                return r;

        if (mkfifo(t, mode) < 0)
                return -errno;

        if (rename(t, path) < 0) {
                unlink_noerrno(t);
                return -errno;
        }

        return 0;
}

int get_files_in_directory(const char *path, char ***list) {
        _cleanup_closedir_ DIR *d = NULL;
        size_t bufsize = 0, n = 0;
        _cleanup_strv_free_ char **l = NULL;

        assert(path);

        /* Returns all files in a directory in *list, and the number
         * of files as return value. If list is NULL returns only the
         * number. */

        d = opendir(path);
        if (!d)
                return -errno;

        for (;;) {
                struct dirent *de;

                errno = 0;
                de = readdir(d);
                if (!de && errno > 0)
                        return -errno;
                if (!de)
                        break;

                dirent_ensure_type(d, de);

                if (!dirent_is_file(de))
                        continue;

                if (list) {
                        /* one extra slot is needed for the terminating NULL */
                        if (!GREEDY_REALLOC(l, bufsize, n + 2))
                                return -ENOMEM;

                        l[n] = strdup(de->d_name);
                        if (!l[n])
                                return -ENOMEM;

                        l[++n] = NULL;
                } else
                        n++;
        }

        if (list) {
                *list = l;
                l = NULL; /* avoid freeing */
        }

        return n;
}

int inotify_add_watch_fd(int fd, int what, uint32_t mask) {
        char path[strlen("/proc/self/fd/") + DECIMAL_STR_MAX(int) + 1];
        int r;

        /* This is like inotify_add_watch(), except that the file to watch is not referenced by a path, but by an fd */
        xsprintf(path, "/proc/self/fd/%i", what);

        r = inotify_add_watch(fd, path, mask);
        if (r < 0)
                return -errno;

        return r;
}

static bool safe_transition(const struct stat *a, const struct stat *b) {
        /* Returns true if the transition from a to b is safe, i.e. that we never transition from unprivileged to
         * privileged files or directories. Why bother? So that unprivileged code can't symlink to privileged files
         * making us believe we read something safe even though it isn't safe in the specific context we open it in. */

        if (a->st_uid == 0) /* Transitioning from privileged to unprivileged is always fine */
                return true;

        return a->st_uid == b->st_uid; /* Otherwise we need to stay within the same UID */
}

int chase_symlinks(const char *path, const char *original_root, unsigned flags, char **ret) {
        _cleanup_free_ char *buffer = NULL, *done = NULL, *root = NULL;
        _cleanup_close_ int fd = -1;
        unsigned max_follow = 32; /* how many symlinks to follow before giving up and returning ELOOP */
        struct stat previous_stat;
        bool exists = true;
        char *todo;
        int r;

        assert(path);

        /* Either the file may be missing, or we return an fd to the final object, but both make no sense */
        if ((flags & (CHASE_NONEXISTENT|CHASE_OPEN)) == (CHASE_NONEXISTENT|CHASE_OPEN))
                return -EINVAL;

        if ((flags & (CHASE_STEP|CHASE_OPEN)) == (CHASE_STEP|CHASE_OPEN))
                return -EINVAL;

        if (isempty(path))
                return -EINVAL;

        /* This is a lot like canonicalize_file_name(), but takes an additional "root" parameter, that allows following
         * symlinks relative to a root directory, instead of the root of the host.
         *
         * Note that "root" primarily matters if we encounter an absolute symlink. It is also used when following
         * relative symlinks to ensure they cannot be used to "escape" the root directory. The path parameter passed is
         * assumed to be already prefixed by it, except if the CHASE_PREFIX_ROOT flag is set, in which case it is first
         * prefixed accordingly.
         *
         * Algorithmically this operates on two path buffers: "done" are the components of the path we already
         * processed and resolved symlinks, "." and ".." of. "todo" are the components of the path we still need to
         * process. On each iteration, we move one component from "todo" to "done", processing it's special meaning
         * each time. The "todo" path always starts with at least one slash, the "done" path always ends in no
         * slash. We always keep an O_PATH fd to the component we are currently processing, thus keeping lookup races
         * at a minimum.
         *
         * Suggested usage: whenever you want to canonicalize a path, use this function. Pass the absolute path you got
         * as-is: fully qualified and relative to your host's root. Optionally, specify the root parameter to tell this
         * function what to do when encountering a symlink with an absolute path as directory: prefix it by the
         * specified path.
         *
         * There are three ways to invoke this function:
         *
         * 1. Without CHASE_STEP or CHASE_OPEN: in this case the path is resolved and the normalized path is returned
         *    in `ret`. The return value is < 0 on error. If CHASE_NONEXISTENT is also set 0 is returned if the file
         *    doesn't exist, > 0 otherwise. If CHASE_NONEXISTENT is not set >= 0 is returned if the destination was
         *    found, -ENOENT if it doesn't.
         *
         * 2. With CHASE_OPEN: in this case the destination is opened after chasing it as O_PATH and this file
         *    descriptor is returned as return value. This is useful to open files relative to some root
         *    directory. Note that the returned O_PATH file descriptors must be converted into a regular one (using
         *    fd_reopen() or such) before it can be used for reading/writing. CHASE_OPEN may not be combined with
         *    CHASE_NONEXISTENT.
         *
         * 3. With CHASE_STEP: in this case only a single step of the normalization is executed, i.e. only the first
         *    symlink or ".." component of the path is resolved, and the resulting path is returned. This is useful if
         *    a caller wants to trace the a path through the file system verbosely. Returns < 0 on error, > 0 if the
         *    path is fully normalized, and == 0 for each normalization step. This may be combined with
         *    CHASE_NONEXISTENT, in which case 1 is returned when a component is not found.
         *
         * */

        /* A root directory of "/" or "" is identical to none */
        if (isempty(original_root) || path_equal(original_root, "/"))
                original_root = NULL;

        if (!original_root && !ret && (flags & (CHASE_NONEXISTENT|CHASE_SAFE|CHASE_OPEN|CHASE_STEP)) == CHASE_OPEN) {
                /* Shortcut the CHASE_OPEN case if the caller isn't interested in the actual path and has no root set
                 * and doesn't care about any of the other special features we provide either. */
                r = open(path, O_PATH|O_CLOEXEC|((flags & CHASE_NOFOLLOW) ? O_NOFOLLOW : 0));
                if (r < 0)
                        return -errno;

                return r;
        }

        if (original_root) {
                r = path_make_absolute_cwd(original_root, &root);
                if (r < 0)
                        return r;

                if (flags & CHASE_PREFIX_ROOT) {

                        /* We don't support relative paths in combination with a root directory */
                        if (!path_is_absolute(path))
                                return -EINVAL;

                        path = prefix_roota(root, path);
                }
        }

        r = path_make_absolute_cwd(path, &buffer);
        if (r < 0)
                return r;

        fd = open("/", O_CLOEXEC|O_NOFOLLOW|O_PATH);
        if (fd < 0)
                return -errno;

        if (flags & CHASE_SAFE) {
                if (fstat(fd, &previous_stat) < 0)
                        return -errno;
        }

        todo = buffer;
        for (;;) {
                _cleanup_free_ char *first = NULL;
                _cleanup_close_ int child = -1;
                struct stat st;
                size_t n, m;

                /* Determine length of first component in the path */
                n = strspn(todo, "/");                  /* The slashes */
                m = n + strcspn(todo + n, "/");         /* The entire length of the component */

                /* Extract the first component. */
                first = strndup(todo, m);
                if (!first)
                        return -ENOMEM;

                todo += m;

                /* Just a single slash? Then we reached the end. */
                if (isempty(first) || path_equal(first, "/"))
                        break;

                /* Just a dot? Then let's eat this up. */
                if (path_equal(first, "/."))
                        continue;

                /* Two dots? Then chop off the last bit of what we already found out. */
                if (path_equal(first, "/..")) {
                        _cleanup_free_ char *parent = NULL;
                        _cleanup_close_ int fd_parent = -1;

                        /* If we already are at the top, then going up will not change anything. This is in-line with
                         * how the kernel handles this. */
                        if (isempty(done) || path_equal(done, "/"))
                                continue;

                        parent = dirname_malloc(done);
                        if (!parent)
                                return -ENOMEM;

                        /* Don't allow this to leave the root dir.  */
                        if (root &&
                            path_startswith(done, root) &&
                            !path_startswith(parent, root))
                                continue;

                        /* fbui: was originally "free_and_replace(done, parent);" */
                        free(done);
                        done = parent;
                        parent = NULL;

                        if (flags & CHASE_STEP)
                                goto chased_one;

                        fd_parent = openat(fd, "..", O_CLOEXEC|O_NOFOLLOW|O_PATH);
                        if (fd_parent < 0)
                                return -errno;

                        if (flags & CHASE_SAFE) {
                                if (fstat(fd_parent, &st) < 0)
                                        return -errno;

                                if (!safe_transition(&previous_stat, &st))
                                        return -EPERM;

                                previous_stat = st;
                        }

                        safe_close(fd);
                        fd = fd_parent;
                        fd_parent = -1;

                        continue;
                }

                /* Otherwise let's see what this is. */
                child = openat(fd, first + n, O_CLOEXEC|O_NOFOLLOW|O_PATH);
                if (child < 0) {

                        if (errno == ENOENT &&
                            (flags & CHASE_NONEXISTENT) &&
                            (isempty(todo) || path_is_safe(todo))) {

                                /* If CHASE_NONEXISTENT is set, and the path does not exist, then that's OK, return
                                 * what we got so far. But don't allow this if the remaining path contains "../ or "./"
                                 * or something else weird. */

                                /* If done is "/", as first also contains slash at the head, then remove this redundant slash. */
                                if (streq_ptr(done, "/"))
                                        *done = '\0';

                                if (!strextend(&done, first, todo, NULL))
                                        return -ENOMEM;

                                exists = false;
                                break;
                        }

                        return -errno;
                }

                if (fstat(child, &st) < 0)
                        return -errno;

                if ((flags & CHASE_SAFE) &&
                    !safe_transition(&previous_stat, &st))
                        return -EPERM;

                previous_stat = st;

                if (S_ISLNK(st.st_mode) && !((flags & CHASE_NOFOLLOW) && isempty(todo))) {
                        char *joined;

                        _cleanup_free_ char *destination = NULL;

                        /* This is a symlink, in this case read the destination. But let's make sure we don't follow
                         * symlinks without bounds. */
                        if (--max_follow <= 0)
                                return -ELOOP;

                        r = readlinkat_malloc(fd, first + n, &destination);
                        if (r < 0)
                                return r;
                        if (isempty(destination))
                                return -EINVAL;

                        if (path_is_absolute(destination)) {

                                /* An absolute destination. Start the loop from the beginning, but use the root
                                 * directory as base. */

                                safe_close(fd);
                                fd = open(root ?: "/", O_CLOEXEC|O_NOFOLLOW|O_PATH);
                                if (fd < 0)
                                        return -errno;

                                if (flags & CHASE_SAFE) {
                                        if (fstat(fd, &st) < 0)
                                                return -errno;

                                        if (!safe_transition(&previous_stat, &st))
                                                return -EPERM;

                                        previous_stat = st;
                                }

                                free(done);

                                /* Note that we do not revalidate the root, we take it as is. */
                                if (isempty(root))
                                        done = NULL;
                                else {
                                        done = strdup(root);
                                        if (!done)
                                                return -ENOMEM;
                                }

                                /* Prefix what's left to do with what we just read, and start the loop again, but
                                 * remain in the current directory. */
                                /* fbui: added the NULL sentinel */
                                joined = strjoin(destination, todo, NULL);
                        } else
                                joined = strjoin("/", destination, todo, NULL);
                        if (!joined)
                                return -ENOMEM;

                        free(buffer);
                        todo = buffer = joined;

                        if (flags & CHASE_STEP)
                                goto chased_one;

                        continue;
                }

                /* If this is not a symlink, then let's just add the name we read to what we already verified. */
                if (!done) {
                        done = first;
                        first = NULL;
                } else {
                        /* If done is "/", as first also contains slash at the head, then remove this redundant slash. */
                        if (streq(done, "/"))
                                *done = '\0';

                        if (!strextend(&done, first, NULL))
                                return -ENOMEM;
                }

                /* And iterate again, but go one directory further down. */
                safe_close(fd);
                fd = child;
                child = -1;
        }

        if (!done) {
                /* Special case, turn the empty string into "/", to indicate the root directory. */
                done = strdup("/");
                if (!done)
                        return -ENOMEM;
        }

        if (ret) {
                *ret = done;
                done = NULL;
        }

        if (flags & CHASE_OPEN) {
                /* Return the O_PATH fd we currently are looking to the caller. It can translate it to a proper fd by
                 * opening /proc/self/fd/xyz. */

                assert(fd >= 0);

                return TAKE_FD(fd);
        }

        if (flags & CHASE_STEP)
                return 1;

        return exists;

chased_one:
        if (ret) {
                char *c;

                c = strjoin(strempty(done), todo);
                if (!c)
                        return -ENOMEM;

                *ret = c;
        }

        return 0;
}
