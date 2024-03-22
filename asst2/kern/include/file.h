/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
 * Put your function declarations and data types here ...
 */

struct file_descriptor_table {
    int fd_array[OPEN_MAX];
};

struct open_file {
    struct vnode *vn; // vnode pointer of the file
    int flags;  // flags of the file
    int refcount;   // reference count of the file
    off_t offset;   // offset of the file
};

struct open_file_table {
    struct open_file *of[OPEN_MAX];
    struct lock *oft_lock;
};

struct open_file_table *oft;

// file table initialization function prototype
int open_file_table_init(void);

// file descriptor table initialization function prototype
int file_descriptor_table_init(const char *stdin, const char *stdout, const char *stderr);

int init(void);

// file open function prototype
// filename: name of the file to be opened
// flags: flags of the file to be opened
// mode: mode of the file to be opened
// retval: return value of the file open function
int sys_open(userptr_t filename, int flags, mode_t mode, int *retval); 

// file close function prototype
// fd: file descriptor of the file to be closed
int sys_close(int fd);

// file read function prototype
// fd: file descriptor of the file to be read
// buf: buffer to store the data read from the file
// buflen: length of the buffer
// retval: return value of the file read function
int sys_read(int fd, userptr_t buf, size_t buflen, int *retval);

// file write function prototype
// fd: file descriptor of the file to be written
// buf: buffer to store the data to be written to the file
// buflen: length of the buffer
// retval: return value of the file write function
int sys_write(int fd, userptr_t buf, size_t buflen, int *retval);

// file lseek function prototype
// fd: file descriptor of the file to be seeked
// pos: position to be seeked
// whence: where to seek from
// retval: return value of the file lseek function
int sys_lseek(int fd, off_t pos, int whence, off_t *retval64);

// file dup2 function prototype
// oldfd: old file descriptor
// newfd: new file descriptor
// retval: return value of the file dup2 function
int sys_dup2(int oldfd, int newfd, int *retval);

// help function
int stdioe(const char *std, int mode);
int new_file(char *filename, int flags, mode_t mode, int *retval);
int find_empty_slot(int index, struct vnode *vn, int mode);
int create_new_file(char *filename, int flags, mode_t mode, struct open_file *of, struct vnode **vn);
int read_and_write(int fd, userptr_t buf, size_t nbytes, int *retval, int mode);

#endif /* _FILE_H_ */
