#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>

/*
 * Initialise file open table
 */

int open_file_table_init(void) {
    if (oft == NULL) {
        oft = kmalloc(sizeof(struct open_file_table));
        if (oft == NULL) {
            return ENOMEM;
        }

        for (int i = 0; i < OPEN_MAX; i++) {
            oft->of[i] = NULL;
        }

        struct lock *oftLock = lock_create("open_file_table_lock");
        oft->oft_lock = oftLock;
        // avoid oft_lock being freed
        KASSERT(oft->oft_lock != 0);
        if (oft->oft_lock == 0) {
            return ENOMEM;
        }

    }
    return 0;
}

/*
 * Initialise file descriptor table
 */

int file_descriptor_table_init(const char *stdin, const char *stdout,
		const char *stderr) {
    curproc->fdt = kmalloc(sizeof(struct file_descriptor_table));
    if (curproc->fdt == NULL) {
        return ENOMEM;
    }
    for (int i = 0; i < OPEN_MAX; i++) {
        curproc->fdt->fd_array[i] = -1;
    }
    // stdin
    stdioe(stdin, 0);
    // stdout
    stdioe(stdout, 1);
    // stderr
    stdioe(stderr, 1);
    return 0;
}

int init(void) {
    open_file_table_init();
    file_descriptor_table_init("con:", "con:", "con:");
    return 0;
}

int sys_open(userptr_t filename, int flags, mode_t mode, int *retval) {
    char f[PATH_MAX];
    int result = copyinstr(filename, f, PATH_MAX, NULL);
    if (result) {
        return result;
    }

    result = new_file(f, flags, mode, retval);

    return result;
}

int sys_close(int fd) {
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }

    lock_acquire(oft->oft_lock);

    int of_index = curproc->fdt->fd_array[fd];
    if (of_index < 0 || of_index >= OPEN_MAX) {
        lock_release(oft->oft_lock);
        return EBADF;
    }

    struct open_file *of = oft->of[of_index];
    if (of == NULL) {
        lock_release(oft->oft_lock);
        return EBADF;
    }

    curproc->fdt->fd_array[fd] = -1;
    of->refcount--;
    if (of->refcount == 0) {
        oft->of[of_index] = NULL;
        vfs_close(of->vn);
        kfree(of);
    }

    lock_release(oft->oft_lock);

    return 0;
}

int sys_read(int fd, userptr_t buf, size_t nbytes, int *retval) {
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }
    
    return read_and_write(fd, buf, nbytes, retval, 0);
}

int sys_write(int fd, userptr_t buf, size_t nbytes, int *retval) {
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }
    
    return read_and_write(fd, buf, nbytes, retval, 1);
}


int sys_lseek(int fd, off_t pos, int whence, off_t *retval64) {
    // check is fd valid
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }

    int of_inter = curproc->fdt->fd_array[fd];
    
    if (of_inter < 0 || of_inter >= OPEN_MAX) {
        return EBADF;
    }
    struct open_file *of = oft->of[of_inter];
    
    struct stat file;

    off_t offset = of->offset;
    // VOP_ISSEEKABLE()
    if (!VOP_ISSEEKABLE(of->vn)) {
        return ESPIPE;
    }
    // 3 situations for whence
    lock_acquire(oft->oft_lock);
    switch(whence) {
        case SEEK_CUR:
            // start from current position
            if ((pos + offset) < 0) {
                return EINVAL;
            }
            oft->of[of_inter]->offset = pos + offset;
            *retval64 = pos + offset;            
            break;
        case SEEK_END:
            // start from end of file
            VOP_STAT(of->vn, &file);
            if ((pos + file.st_size) < 0) {
                return EINVAL;
            }
            oft->of[of_inter]->offset = pos + file.st_size;
            *retval64 = pos + file.st_size;
            break;
        case SEEK_SET:
            // start from beginning of file
            // check is valid pos
            if (pos < 0) {
                return EINVAL;
            }
            oft->of[of_inter]->offset = pos;
            *retval64 = pos;
            break;
    }
        lock_release(oft->oft_lock);
        return 0;
}

int sys_dup2(int oldfd, int newfd, int *retval) {

    // check oldfd and newfd are in valid range
    if (oldfd < 0 || oldfd >= OPEN_MAX || newfd < 0 || newfd >= OPEN_MAX) {
        return EBADF;
    }
    // check is oldfd valid
    int of_inter = curproc->fdt->fd_array[oldfd];
    //struct open_file *of = oft->of[of_inter];
    if (of_inter < 0 || of_inter >= OPEN_MAX) {
        return EBADF;
    }

    // oldfd valid and oldfd = newfd then return newfd
    if (oldfd == newfd) {
        *retval = newfd;
        return 0;
    }

    // oldfd valid and newfd point to a open file then close this file 
    // and newfd point to oldfd file
    int newof_inter = curproc->fdt->fd_array[newfd];
    if (oft->of[newof_inter] != NULL) {
        sys_close(newfd);
    }
    // olffd valid and newfd not used
    lock_acquire(oft->oft_lock);
    curproc->fdt->fd_array[newfd] = of_inter;
    oft->of[of_inter]->refcount = oft->of[of_inter]->refcount + 1;
    lock_release(oft->oft_lock);
    *retval = newfd;
    return 0;
}

// help functions
int stdioe(const char *std, int mode) {
    int fd;
    char path[PATH_MAX];
    strcpy(path, std);
    int result;
    if (mode == 0) {
        result = new_file(path, O_RDONLY, 0, &fd);
    } else {
        result = new_file(path, O_WRONLY, 0, &fd);
    }
    if (result) {
        kfree(curproc->fdt);
        return result;
    }
    return 0;
}

int new_file(char *filename, int flags, mode_t mode, int *retval) {
    struct vnode *vn;
    int result;
    int of_index = -1;
    int fd_index = -1;
    
    // find an empty slot in open file table
    lock_acquire(oft->oft_lock);
    of_index = find_empty_slot(of_index, vn, 0);
    // find an empty slot in file descriptor table
    fd_index = find_empty_slot(fd_index, vn, 1);
    // create a new open file
    struct open_file *of = kmalloc(sizeof(struct open_file));
    result = create_new_file(filename, flags, mode, of, &vn);
    if (result) {
        lock_release(oft->oft_lock);
        return result;
    }
    // add the open file to open file table
    oft->of[of_index] = of;
    // add the open file to file descriptor table
    curproc->fdt->fd_array[fd_index] = of_index;
    lock_release(oft->oft_lock);
    *retval = fd_index;
    return 0;
}

int create_new_file(char *filename, int flags, mode_t mode, struct open_file *of, struct vnode **vn) {
    int result;
    // open the file
    result = vfs_open(filename, flags, mode, vn);
    if (result) {
        return result;
    }
    // initialise the open file
    of->vn = *vn;
    of->offset = 0;
    of->flags = flags;
    of->refcount = 1;
    return 0;
}

// mode = 0: open file table
// mode = 1: file descriptor table
int find_empty_slot(int index, struct vnode *vn, int mode) {
    if (mode == 0) {
        for (int i = 0; i < OPEN_MAX; i++) {
            if (oft->of[i] == NULL) {
                index = i;
                break;
            }
        }
    }
    else if (mode == 1) {
        for (int i = 0; i < OPEN_MAX; i++) {
            if (curproc->fdt->fd_array[i] == -1) {
                index = i;
                break;
            }
        }
    }
    if (index == -1) {
        lock_release(oft->oft_lock);
        vfs_close(vn);
        return EMFILE;
    }
    return index;
}

int read_and_write(int fd, userptr_t buf, size_t nbytes, int *retval, int mode) {
    struct iovec iov;
    struct uio u;

    int of_inter = curproc->fdt->fd_array[fd];
    if (of_inter < 0 || of_inter >= OPEN_MAX) {
        return EBADF;
    }

    
    struct open_file *of = oft->of[of_inter];
    if (of == NULL) {
        return EBADF;
    }

    struct vnode *vn = of->vn;
    int result;
    lock_acquire(oft->oft_lock);
    if (mode == 0) {
        if ((of->flags & O_ACCMODE) == O_WRONLY) {
            lock_release(oft->oft_lock);
            return EBADF;
        }
        uio_uinit(&iov, &u, buf, nbytes, of->offset, UIO_READ);
        result = VOP_READ(vn, &u);
    } else {
        if ((of->flags & O_ACCMODE) == O_RDONLY) {
            lock_release(oft->oft_lock);
            return EBADF;
        }
        uio_uinit(&iov, &u, buf, nbytes, of->offset, UIO_WRITE);
        result = VOP_WRITE(vn, &u);
    }

    if (result) {
        lock_release(oft->oft_lock);
        return result;
    }

    *retval = u.uio_offset - of->offset;
    of->offset = u.uio_offset;
    
    lock_release(oft->oft_lock);

    return 0;
}
