/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/* Missing glibc definitions to access certain kernel APIs */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef ARCH_MIPS
#include <asm/sgidefs.h>
#endif

#if defined(__x86_64__) && defined(__ILP32__)
#  define systemd_SC_arch_bias(x) ((x) | /* __X32_SYSCALL_BIT */ 0x40000000)
#elif defined(__ia64__)
#  define systemd_SC_arch_bias(x) (1024 + (x))
#elif defined __alpha__
#  define systemd_SC_arch_bias(x) (110 + (x))
#elif defined _MIPS_SIM
#  if _MIPS_SIM == _MIPS_SIM_ABI32
#    define systemd_SC_arch_bias(x) (4000 + (x))
#  elif _MIPS_SIM == _MIPS_SIM_NABI32
#    define systemd_SC_arch_bias(x) (6000 + (x))
#  elif _MIPS_SIM == _MIPS_SIM_ABI64
#    define systemd_SC_arch_bias(x) (5000 + (x))
#  else
#    error "Unknown MIPS ABI"
#  endif
#else
#  define systemd_SC_arch_bias(x) (x)
#endif

#include "missing_keyctl.h"
#include "missing_stat.h"

/* linux/kcmp.h */
#ifndef KCMP_FILE /* 3f4994cfc15f38a3159c6e3a4b3ab2e1481a6b02 (3.19) */
#define KCMP_FILE 0
#endif

#if !HAVE_PIVOT_ROOT
static inline int missing_pivot_root(const char *new_root, const char *put_old) {
        return syscall(__NR_pivot_root, new_root, put_old);
}

#  define pivot_root missing_pivot_root
#endif

/* ======================================================================= */

#if !HAVE_MEMFD_CREATE
/* may be (invalid) negative number due to libseccomp, see PR 13319 */
#  if ! (defined __NR_memfd_create && __NR_memfd_create >= 0)
#    if defined __NR_memfd_create
#      undef __NR_memfd_create
#    endif
#    if defined __x86_64__
#      define __NR_memfd_create systemd_SC_arch_bias(319)
#    elif defined __arm__
#      define __NR_memfd_create 385
#    elif defined __aarch64__
#      define __NR_memfd_create 279
#    elif defined __s390__
#      define __NR_memfd_create 350
#    elif defined _MIPS_SIM
#      if _MIPS_SIM == _MIPS_SIM_ABI32
#        define __NR_memfd_create 4354
#      endif
#      if _MIPS_SIM == _MIPS_SIM_NABI32
#        define __NR_memfd_create 6318
#      endif
#      if _MIPS_SIM == _MIPS_SIM_ABI64
#        define __NR_memfd_create 5314
#      endif
#    elif defined __i386__
#      define __NR_memfd_create 356
#    elif defined __arc__
#      define __NR_memfd_create 279
#    else
#      warning "__NR_memfd_create unknown for your architecture"
#    endif
#  endif

static inline int missing_memfd_create(const char *name, unsigned int flags) {
#  ifdef __NR_memfd_create
        return syscall(__NR_memfd_create, name, flags);
#  else
        errno = ENOSYS;
        return -1;
#  endif
}

#  define memfd_create missing_memfd_create
#endif

/* ======================================================================= */

#if !HAVE_GETRANDOM
/* may be (invalid) negative number due to libseccomp, see PR 13319 */
#  if ! (defined __NR_getrandom && __NR_getrandom >= 0)
#    if defined __NR_getrandom
#      undef __NR_getrandom
#    endif
#    if defined __x86_64__
#      define __NR_getrandom systemd_SC_arch_bias(318)
#    elif defined(__i386__)
#      define __NR_getrandom 355
#    elif defined(__arm__)
#      define __NR_getrandom 384
#   elif defined(__aarch64__)
#      define __NR_getrandom 278
#    elif defined(__ia64__)
#      define __NR_getrandom 1339
#    elif defined(__m68k__)
#      define __NR_getrandom 352
#    elif defined(__s390x__)
#      define __NR_getrandom 349
#    elif defined(__powerpc__)
#      define __NR_getrandom 359
#    elif defined _MIPS_SIM
#      if _MIPS_SIM == _MIPS_SIM_ABI32
#        define __NR_getrandom 4353
#      endif
#      if _MIPS_SIM == _MIPS_SIM_NABI32
#        define __NR_getrandom 6317
#      endif
#      if _MIPS_SIM == _MIPS_SIM_ABI64
#        define __NR_getrandom 5313
#      endif
#    elif defined(__arc__)
#      define __NR_getrandom 278
#    else
#      warning "__NR_getrandom unknown for your architecture"
#    endif
#  endif

static inline int missing_getrandom(void *buffer, size_t count, unsigned flags) {
#  ifdef __NR_getrandom
        return syscall(__NR_getrandom, buffer, count, flags);
#  else
        errno = ENOSYS;
        return -1;
#  endif
}

#  define getrandom missing_getrandom
#endif

/* ======================================================================= */

#if !HAVE_GETTID
static inline pid_t missing_gettid(void) {
        return (pid_t) syscall(__NR_gettid);
}

#  define gettid missing_gettid
#endif

/* ======================================================================= */

#if !HAVE_NAME_TO_HANDLE_AT
/* may be (invalid) negative number due to libseccomp, see PR 13319 */
#  if ! (defined __NR_name_to_handle_at && __NR_name_to_handle_at >= 0)
#    if defined __NR_name_to_handle_at
#      undef __NR_name_to_handle_at
#    endif
#    if defined(__x86_64__)
#      define __NR_name_to_handle_at systemd_SC_arch_bias(303)
#    elif defined(__i386__)
#      define __NR_name_to_handle_at 341
#    elif defined(__arm__)
#      define __NR_name_to_handle_at 370
#    elif defined(__powerpc__)
#      define __NR_name_to_handle_at 345
#    elif defined(__arc__)
#      define __NR_name_to_handle_at 264
#    elif defined _MIPS_SIM
#      if _MIPS_SIM == _MIPS_SIM_ABI32
#        define systemd_NR_name_to_handle_at systemd_SC_arch_bias(339)
#      elif _MIPS_SIM == _MIPS_SIM_NABI32
#        define systemd_NR_name_to_handle_at systemd_SC_arch_bias(303)
#      elif _MIPS_SIM == _MIPS_SIM_ABI64
#        define systemd_NR_name_to_handle_at systemd_SC_arch_bias(298)
#      endif
#    else
#      error "__NR_name_to_handle_at is not defined"
#    endif
#  endif

struct file_handle {
        unsigned int handle_bytes;
        int handle_type;
        unsigned char f_handle[0];
};

static inline int missing_name_to_handle_at(int fd, const char *name, struct file_handle *handle, int *mnt_id, int flags) {
#  ifdef __NR_name_to_handle_at
        return syscall(__NR_name_to_handle_at, fd, name, handle, mnt_id, flags);
#  else
        errno = ENOSYS;
        return -1;
#  endif
}

#  define name_to_handle_at missing_name_to_handle_at
#endif

/* ======================================================================= */

#if !HAVE_SETNS
/* may be (invalid) negative number due to libseccomp, see PR 13319 */
#  if ! (defined __NR_setns && __NR_setns >= 0)
#    if defined __NR_setns
#      undef __NR_setns
#    endif
#    if defined(__x86_64__)
#      define __NR_setns systemd_SC_arch_bias(308)
#    elif defined(__i386__)
#      define __NR_setns 346
#    elif defined(__arc__)
#      define __NR_setns 268
#    elif defined _MIPS_SIM
#      if _MIPS_SIM == _MIPS_SIM_ABI32
#        define systemd_NR_setns systemd_SC_arch_bias(344)
#      elif _MIPS_SIM == _MIPS_SIM_NABI32
#        define systemd_NR_setns systemd_SC_arch_bias(308)
#      elif _MIPS_SIM == _MIPS_SIM_ABI64
#        define systemd_NR_setns systemd_SC_arch_bias(303)
#      endif
#    else
#      error "__NR_setns is not defined"
#    endif
#  endif

static inline int missing_setns(int fd, int nstype) {
#  ifdef __NR_setns
        return syscall(__NR_setns, fd, nstype);
#  else
        errno = ENOSYS;
        return -1;
#  endif
}

#  define setns missing_setns
#endif

/* ======================================================================= */

static inline pid_t raw_getpid(void) {
#if defined(__alpha__)
        return (pid_t) syscall(__NR_getxpid);
#else
        return (pid_t) syscall(__NR_getpid);
#endif
}

/* ======================================================================= */

#if !HAVE_RENAMEAT2
/* may be (invalid) negative number due to libseccomp, see PR 13319 */
#  if ! (defined __NR_renameat2 && __NR_renameat2 >= 0)
#    if defined __NR_renameat2
#      undef __NR_renameat2
#    endif
#    if defined __x86_64__
#      define __NR_renameat2 systemd_SC_arch_bias(316)
#    elif defined __arm__
#      define __NR_renameat2 382
#    elif defined __aarch64__
#      define __NR_renameat2 276
#    elif defined _MIPS_SIM
#      if _MIPS_SIM == _MIPS_SIM_ABI32
#        define __NR_renameat2 4351
#      endif
#      if _MIPS_SIM == _MIPS_SIM_NABI32
#        define __NR_renameat2 6315
#      endif
#      if _MIPS_SIM == _MIPS_SIM_ABI64
#        define __NR_renameat2 5311
#      endif
#    elif defined __i386__
#      define __NR_renameat2 353
#    elif defined __powerpc64__
#      define __NR_renameat2 357
#    elif defined __s390__ || defined __s390x__
#      define __NR_renameat2 347
#    elif defined __arc__
#      define __NR_renameat2 276
#    else
#      warning "__NR_renameat2 unknown for your architecture"
#    endif
#  endif

static inline int missing_renameat2(int oldfd, const char *oldname, int newfd, const char *newname, unsigned flags) {
#  ifdef __NR_renameat2
        return syscall(__NR_renameat2, oldfd, oldname, newfd, newname, flags);
#  else
        errno = ENOSYS;
        return -1;
#  endif
}

#  define renameat2 missing_renameat2
#endif

/* ======================================================================= */

#if !HAVE_KCMP
static inline int missing_kcmp(pid_t pid1, pid_t pid2, int type, unsigned long idx1, unsigned long idx2) {
#  if defined __NR_kcmp && __NR_kcmp >= 0
        return syscall(__NR_kcmp, pid1, pid2, type, idx1, idx2);
#  else
        errno = ENOSYS;
        return -1;
#  endif
}

#  define kcmp missing_kcmp
#endif

/* ======================================================================= */

#if !HAVE_KEYCTL
static inline long missing_keyctl(int cmd, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) {
#  if defined __NR_keyctl && __NR_keyctl >= 0
        return syscall(__NR_keyctl, cmd, arg2, arg3, arg4, arg5);
#  else
        errno = ENOSYS;
        return -1;
#  endif

#  define keyctl missing_keyctl
}

static inline key_serial_t missing_add_key(const char *type, const char *description, const void *payload, size_t plen, key_serial_t ringid) {
#  if defined __NR_add_key && __NR_add_key >= 0
        return syscall(__NR_add_key, type, description, payload, plen, ringid);
#  else
        errno = ENOSYS;
        return -1;
#  endif

#  define add_key missing_add_key
}

static inline key_serial_t missing_request_key(const char *type, const char *description, const char * callout_info, key_serial_t destringid) {
#  if defined __NR_request_key && __NR_request_key >= 0
        return syscall(__NR_request_key, type, description, callout_info, destringid);
#  else
        errno = ENOSYS;
        return -1;
#  endif

#  define request_key missing_request_key
}
#endif

/* ======================================================================= */

#if !HAVE_COPY_FILE_RANGE
/* may be (invalid) negative number due to libseccomp, see PR 13319 */
#  if ! (defined __NR_copy_file_range && __NR_copy_file_range >= 0)
#    if defined __NR_copy_file_range
#      undef __NR_copy_file_range
#    endif
#    if defined(__x86_64__)
#      define __NR_copy_file_range systemd_SC_arch_bias(326)
#    elif defined(__i386__)
#      define __NR_copy_file_range 377
#    elif defined __s390__
#      define __NR_copy_file_range 375
#    elif defined __arm__
#      define __NR_copy_file_range 391
#    elif defined __aarch64__
#      define __NR_copy_file_range 285
#    elif defined __powerpc__
#      define __NR_copy_file_range 379
#    elif defined __arc__
#      define __NR_copy_file_range 285
#    elif defined _MIPS_SIM
#      if _MIPS_SIM == _MIPS_SIM_ABI32
#        define systemd_NR_copy_file_range systemd_SC_arch_bias(360)
#      elif _MIPS_SIM == _MIPS_SIM_NABI32
#        define systemd_NR_copy_file_range systemd_SC_arch_bias(324)
#      elif _MIPS_SIM == _MIPS_SIM_ABI64
#        define systemd_NR_copy_file_range systemd_SC_arch_bias(320)
#      endif
#    else
#      warning "__NR_copy_file_range not defined for your architecture"
#    endif
#  endif

static inline ssize_t missing_copy_file_range(int fd_in, loff_t *off_in,
                                              int fd_out, loff_t *off_out,
                                              size_t len,
                                              unsigned int flags) {
#  ifdef __NR_copy_file_range
        return syscall(__NR_copy_file_range, fd_in, off_in, fd_out, off_out, len, flags);
#  else
        errno = ENOSYS;
        return -1;
#  endif
}

#  define copy_file_range missing_copy_file_range
#endif

/* ======================================================================= */

#if !HAVE_BPF
/* may be (invalid) negative number due to libseccomp, see PR 13319 */
#  if ! (defined __NR_bpf && __NR_bpf >= 0)
#    if defined __NR_bpf
#      undef __NR_bpf
#    endif
#    if defined __i386__
#      define __NR_bpf 357
#    elif defined __x86_64__
#      define __NR_bpf systemd_SC_arch_bias(321)
#    elif defined __aarch64__
#      define __NR_bpf 280
#    elif defined __arm__
#      define __NR_bpf 386
#    elif defined __sparc__
#      define __NR_bpf 349
#    elif defined __s390__
#      define __NR_bpf 351
#    elif defined __tilegx__
#      define __NR_bpf 280
#    elif defined _MIPS_SIM
#      if _MIPS_SIM == _MIPS_SIM_ABI32
#        define systemd_NR_bpf systemd_SC_arch_bias(355)
#      elif _MIPS_SIM == _MIPS_SIM_NABI32
#        define systemd_NR_bpf systemd_SC_arch_bias(319)
#      elif _MIPS_SIM == _MIPS_SIM_ABI64
#        define systemd_NR_bpf systemd_SC_arch_bias(315)
#      endif
#    else
#      warning "__NR_bpf not defined for your architecture"
#    endif
#  endif

union bpf_attr;

static inline int missing_bpf(int cmd, union bpf_attr *attr, size_t size) {
#ifdef __NR_bpf
        return (int) syscall(__NR_bpf, cmd, attr, size);
#else
        errno = ENOSYS;
        return -1;
#endif
}

#  define bpf missing_bpf
#endif

/* ======================================================================= */

#ifndef __IGNORE_pkey_mprotect
/* may be (invalid) negative number due to libseccomp, see PR 13319 */
#  if ! (defined __NR_pkey_mprotect && __NR_pkey_mprotect >= 0)
#    if defined __NR_pkey_mprotect
#      undef __NR_pkey_mprotect
#    endif
#    if defined __i386__
#      define __NR_pkey_mprotect 380
#    elif defined __x86_64__
#      define __NR_pkey_mprotect systemd_SC_arch_bias(329)
#    elif defined __arm__
#      define __NR_pkey_mprotect 394
#    elif defined __aarch64__
#      define __NR_pkey_mprotect 288
#    elif defined __powerpc__
#      define __NR_pkey_mprotect 386
#    elif defined __s390__
#      define __NR_pkey_mprotect 384
#    elif defined _MIPS_SIM
#      if _MIPS_SIM == _MIPS_SIM_ABI32
#        define __NR_pkey_mprotect 4363
#      endif
#      if _MIPS_SIM == _MIPS_SIM_NABI32
#        define __NR_pkey_mprotect 6327
#      endif
#      if _MIPS_SIM == _MIPS_SIM_ABI64
#        define __NR_pkey_mprotect 5323
#      endif
#    else
#      warning "__NR_pkey_mprotect not defined for your architecture"
#    endif
#  endif
#endif

/* ======================================================================= */

#if !HAVE_STATX
/* may be (invalid) negative number due to libseccomp, see PR 13319 */
#  if ! (defined __NR_statx && __NR_statx >= 0)
#    if defined __NR_statx
#      undef __NR_statx
#    endif
#    if defined __aarch64__
#      define __NR_statx 291
#    elif defined __arm__
#      define __NR_statx 397
#    elif defined __alpha__
#      define __NR_statx 522
#    elif defined __i386__ || defined __powerpc64__
#      define __NR_statx 383
#    elif defined __sparc__
#      define __NR_statx 360
#    elif defined __x86_64__
#      define __NR_statx systemd_SC_arch_bias(332)
#    elif defined _MIPS_SIM
#      if _MIPS_SIM == _MIPS_SIM_ABI32
#        define systemd_NR_statx systemd_SC_arch_bias(366)
#      elif _MIPS_SIM == _MIPS_SIM_NABI32
#        define systemd_NR_statx systemd_SC_arch_bias(330)
#      elif _MIPS_SIM == _MIPS_SIM_ABI64
#        define systemd_NR_statx systemd_SC_arch_bias(326)
#      endif
#    else
#      warning "__NR_statx not defined for your architecture"
#    endif
#  endif

struct statx;
#endif

/* This typedef is supposed to be always defined. */
typedef struct statx struct_statx;

#if !HAVE_STATX
static inline ssize_t missing_statx(int dfd, const char *filename, unsigned flags, unsigned int mask, struct statx *buffer) {
#  ifdef __NR_statx
        return syscall(__NR_statx, dfd, filename, flags, mask, buffer);
#  else
        errno = ENOSYS;
        return -1;
#  endif
}

#  define statx missing_statx
#endif

#if !HAVE_SET_MEMPOLICY

enum {
        MPOL_DEFAULT,
        MPOL_PREFERRED,
        MPOL_BIND,
        MPOL_INTERLEAVE,
        MPOL_LOCAL,
};

static inline long missing_set_mempolicy(int mode, const unsigned long *nodemask,
                           unsigned long maxnode) {
        long i;
#  if defined __NR_set_mempolicy && __NR_set_mempolicy >= 0
        i = syscall(__NR_set_mempolicy, mode, nodemask, maxnode);
#  else
        errno = ENOSYS;
        i = -1;
#  endif
        return i;
}

#  define set_mempolicy missing_set_mempolicy
#endif

#if !HAVE_GET_MEMPOLICY
static inline long missing_get_mempolicy(int *mode, unsigned long *nodemask,
                           unsigned long maxnode, void *addr,
                           unsigned long flags) {
        long i;
#  ifdef __NR_get_mempolicy
        i = syscall(__NR_get_mempolicy, mode, nodemask, maxnode, addr, flags);
#  else
        errno = ENOSYS;
        i = -1;
#  endif
        return i;
}

#  define get_mempolicy missing_get_mempolicy
#endif

#if !HAVE_PIDFD_SEND_SIGNAL
/* may be (invalid) negative number due to libseccomp, see PR 13319 */
#  if ! (defined __NR_pidfd_send_signal && __NR_pidfd_send_signal >= 0)
#    if defined __NR_pidfd_send_signal
#      undef __NR_pidfd_send_signal
#    endif
/* should be always defined, see kernel 39036cd2727395c3369b1051005da74059a85317 */
#    if defined(__alpha__)
#      define __NR_pidfd_send_signal 534
#    elif defined _MIPS_SIM
#      if _MIPS_SIM == _MIPS_SIM_ABI32  /* o32 */
#        define systemd_NR_pidfd_send_signal (424 + 4000)
#      endif
#      if _MIPS_SIM == _MIPS_SIM_NABI32 /* n32 */
#        define systemd_NR_pidfd_send_signal (424 + 6000)
#      endif
#      if _MIPS_SIM == _MIPS_SIM_ABI64  /* n64 */
#        define systemd_NR_pidfd_send_signal (424 + 5000)
#      endif
#    elif defined __ia64__
#      define systemd_NR_pidfd_send_signal (424 + 1024)
#    else
#      define __NR_pidfd_send_signal systemd_SC_arch_bias(424)
#    endif
#  endif
static inline int missing_pidfd_send_signal(int fd, int sig, siginfo_t *info, unsigned flags) {
#  ifdef __NR_pidfd_send_signal
        return syscall(__NR_pidfd_send_signal, fd, sig, info, flags);
#  else
        errno = ENOSYS;
        return -1;
#  endif
}

#  define pidfd_send_signal missing_pidfd_send_signal
#endif

#if !HAVE_PIDFD_OPEN
/* may be (invalid) negative number due to libseccomp, see PR 13319 */
#  if ! (defined __NR_pidfd_open && __NR_pidfd_open >= 0)
#    if defined __NR_pidfd_open
#      undef __NR_pidfd_open
#    endif
/* should be always defined, see kernel 7615d9e1780e26e0178c93c55b73309a5dc093d7 */
#    if defined(__alpha__)
#      define __NR_pidfd_open 544
#    elif defined _MIPS_SIM
#      if _MIPS_SIM == _MIPS_SIM_ABI32  /* o32 */
#        define systemd_NR_pidfd_open (434 + 4000)
#      endif
#      if _MIPS_SIM == _MIPS_SIM_NABI32 /* n32 */
#        define systemd_NR_pidfd_open (434 + 6000)
#      endif
#      if _MIPS_SIM == _MIPS_SIM_ABI64  /* n64 */
#        define systemd_NR_pidfd_open (434 + 5000)
#      endif
#    elif defined __ia64__
#      define systemd_NR_pidfd_open (434 + 1024)
#    else
#      define __NR_pidfd_open systemd_SC_arch_bias(434)
#    endif
#  endif
static inline int missing_pidfd_open(pid_t pid, unsigned flags) {
#  ifdef __NR_pidfd_open
        return syscall(__NR_pidfd_open, pid, flags);
#  else
        errno = ENOSYS;
        return -1;
#  endif
}

#  define pidfd_open missing_pidfd_open
#endif

#if !HAVE_RT_SIGQUEUEINFO
static inline int missing_rt_sigqueueinfo(pid_t tgid, int sig, siginfo_t *info) {
        return syscall(__NR_rt_sigqueueinfo, tgid, sig, info);
}

#  define rt_sigqueueinfo missing_rt_sigqueueinfo
#endif
