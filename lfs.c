#include "lfs.h"

int fs_getattr( const char *path, struct stat *stbuf ) {
	int res = 0;
	printf("getattr: (path=%s)\n", path);

	memset(stbuf, 0, sizeof(struct stat));
	if( strcmp( path, "/" ) == 0 ) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if( strcmp( path, "/hello" ) == 0 ) {
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = 12;
	} else
		res = -ENOENT;

	return res;
}

int fs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
	(void) offset;
	(void) fi;
	printf("readdir: (path=%s)\n", path);

	if(strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, "hello", NULL, 0);
	filler(buf, "test123", NULL, 0);

	if (strcmp(path, "/") == 0){
		filler(buf, "root_file", NULL, 0);
	}

	return 0;
}

//Permission
int fs_open( const char *path, struct fuse_file_info *fi ) {
    printf("open: (path=%s)\n", path);
	return 0;
}

int fs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
    printf("read: (path=%s)\n", path);
	memcpy( buf, "Hello\n", 6 );
	return 6;
}

int fs_write( const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
  return 0;
}

int fs_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}

int fs_mkdir(const char *path , mode_t mode){
  return 0;
}
int fs_rmdir(const char *path, mode_t mode){
  return 0;
}

int fs_utime(const char *path, const struct timespec tv[2], struct fuse_file_info *fi){
  return 0;
}

int fs_truncate(const char *path, off_t size, struct fuse_file_info *fi){
  return 0;
}


int main( int argc, char *argv[] ) {
	fuse_main( argc, argv, &lfs_oper ); // Mounts the file system, at the mountpoint given

	return 0;
}
