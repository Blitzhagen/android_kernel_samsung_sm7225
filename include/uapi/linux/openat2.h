/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_OPENAT2_H
#define _UAPI_LINUX_OPENAT2_H

#include <linux/types.h>

/*
 * Arguments for how openat2(2) should open the target path. If @resolve is
 * zero, openat2(2) operates identically to openat(2).
 */
struct open_how {
	__u64 flags;		/* O_* flags. */
	__u64 mode;		/* O_CREAT/O_TMPFILE file mode. */
	__u64 resolve;		/* RESOLVE_* flags. */
};

/* how->resolve flags for openat2(2). */
#define RESOLVE_NO_XDEV		0x01 /* Block mount-point crossings
					(includes bind-mounts). */
#define RESOLVE_NO_MAGICLINKS	0x02 /* Block traversal through procfs-style
					"magic-links". */
#define RESOLVE_NO_SYMLINKS	0x04 /* Block traversal through all symlinks
					(implies OEXT_NO_MAGICLINKS). */
#define RESOLVE_BENEATH		0x08 /* Block "lexical" trickery like
					"..", symlinks, and absolute
					paths which escape the dirfd. */
#define RESOLVE_IN_ROOT		0x10 /* Make all jumps to "/" and ".."
					be scoped inside the dirfd
					(similar to chroot(2)). */
#define RESOLVE_CACHED		0x20 /* Only complete if resolution can be
					completed through cached lookup. May
					return -EAGAIN if that's not
					possible. */

/*
 * VALID_RESOLVE_FLAGS is the set of flags the kernel will accept.
 */
#define VALID_RESOLVE_FLAGS \
	(RESOLVE_NO_XDEV | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_SYMLINKS | \
	 RESOLVE_BENEATH | RESOLVE_IN_ROOT | RESOLVE_CACHED)

#endif /* _UAPI_LINUX_OPENAT2_H */
