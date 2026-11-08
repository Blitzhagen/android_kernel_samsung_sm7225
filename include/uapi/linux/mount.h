/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_MOUNT_H
#define _UAPI_LINUX_MOUNT_H

#include <linux/types.h>
#include <linux/limits.h>

/*
 * Mount object attributes for the fsmount() and open_tree() syscalls.
 */
#define MOUNT_ATTR_RDONLY	0x00000001 /* Mount read-only */
#define MOUNT_ATTR_NOSUID	0x00000002 /* Ignore suid and sgid bits */
#define MOUNT_ATTR_NODEV	0x00000004 /* Disallow access to device special files */
#define MOUNT_ATTR_NOEXEC	0x00000008 /* Disallow program execution */
#define MOUNT_ATTR__ATIME	0x00000070 /* Setting on how atime should be updated */
#define MOUNT_ATTR_RELATIME	0x00000000 /* - Update atime relative to mtime/ctime */
#define MOUNT_ATTR_NOATIME	0x00000010 /* - Do not update access times */
#define MOUNT_ATTR_STRICTATIME	0x00000020 /* - Always perform atime updates */
#define MOUNT_ATTR_NODIRATIME	0x00000080 /* Do not update directory access times */
#define MOUNT_ATTR_NOSYMFOLLOW	0x00200000 /* Do not follow symlinks */

/*
 * List of mount ID attributes.
 */
enum mount_attr_id {
	MOUNT_ATTR_IDMAP,
	MOUNT_ATTR_SIZE_VER0,
	MOUNT_ATTR_SIZE_VER1,
};

/*
 * Flags for mount_setattr().
 */
#define AT_RECURSIVE		0x00008000 /* Change the mount properties recursively */

/*
 * fsopen() flags.
 */
#define FSOPEN_CLOEXEC		0x00000001

/*
 * fspick() flags.
 */
#define FSPICK_CLOEXEC		0x00000001
#define FSPICK_SYMLINK_NOFOLLOW	0x00000002
#define FSPICK_NO_AUTOMOUNT	0x00000004
#define FSPICK_EMPTY_PATH	0x00000008

/*
 * fsmount() flags.
 */
#define FSMOUNT_CLOEXEC		0x00000001

/*
 * Mount attributes for fsmount(2) and open_tree(2).
 */
#define FSCONFIG_SET_FLAG	0	/* Set parameter, supplying no value */
#define FSCONFIG_SET_STRING	1	/* Set parameter, supplying a string value */
#define FSCONFIG_SET_BINARY	2	/* Set parameter, supplying a binary blob value */
#define FSCONFIG_SET_PATH	3	/* Set parameter, supplying an object by path */
#define FSCONFIG_SET_PATH_EMPTY	4	/* Set parameter, supplying an object by (empty) path */
#define FSCONFIG_SET_FD		5	/* Set parameter, supplying an object by fd */
#define FSCONFIG_CMD_CREATE	6	/* Invoke superblock creation */
#define FSCONFIG_CMD_RECONFIGURE	7	/* Invoke superblock reconfiguration */

/*
 * move_mount() flags.
 */
#define MOVE_MOUNT_F_SYMLINKS		0x00000001 /* Follow symlinks on from path */
#define MOVE_MOUNT_F_AUTOMOUNTS		0x00000002 /* Follow automounts on from path */
#define MOVE_MOUNT_F_EMPTY_PATH		0x00000004 /* Empty from path permitted */
#define MOVE_MOUNT_T_SYMLINKS		0x00000010 /* Follow symlinks on to path */
#define MOVE_MOUNT_T_AUTOMOUNTS		0x00000020 /* Follow automounts on to path */
#define MOVE_MOUNT_T_EMPTY_PATH		0x00000040 /* Empty to path permitted */
#define MOVE_MOUNT_BENEATH		0x00000100 /* Mount beneath the existing mount */
#define MOVE_MOUNT__MASK		0x00000177

/*
 * open_tree() flags.
 */
#define OPEN_TREE_CLONE		1		/* Clone the target tree and attach the clone */
#define OPEN_TREE_CLOEXEC	O_CLOEXEC	/* Close the FD on execve() */
#define AT_MOVE_MOUNT_BENEATH	AT_RECURSIVE	/* (legacy name) */

#endif /* _UAPI_LINUX_MOUNT_H */
