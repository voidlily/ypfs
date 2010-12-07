/*
  Big Brother File System

  The point of this FUSE filesystem is to provide an introduction to
  FUSE.  It was my first FUSE filesystem as I got to know the
  software; hopefully, the comments in this code will help people who
  follow later to get a gentler introduction.

  This might be called a no-op filesystem:  it doesn't impose
  filesystem semantics on top of any other existing structure.  It
  simply reports the requests that come in, and pass them to an
  underlying filesystem.  The information is saved in a logfile named
  bbfs.log, in the directory from which you run bbfs.

  gcc -Wall `pkg-config fuse --cflags --libs` -o bbfs bbfs.c
*/

#include "params.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>


// Report errors to logfile and give -errno to caller
int ypfs_error(char *str)
{
    int ret = -errno;
    
    
    return ret;
}

//  All the paths I see are relative to the root of the mounted
//  filesystem.  In order to get to the underlying filesystem, I need to
//  have the mountpoint.  I'll save it away early on in main(), and then
//  whenever I need a path for something I'll call this to construct
//  it.
void ypfs_fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, YPFS_DATA->rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will
				    // break here

}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int ypfs_getattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    retstat = lstat(fpath, statbuf);
    if (retstat != 0)
	retstat = ypfs_error("ypfs_getattr lstat");
    
    return retstat;
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// the description given above doesn't correspond to the readlink(2)
// man page -- according to that, if the link is too long for the
// buffer, it ends up without the null termination
int ypfs_readlink(const char *path, char *link, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    retstat = readlink(fpath, link, size);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_readlink readlink");
    
    return 0;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int ypfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    // On Linux this could just be 'mknod(path, mode, rdev)' but this
    //  is more portable
    if (S_ISREG(mode)) {
        retstat = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
	if (retstat < 0)
	    retstat = ypfs_error("ypfs_mknod open");
        else {
            retstat = close(retstat);
	    if (retstat < 0)
		retstat = ypfs_error("ypfs_mknod close");
	}
    } else
	if (S_ISFIFO(mode)) {
	    retstat = mkfifo(fpath, mode);
	    if (retstat < 0)
		retstat = ypfs_error("ypfs_mknod mkfifo");
	} else {
	    retstat = mknod(fpath, mode, dev);
	    if (retstat < 0)
		retstat = ypfs_error("ypfs_mknod mknod");
	}
    
    return retstat;
}

/** Create a directory */
int ypfs_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    // TODO
    // If parent directories do not exist, they need to be made like mkdir -p
    // (plenty of examples on google for this)
    retstat = mkdir(fpath, mode);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_mkdir mkdir");
    
    return retstat;
}

/** Remove a file */
int ypfs_unlink(const char *path)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    retstat = unlink(fpath);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_unlink unlink");
    
    return retstat;
}

/** Remove a directory */
int ypfs_rmdir(const char *path)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    retstat = rmdir(fpath);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_rmdir rmdir");
    
    return retstat;
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int ypfs_symlink(const char *path, const char *link)
{
    int retstat = 0;
    char flink[PATH_MAX];
    
    ypfs_fullpath(flink, link);
    
    retstat = symlink(path, flink);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_symlink symlink");
    
    return retstat;
}

/** Rename a file */
// both path and newpath are fs-relative
int ypfs_rename(const char *path, const char *newpath)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    char fnewpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    ypfs_fullpath(fnewpath, newpath);
    
    retstat = rename(fpath, fnewpath);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_rename rename");
    
    return retstat;
}

/** Create a hard link to a file */
int ypfs_link(const char *path, const char *newpath)
{
    int retstat = 0;
    char fpath[PATH_MAX], fnewpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    ypfs_fullpath(fnewpath, newpath);
    
    retstat = link(fpath, fnewpath);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_link link");
    
    return retstat;
}

/** Change the permission bits of a file */
int ypfs_chmod(const char *path, mode_t mode)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    retstat = chmod(fpath, mode);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_chmod chmod");
    
    return retstat;
}

/** Change the owner and group of a file */
int ypfs_chown(const char *path, uid_t uid, gid_t gid)
  
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    retstat = chown(fpath, uid, gid);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_chown chown");
    
    return retstat;
}

/** Change the size of a file */
int ypfs_truncate(const char *path, off_t newsize)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    retstat = truncate(fpath, newsize);
    if (retstat < 0)
	ypfs_error("ypfs_truncate truncate");
    
    return retstat;
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int ypfs_utime(const char *path, struct utimbuf *ubuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    retstat = utime(fpath, ubuf);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_utime utime");
    
    return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int ypfs_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    int fd;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    fd = open(fpath, fi->flags);
    if (fd < 0)
	retstat = ypfs_error("ypfs_open open");
    
    fi->fh = fd;
    
    return 0;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int ypfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    // no need to get fpath on this one, since I work from fi->fh not the path
    
    retstat = pread(fi->fh, buf, size, offset);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_read read");
    
    return retstat;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int ypfs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    
    // no need to get fpath on this one, since I work from fi->fh not the path
	
    retstat = pwrite(fi->fh, buf, size, offset);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_write pwrite");
    
    return retstat;
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int ypfs_statfs(const char *path, struct statvfs *statv)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    // get stats for underlying filesystem
    retstat = statvfs(fpath, statv);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_statfs statvfs");
    
    
    return retstat;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
int ypfs_flush(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
	
    return retstat;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int ypfs_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    // TODO
    // We copy files from elsewhere into the root directory.
    // When the copying is done, release is the last call done.
    // If the file is released and in the root directory, move to the proper place
    // (with creating new directories as necessary)
    // Will need to write a function to check for exif data
    // If the exif exists, use the exif date to place the directory
    // Otherwise, use old file modified date (since create date does not exist in linux)
    
    return retstat;
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int ypfs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    
    if (datasync)
	retstat = fdatasync(fi->fh);
    else
	retstat = fsync(fi->fh);
    
    if (retstat < 0)
	ypfs_error("ypfs_fsync fsync");
    
    return retstat;
}

/** Set extended attributes */
int ypfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    retstat = lsetxattr(fpath, name, value, size, flags);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_setxattr lsetxattr");
    
    return retstat;
}

/** Get extended attributes */
int ypfs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    retstat = lgetxattr(fpath, name, value, size);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_getxattr lgetxattr");
    
    return retstat;
}

/** List extended attributes */
int ypfs_listxattr(const char *path, char *list, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    retstat = llistxattr(fpath, list, size);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_listxattr llistxattr");
    
    
    return retstat;
}

/** Remove extended attributes */
int ypfs_removexattr(const char *path, const char *name)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    retstat = lremovexattr(fpath, name);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_removexattr lrmovexattr");
    
    return retstat;
}

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int ypfs_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int retstat = 0;
    char fpath[PATH_MAX];
    
    ypfs_fullpath(fpath, path);
    
    dp = opendir(fpath);
    if (dp == NULL)
	retstat = ypfs_error("ypfs_opendir opendir");
    
    fi->fh = (intptr_t) dp;
    
    
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int ypfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int retstat = 0;
    DIR *dp;
    struct dirent *de;
    
    // once again, no need for fullpath -- but note that I need to cast fi->fh
    dp = (DIR *) (uintptr_t) fi->fh;

    // Every directory contains at least two entries: . and ..  If my
    // first call to the system readdir() returns NULL I've got an
    // error; near as I can tell, that's the only condition under
    // which I can get an error from readdir()
    de = readdir(dp);
    if (de == 0)
	return -errno;

    // This will copy the entire directory into the buffer.  The loop exits
    // when either the system readdir() returns NULL, or filler()
    // returns something non-zero.  The first case just means I've
    // read the whole directory; the second means the buffer is full.
    do {
	if (filler(buf, de->d_name, NULL, 0) != 0)
	    return -ENOMEM;
    } while ((de = readdir(dp)) != NULL);
    
    
    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int ypfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    
    closedir((DIR *) (uintptr_t) fi->fh);
    
    return retstat;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ???
int ypfs_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    
    return retstat;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *ypfs_init(struct fuse_conn_info *conn)
{
    
    
    return YPFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void ypfs_destroy(void *userdata)
{
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int ypfs_access(const char *path, int mask)
{
    int retstat = 0;
    char fpath[PATH_MAX];
   
    ypfs_fullpath(fpath, path);
    
    retstat = access(fpath, mask);
    
    if (retstat < 0)
	retstat = ypfs_error("ypfs_access access");
    
    return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int ypfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    int fd;
    
    ypfs_fullpath(fpath, path);
    
    fd = creat(fpath, mode);
    if (fd < 0)
	retstat = ypfs_error("ypfs_create creat");
    
    fi->fh = fd;
    
    
    return retstat;
}

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int ypfs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    
    retstat = ftruncate(fi->fh, offset);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_ftruncate ftruncate");
    
    return retstat;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
// Since it's currently only called after ypfs_create(), and ypfs_create()
// opens the file, I ought to be able to just use the fd and ignore
// the path...
int ypfs_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    
    retstat = fstat(fi->fh, statbuf);
    if (retstat < 0)
	retstat = ypfs_error("ypfs_fgetattr fstat");
    
    
    return retstat;
}

struct fuse_operations ypfs_oper = {
  .getattr = ypfs_getattr,
  .readlink = ypfs_readlink,
  // no .getdir -- that's deprecated
  .getdir = NULL,
  .mknod = ypfs_mknod,
  .mkdir = ypfs_mkdir,
  .unlink = ypfs_unlink,
  .rmdir = ypfs_rmdir,
  .symlink = ypfs_symlink,
  .rename = ypfs_rename,
  .link = ypfs_link,
  .chmod = ypfs_chmod,
  .chown = ypfs_chown,
  .truncate = ypfs_truncate,
  .utime = ypfs_utime,
  .open = ypfs_open,
  .read = ypfs_read,
  .write = ypfs_write,
  /** Just a placeholder, don't set */ // huh???
  .statfs = ypfs_statfs,
  .flush = ypfs_flush,
  .release = ypfs_release,
  .fsync = ypfs_fsync,
  .setxattr = ypfs_setxattr,
  .getxattr = ypfs_getxattr,
  .listxattr = ypfs_listxattr,
  .removexattr = ypfs_removexattr,
  .opendir = ypfs_opendir,
  .readdir = ypfs_readdir,
  .releasedir = ypfs_releasedir,
  .fsyncdir = ypfs_fsyncdir,
  .init = ypfs_init,
  .destroy = ypfs_destroy,
  .access = ypfs_access,
  .create = ypfs_create,
  .ftruncate = ypfs_ftruncate,
  .fgetattr = ypfs_fgetattr
};

void ypfs_usage()
{
    fprintf(stderr, "usage:  bbfs rootDir mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int i;
    int fuse_stat;
    struct ypfs_state *ypfs_data;

    ypfs_data = calloc(sizeof(struct ypfs_state), 1);
    if (ypfs_data == NULL) {
	perror("main calloc");
	abort();
    }
    

    // libfuse is able to do most of the command line parsing; all I
    // need to do is to extract the rootdir; this will be the first
    // non-option passed in.  I'm using the GNU non-standard extension
    // and having realpath malloc the space for the path
    // the string.
    for (i = 1; (i < argc) && (argv[i][0] == '-'); i++);
    if (i == argc)
	ypfs_usage();
    
    ypfs_data->rootdir = realpath(argv[i], NULL);

    for (; i < argc; i++)
	   argv[i] = argv[i+1];
    argc--;

    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &ypfs_oper, ypfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
