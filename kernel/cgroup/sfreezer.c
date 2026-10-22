// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal sfreezer cgroup controller.
 *
 * Samsung vendor userspace mounts a "sfreezer" cgroup v1 hierarchy at
 * /dev/freezer (see vendor cgroups.json). The stock Samsung kernel ships a
 * proprietary controller under this name. Without it, the v1 mount fails
 * which aborts CgroupSetup() in second-stage init and cascades into
 * createProcessGroup() failures for subsequently started services.
 *
 * This registers an inert controller so the mount succeeds.
 */
#include <linux/cgroup.h>
#include <linux/slab.h>

#include "cgroup-internal.h"

static struct cgroup_subsys_state *
sfreezer_css_alloc(struct cgroup_subsys_state *parent_css)
{
	struct cgroup_subsys_state *css = kzalloc(sizeof(*css), GFP_KERNEL);

	if (!css)
		return ERR_PTR(-ENOMEM);

	return css;
}

static void sfreezer_css_free(struct cgroup_subsys_state *css)
{
	kfree(css);
}

struct cgroup_subsys sfreezer_cgrp_subsys = {
	.css_alloc	= sfreezer_css_alloc,
	.css_free	= sfreezer_css_free,
};
