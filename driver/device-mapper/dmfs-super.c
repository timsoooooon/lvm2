/*
 * dmfs-super.c
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

#include <linux/config.h>
#include <linux/fs.h>

#define DMFS_MAGIC 0x444D4653

extern struct inode *dmfs_create_root(struct super_block *sb, int);

static int dmfs_statfs(struct super_block *sb, struct statfs *buf)
{
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
	buf->f_namelen = DM_NAME_LEN - 1;

	return 0;
}

static void dmfs_delete_inode(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct dmfs_i *dmi = DMFS_I(inode);

	if (dmi) {
		if (dmi->md)
			dm_remove(dmi->md);
		if (dmi->table)
			dm_put_table(dmi->table):
		if (dmi->dentry)
			dm_unlock_tdir(dmi->dentry);
		kfree(dmi);
	}

	inode->u.generic_ip = NULL;
	clear_inode(inode);
}

static struct super_operations dmfs_super_operations = {
	statfs:		dmfs_statfs,
	put_inode:	force_delete,
	delete_inode:	dmfs_delete_inode,
};

struct super_block *dmfs_read_super(struct super_block *sb, void *data, int silent)
{
	struct inode *inode;
	struct dentry *root;

	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = DMFS_MAGIC;
	sb->s_ops = &dmfs_super_operations;
	sb->s_maxbytes = MAX_NON_LFS;

	inode = dmfs_create_root(sb, 0755);
	if (IS_ERR(inode))
		return NULL;
	root = d_alloc_root(inode);
	if (!root) {
		iput(inode);
		return NULL;
	}
	sb->s_root = root;

	return sb;
}


