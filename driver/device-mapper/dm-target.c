/*
 * dm-target.c
 *
 * Copyright (C) 2001 Sistina Software
 *
 * This software is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * 16/08/2001 - First Version [Joe Thornber]
 */

#include "dm.h"

static struct target_type *_targets;
static spinlock_t _lock = SPIN_LOCK_UNLOCKED;

struct target_type *__get_target(const char *name)
{
	struct target_type *t;
	for (t = _targets; t && strcmp(t->name, name); t = t->next)
		;
	return t;
}

struct target_type *dm_get_target_type(const char *name)
{
	struct target_type *t;

	spin_lock(&_lock);
	t = __get_target(name);
	spin_unlock(&_lock);

	return t;
}

/*
 * register a new target_type.
 */
int dm_register_target(const char *name, dm_ctr_fn ctr,
		       dm_dtr_fn dtr, dm_map_fn map)
{
	struct target_type *t =
		kmalloc(sizeof(*t) + strlen(name) + 1, GFP_KERNEL);

	if (!t)
		return -ENOMEM;

	spin_lock(&_lock);
	if (__get_target(name)) {
		WARN("mapper(%s) already registered\n", name);
		spin_unlock(&_lock);
		return -1;	/* FIXME: what's a good return value ? */
	}

	t->name = (char *) (t + 1);
	strcpy(t->name, name);

	t->ctr = ctr;
	t->dtr = dtr;
	t->map = map;

	t->next = _targets;
	_targets = t;

	spin_unlock(&_lock);
	return 0;
}


/*
 * io-err: always fails an io, useful for bringing
 * up LV's that have holes in them.
 */
static int io_err_ctr(struct dm_table *t, offset_t b, offset_t l,
		      struct text_region *args, void **result,
		      dm_error_fn fn, void *private)
{
	/* this takes no arguments */
	*result = 0;
	return 0;
}

static void io_err_dtr(struct dm_table *t, void *c)
{
	/* empty */
}

static int io_err_map(struct buffer_head *bh, void *context)
{
	buffer_IO_error(bh);
	return 0;
}

/*
 * linear: maps a linear range of a device.
 */
struct linear_c {
	kdev_t dev;
	int delta;		/* FIXME: we need a signed offset type */
};

/*
 * construct a linear mapping.
 * <dev_path> <offset>
 */
static int linear_ctr(struct dm_table *t, offset_t b, offset_t l,
		      struct text_region *args, void **result,
		      dm_error_fn fn, void *private)
{
	struct linear_c *lc;
	unsigned int start;
	kdev_t dev;
	int r;
	char path[256];
	struct text_region word;

	if (!dm_get_word(args, &word)) {
		fn("couldn't get device path", private);
		return -EINVAL;
	}

	dm_txt_copy(path, sizeof(path) - 1, &word);

	if ((r = dm_table_lookup_device(path, &dev))) {
		fn("no such device", private);
		return r;
	}

	if (!dm_get_number(args, &start)) {
		fn("destination start not given", private);
		return -EINVAL;
	}

	if (!(lc = kmalloc(sizeof(lc), GFP_KERNEL))) {
		fn("couldn't allocate memory for linear context\n", private);
		return -ENOMEM;
	}

	lc->dev = dev;
	lc->delta = (int) start - (int) b;

	if ((r = dm_table_add_device(t, lc->dev))) {
		fn("failed to add destination device to list", private);
		kfree(lc);
		return r;
	}

	*result = lc;
	return 0;
}

static void linear_dtr(struct dm_table *t, void *c)
{
	struct linear_c *lc = (struct linear_c *) c;
	dm_table_remove_device(t, lc->dev);
	kfree(c);
}

static int linear_map(struct buffer_head *bh, void *context)
{
	struct linear_c *lc = (struct linear_c *) context;

	bh->b_rdev = lc->dev;
	bh->b_rsector = bh->b_rsector + lc->delta;
	return 1;
}

/*
 * registers io-err and linear targets
 */
int dm_target_init(void)
{
	int ret;

#define xx(n, fn) \
	if ((ret = dm_register_target(n, \
             fn ## _ctr, fn ## _dtr, fn ## _map) < 0)) return ret

	xx("io-err", io_err);
	xx("linear", linear);
#undef xx

	return 0;
}

EXPORT_SYMBOL(dm_register_target);
