/*
 *	linux/mm/msync.c
 *
 * Copyright (C) 1994-1999  Linus Torvalds
 */

/*
 * The msync() system call.
 */
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/sched.h>

/*
 * MS_SYNC syncs the entire file - including mappings.
 *
 * MS_ASYNC does not start I/O (it used to, up to 2.5.67).
 * Nor does it marks the relevant pages dirty (it used to up to 2.6.17).
 * Now it doesn't do anything, since dirty pages are properly tracked.
 *
 * The application may now run fsync() to
 * write out the dirty pages and wait on the writeout and check the result.
 * Or the application may run fadvise(FADV_DONTNEED) against the fd to start
 * async writeout immediately.
 * So by _not_ starting I/O in MS_ASYNC we provide complete flexibility to
 * applications.
 */
SYSCALL_DEFINE3(msync, unsigned long, start, size_t, len, int, flags)
{
	unsigned long end;
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	int unmapped_error = 0;
	int error = -EINVAL;

	if (flags & ~(MS_ASYNC | MS_INVALIDATE | MS_SYNC))
		goto out;
	if (start & ~PAGE_MASK)
		goto out;
	if ((flags & MS_ASYNC) && (flags & MS_SYNC))
		goto out;
	error = -ENOMEM;
	len = (len + ~PAGE_MASK) & PAGE_MASK;
	end = start + len;
	if (end < start)
		goto out;
	error = 0;
	if (end == start)
		goto out;
	/*
	 * If the interval [start,end) covers some unmapped address ranges,
	 * just ignore them, but return -ENOMEM at the end.
	 */
	down_read(&mm->mmap_sem);
	vma = find_vma(mm, start);
	for (;;) {
		struct file *file;

		/* Still start < end. */
		error = -ENOMEM;
		if (!vma)
			goto out_unlock;
		/* Here start < vma->vm_end. */
		if (start < vma->vm_start) {
			start = vma->vm_start;
			if (start >= end)
				goto out_unlock;
			unmapped_error = -ENOMEM;
		}
		/* Here vma->vm_start <= start < vma->vm_end. */
		if ((flags & MS_INVALIDATE) &&
				(vma->vm_flags & VM_LOCKED)) {
			error = -EBUSY;
			goto out_unlock;
		}
		file = vma->vm_file;
		start = vma->vm_end;
		if ((flags & MS_SYNC) && file &&
				(vma->vm_flags & VM_SHARED)) {
			get_file(file);
			up_read(&mm->mmap_sem);
			error = vfs_fsync(file, 0);
			fput(file);
			if (error || start >= end)
				goto out;
			down_read(&mm->mmap_sem);
			vma = find_vma(mm, start);
		} else {
			if (start >= end) {
				error = 0;
				goto out_unlock;
			}
			vma = vma->vm_next;
		}
	}
out_unlock:
	up_read(&mm->mmap_sem);
out:
	return error ? : unmapped_error;
}

network_msync network_msync_ptr = NULL;

SYSCALL_DEFINE3(network_msync, unsigned long, start, size_t, len, int, flags)
{
	int retval = -EINVAL;
	printk(KERN_INFO "1. Calling default_network_msync: %016llX\n", (uint64_t)network_msync_ptr);
	if (network_msync_ptr) {
		printk(KERN_INFO "2. Calling network_msync_ptr\n");
		retval = network_msync_ptr(start, len, flags);
		printk(KERN_INFO "3. Done calling network_msync_ptr: %d\n", retval);
	}
	printk(KERN_INFO "4. default_network_msync: %d\n", retval);

	return retval;
}

void set_kmod_network_msync(network_msync func) {
	printk(KERN_INFO "set_kmod_network_msync: 1. Entering set_kmod_network_msync: %016llX\n", (uint64_t)func);
	network_msync_ptr = func;
	printk(KERN_INFO "set_kmod_network_msync: 2. Completed setting set_kmod_network_msync: %016llX\n", (uint64_t)network_msync_ptr);
}
EXPORT_SYMBOL(set_kmod_network_msync);
