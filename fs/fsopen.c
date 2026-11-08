// SPDX-License-Identifier: GPL-2.0
/*
 * Filesystem context userspace API: fsopen(), fspick(), fsconfig().
 *
 * Backport for the 4.19 downstream kernel. The fs_context core already
 * exists (Android-4.19 backport); this file adds the file-descriptor
 * facing layer on top of it. Differences to upstream 5.x:
 *  - no fc_log buffer (the Android fs_context has no ->log)
 *  - phase tracking lives in struct fsctx_file, not in fs_context
 */
#include <linux/fs.h>
#include <linux/fs_context.h>
#include <linux/mount.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/fsnotify.h>
#include <linux/namei.h>
#include <linux/security.h>
#include <linux/user_namespace.h>
#include <linux/mnt_namespace.h>
#include <linux/anon_inodes.h>
#include <linux/syscalls.h>
#include <linux/capability.h>
#include <linux/sched/task.h>
#include <linux/uaccess.h>
#include <uapi/linux/mount.h>
#include "mount.h"
#include "internal.h"

static int fscontext_release(struct inode *inode, struct file *file)
{
	struct fsctx_file *f = file->private_data;

	if (f) {
		file->private_data = NULL;
		if (f->fc)
			put_fs_context(f->fc);
		kfree(f);
	}
	return 0;
}

const struct file_operations fscontext_fops = {
	.release	= fscontext_release,
	.llseek		= no_llseek,
};

static int fscontext_create_fd(struct fs_context *fc, unsigned int o_flags)
{
	struct fsctx_file *f;
	int fd;

	f = kzalloc(sizeof(*f), GFP_KERNEL);
	if (!f) {
		put_fs_context(fc);
		return -ENOMEM;
	}
	f->fc = fc;
	f->phase = FSCTX_PHASE_PARAMS;
	mutex_init(&f->uapi_mutex);

	fd = anon_inode_getfd("[fscontext]", &fscontext_fops, f,
			      O_RDWR | o_flags);
	if (fd < 0) {
		put_fs_context(fc);
		kfree(f);
	}
	return fd;
}

static int mount_capable_fsopen(struct fs_context *fc)
{
	return ns_capable(fc->user_ns, CAP_SYS_ADMIN) &&
	       ns_capable(current->nsproxy->mnt_ns->user_ns, CAP_SYS_ADMIN);
}

/*
 * Open a filesystem by name so that it can be configured for mounting.
 */
SYSCALL_DEFINE2(fsopen, const char __user *, _fs_name, unsigned int, flags)
{
	struct file_system_type *fs_type;
	struct fs_context *fc;
	const char *fs_name;
	int ret;

	if (!ns_capable(current->nsproxy->mnt_ns->user_ns, CAP_SYS_ADMIN))
		return -EPERM;

	if (flags & ~FSOPEN_CLOEXEC)
		return -EINVAL;

	pr_err("FSAPI: fsopen called\n");
	fs_name = strndup_user(_fs_name, PAGE_SIZE);
	if (IS_ERR(fs_name))
		return PTR_ERR(fs_name);

	fs_type = get_fs_type(fs_name);
	kfree(fs_name);
	if (!fs_type)
		return -ENODEV;

	fc = fs_context_for_mount(fs_type, 0);
	put_filesystem(fs_type);
	if (IS_ERR(fc))
		return PTR_ERR(fc);

	return fscontext_create_fd(fc, flags & FSOPEN_CLOEXEC ? O_CLOEXEC : 0);
}

/*
 * Pick a superblock into a context for reconfiguration.
 */
SYSCALL_DEFINE3(fspick, int, dfd, const char __user *, path, unsigned int, flags)
{
	struct fs_context *fc;
	struct path target;
	unsigned int lookup_flags;
	int ret;

	if (!ns_capable(current->nsproxy->mnt_ns->user_ns, CAP_SYS_ADMIN))
		return -EPERM;

	if ((flags & ~(FSPICK_CLOEXEC |
		       FSPICK_SYMLINK_NOFOLLOW |
		       FSPICK_NO_AUTOMOUNT |
		       FSPICK_EMPTY_PATH)) != 0)
		return -EINVAL;

	lookup_flags = LOOKUP_FOLLOW | LOOKUP_AUTOMOUNT;
	if (flags & FSPICK_SYMLINK_NOFOLLOW)
		lookup_flags &= ~LOOKUP_FOLLOW;
	if (flags & FSPICK_NO_AUTOMOUNT)
		lookup_flags &= ~LOOKUP_AUTOMOUNT;
	if (flags & FSPICK_EMPTY_PATH)
		lookup_flags |= LOOKUP_EMPTY;
	ret = user_path_at(dfd, path, lookup_flags, &target);
	if (ret < 0)
		return ret;

	ret = -EINVAL;
	if (target.mnt->mnt_root != target.dentry)
		goto err_path;

	fc = fs_context_for_reconfigure(target.dentry, 0, 0);
	if (IS_ERR(fc)) {
		ret = PTR_ERR(fc);
		goto err_path;
	}

	path_put(&target);
	ret = fscontext_create_fd(fc, flags & FSPICK_CLOEXEC ? O_CLOEXEC : 0);
	if (ret >= 0) {
		struct fd f = fdget(ret);
		if (f.file)
			((struct fsctx_file *)f.file->private_data)->phase =
				FSCTX_PHASE_RECONF;
		fdput(f);
	}
	return ret;

err_path:
	path_put(&target);
	return ret;
}

static int vfs_fsconfig_locked(struct fsctx_file *f, int cmd,
			       struct fs_parameter *param)
{
	struct fs_context *fc = f->fc;
	struct super_block *sb;
	int ret;

	switch (cmd) {
	case FSCONFIG_CMD_CREATE:
		if (f->phase != FSCTX_PHASE_PARAMS)
			return -EBUSY;
		if (!mount_capable_fsopen(fc))
			return -EPERM;
		ret = vfs_get_tree(fc);
		if (ret)
			return ret;
		sb = fc->root->d_sb;
		ret = security_sb_kern_mount(sb);
		if (unlikely(ret)) {
			fc_drop_locked(fc);
			return ret;
		}
		up_write(&sb->s_umount);
		f->phase = FSCTX_PHASE_MOUNTED;
		return 0;
	case FSCONFIG_CMD_RECONFIGURE:
		if (f->phase != FSCTX_PHASE_RECONF &&
		    f->phase != FSCTX_PHASE_MOUNTED)
			return -EBUSY;
		sb = fc->root->d_sb;
		if (!ns_capable(sb->s_user_ns, CAP_SYS_ADMIN))
			return -EPERM;
		down_write(&sb->s_umount);
		ret = reconfigure_super(fc);
		up_write(&sb->s_umount);
		return ret;
	default:
		if (f->phase != FSCTX_PHASE_PARAMS &&
		    f->phase != FSCTX_PHASE_RECONF &&
		    f->phase != FSCTX_PHASE_MOUNTED)
			return -EBUSY;
		return vfs_parse_fs_param(fc, param);
	}
}

/**
 * sys_fsconfig - Set parameters and trigger actions on a context
 * @fd: The filesystem context to act upon
 * @cmd: The action to take
 * @_key: Where appropriate, the parameter key to set
 * @_value: Where appropriate, the parameter value to set
 * @aux: Additional information for the value
 */
SYSCALL_DEFINE5(fsconfig,
		int, fd,
		unsigned int, cmd,
		const char __user *, _key,
		const void __user *, _value,
		int, aux)
{
	struct fsctx_file *fctx;
	struct fd f;
	int ret;

	struct fs_parameter param = {
		.type	= fs_value_is_undefined,
	};

	if (fd < 0)
		return -EINVAL;

	switch (cmd) {
	case FSCONFIG_SET_FLAG:
		if (!_key || _value || aux)
			return -EINVAL;
		param.type = fs_value_is_flag;
		break;
	case FSCONFIG_SET_STRING:
		if (!_key || !_value || aux)
			return -EINVAL;
		param.type = fs_value_is_string;
		break;
	case FSCONFIG_SET_BINARY:
		if (!_key || !_value || aux <= 0 || aux > 1024 * 1024)
			return -EINVAL;
		param.type = fs_value_is_blob;
		break;
	case FSCONFIG_SET_PATH:
	case FSCONFIG_SET_PATH_EMPTY:
	case FSCONFIG_SET_FD:
		return -EOPNOTSUPP;
	case FSCONFIG_CMD_CREATE:
	case FSCONFIG_CMD_RECONFIGURE:
		if (_key || _value || aux)
			return -EINVAL;
		break;
	default:
		return -EOPNOTSUPP;
	}

	f = fdget(fd);
	if (!f.file)
		return -EBADF;
	ret = -EINVAL;
	if (f.file->f_op != &fscontext_fops)
		goto out_f;

	fctx = f.file->private_data;

	if (_key) {
		param.key = strndup_user(_key, 256);
		if (IS_ERR(param.key)) {
			ret = PTR_ERR(param.key);
			goto out_f;
		}
	}

	switch (param.type) {
	case fs_value_is_string:
		param.string = strndup_user(_value, 256 * 1024);
		if (IS_ERR(param.string)) {
			ret = PTR_ERR(param.string);
			param.string = NULL;
			goto out_key;
		}
		param.size = strlen(param.string);
		break;
	case fs_value_is_blob:
		param.size = aux;
		param.blob = memdup_user(_value, aux);
		if (IS_ERR(param.blob)) {
			ret = PTR_ERR(param.blob);
			param.blob = NULL;
			goto out_key;
		}
		break;
	default:
		break;
	}

	pr_err("FSAPI: fsconfig cmd=%d key=%s\n", cmd,
	       param.key ? param.key : "-");
	ret = mutex_lock_interruptible(&fctx->uapi_mutex);
	if (ret == 0) {
		ret = vfs_fsconfig_locked(fctx, cmd, &param);
		pr_err("FSAPI: fsconfig ret=%d\n", ret);
		mutex_unlock(&fctx->uapi_mutex);
	}
out_key:
	switch (param.type) {
	case fs_value_is_string:
		kfree(param.string);
		break;
	case fs_value_is_blob:
		kfree(param.blob);
		break;
	default:
		break;
	}
	kfree(param.key);
out_f:
	fdput(f);
	return ret;
}
