#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int fs_getattr( const char *, struct stat * );
int fs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int fs_open( const char *, struct fuse_file_info * );
int fs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int fs_release(const char *path, struct fuse_file_info *);
int fs_write( const char *, const char *, size_t, off_t, struct fuse_file_info *);
int fs_mkdir(const char *, mode_t);
int fs_rmdir(const char *, mode_t);
int fs_utime(const char *, const struct timespec tv[2], struct fuse_file_info *);
int fs_truncate(const char *, off_t, struct fuse_file_info *);

static struct fuse_operations lfs_oper = {
	.getattr	= fs_getattr,
	.readdir	= fs_readdir,
	.mkdir = fs_mkdir,
	.rmdir = fs_rmdir,
	.truncate = fs_truncate,
	.open	= fs_open,
	.read	= fs_read,
	.release = fs_release, //closes a file
	.write = fs_write,
	.rename = fs_rename,
	.utime = fs_utime,     //maybe should use uitemns (used to update access and modification of file)
};
