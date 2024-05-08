/*
 * drivers/staging/android/ion/ion.h
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2016, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_ION_H
#define _LINUX_ION_H

#include <linux/err.h>
#include "../uapi/ion.h"

struct ion_handle;
struct ion_device;
struct ion_heap;
struct ion_mapper;
struct ion_client;
struct ion_buffer;

/*
 * This should be removed some day when phys_addr_t's are fully
 * plumbed in the kernel, and all instances of ion_phys_addr_t should
 * be converted to phys_addr_t.  For the time being many kernel interfaces
 * do not accept phys_addr_t's that would have to
 */
#define ion_phys_addr_t dma_addr_t

/**
 * struct ion_platform_heap - defines a heap in the given platform
 * @type:	type of the heap from ion_heap_type enum
 * @id:		unique identifier for heap.  When allocating higher numbers
 *		will be allocated from first.  At allocation these are passed
 *		as a bit mask and therefore can not exceed ION_NUM_HEAP_IDS.
 * @name:	used for debug purposes
 * @base:	base address of heap in physical memory if applicable
 * @size:	size of the heap in bytes if applicable
 * @has_outer_cache:    set to 1 if outer cache is used, 0 otherwise.
 * @extra_data:	Extra data specific to each heap type
 * @priv:	heap private data
 * @align:	required alignment in physical memory if applicable
 * @priv:	private info passed from the board file
 *
 * Provided by the board file.
 */
struct ion_platform_heap {
	enum ion_heap_type type;
	unsigned int id;
	const char *name;
	ion_phys_addr_t base;
	size_t size;
	unsigned int has_outer_cache;
	void *extra_data;
	ion_phys_addr_t align;
	void *priv;
};

/**
 * struct ion_platform_data - array of platform heaps passed from board file
 * @has_outer_cache:    set to 1 if outer cache is used, 0 otherwise.
 * @nr:    number of structures in the array
 * @heaps: array of platform_heap structions
 *
 * Provided by the board file in the form of platform data to a platform device.
 */
struct ion_platform_data {
	unsigned int has_outer_cache;
	int nr;
	struct ion_platform_heap *heaps;
};

#ifdef CONFIG_ION

/**
 * struct ion_buffer - metadata for a particular buffer
 * @ref:		reference count
 * @node:		node in the ion_device buffers tree
 * @dev:		back pointer to the ion_device
 * @heap:		back pointer to the heap the buffer came from
 * @flags:		buffer specific flags
 * @private_flags:	internal buffer specific flags
 * @size:		size of the buffer
 * @priv_virt:		private data to the buffer representable as
 *			a void *
 * @lock:		protects the buffers cnt fields
 * @kmap_cnt:		number of times the buffer is mapped to the kernel
 * @vaddr:		the kernel mapping if kmap_cnt is not zero
 * @sg_table:		the sg table for the buffer if dmap_cnt is not zero
 * @vmas:		list of vma's mapping this buffer
 */
struct ion_dma_buf_attachment;
struct ion_buffer {
	struct ion_heap *heap;
	struct sg_table *sg_table;
	struct ion_dma_buf_attachment *attachments;
	struct list_head map_freelist;
	void *priv_virt;
	void *vaddr;
	unsigned int flags;
	unsigned int private_flags;
	size_t size;
	int kmap_refcount;
};

void ion_buffer_destroy(struct ion_buffer *buffer);

/* refer to include/linux/pm.h */
struct ion_pm_ops {
	int (*freeze)(struct ion_heap *heap);
	int (*restore)(struct ion_heap *heap);
};

/**
 * struct ion_heap_ops - ops to operate on a given heap
 * @allocate:		allocate memory
 * @free:		free memory
 * @map_kernel		map memory to the kernel
 * @unmap_kernel	unmap memory to the kernel
 * @map_user		map memory to userspace
 *
 * Calls memblock reserve to set aside memory for heaps that are
 * located at specific memory addresses or of specific sizes not
 * managed by the kernel
 */
void ion_reserve(struct ion_platform_data *data);

/**
 * ion_client_create() -  allocate a client and returns it
 * @dev:		the global ion device
 * @name:		used for debugging
 */
struct ion_client *ion_client_create(struct ion_device *dev,
				     const char *name);

/**
 * ion_client_destroy() -  free's a client and all it's handles
 * @client:	the client
 *
 * Free the provided client and all it's resources including
 * any handles it is holding.
 */
struct ion_heap {
	enum ion_heap_type type;
	struct ion_heap_ops *ops;
	unsigned long flags;
	unsigned int id;
	const char *name;
	void *priv;
	struct workqueue_struct *wq;
};

/**
 * ion_alloc - allocate ion memory
 * @client:		the client
 * @len:		size of the allocation
 * @align:		requested allocation alignment, lots of hardware blocks
 *			have alignment requirements of some kind
 * @heap_id_mask:	mask of heaps to allocate from, if multiple bits are set
 *			heaps will be tried in order from highest to lowest
 *			id
 * @flags:		heap flags, the low 16 bits are consumed by ion, the
 *			high 16 bits are passed on to the respective heap and
 *			can be heap custom
 *
 * Allocate memory in one of the heaps provided in heap mask and return
 * an opaque handle to it.
 */
static inline bool ion_buffer_cached(struct ion_buffer *buffer)
{
	return buffer->flags & ION_FLAG_CACHED;
}

/**
 * ion_free - free a handle
 * @client:	the client
 * @handle:	the handle to free
 *
 * Free the provided handle.
 */
void ion_free(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_phys - returns the physical address and len of a handle
 * @client:	the client
 * @handle:	the handle
 * @addr:	a pointer to put the address in
 * @len:	a pointer to put the length in
 *
 * This function queries the heap for a particular handle to get the
 * handle's physical address.  It't output is only correct if
 * a heap returns physically contiguous memory -- in other cases
 * this api should not be implemented -- ion_sg_table should be used
 * instead.  Returns -EINVAL if the handle is invalid.  This has
 * no implications on the reference counting of the handle --
 * the returned value may not be valid if the caller is not
 * holding a reference.
 */
int ion_phys(struct ion_client *client, struct ion_handle *handle,
	     ion_phys_addr_t *addr, size_t *len);

/**
 * ion_device_add_heap - adds a heap to the ion device
 * @dev:		the device
 * @heap:		the heap to add
 */
void ion_device_add_heap(struct ion_device *idev, struct ion_heap *heap);

/**
 * some helpers for common operations on buffers using the sg_table
 * and vaddr fields
 */
void *ion_heap_map_kernel(struct ion_heap *heap, struct ion_buffer *buffer);
void ion_heap_unmap_kernel(struct ion_heap *heap, struct ion_buffer *buffer);
int ion_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
		      struct vm_area_struct *vma);
int ion_heap_buffer_zero(struct ion_buffer *buffer);
int ion_heap_pages_zero(struct page *page, size_t size, pgprot_t pgprot);

/**
 * ion_heap_init_shrinker
 * @heap:		the heap
 *
 * This function returns the sg_table describing
 * a particular ion handle.
 */
struct sg_table *ion_sg_table(struct ion_client *client,
			      struct ion_handle *handle);

/**
 * functions for creating and destroying the built in ion heaps.
 * architectures can add their own custom architecture specific
 * heaps as appropriate.
 */
struct ion_handle *ion_import_dma_buf_fd(struct ion_client *client, int fd);

#else
static inline void ion_reserve(struct ion_platform_data *data) {}

static inline struct ion_client *ion_client_create(
	struct ion_device *dev, unsigned int heap_id_mask, const char *name)
{
	return ERR_PTR(-ENODEV);
}

static inline void ion_client_destroy(struct ion_client *client) {}

static inline struct ion_handle *ion_alloc(struct ion_client *client,
					   size_t len, size_t align,
					   unsigned int heap_id_mask,
					   unsigned int flags)
{
	return ERR_PTR(-ENODEV);
}

static inline void ion_free(struct ion_client *client,
			    struct ion_handle *handle) {}

static inline int ion_phys(struct ion_client *client, struct ion_handle *handle,
			   ion_phys_addr_t *addr, size_t *len)
{
	return -ENODEV;
}

static inline struct sg_table *ion_sg_table(struct ion_client *client,
					    struct ion_handle *handle)
{
	return ERR_PTR(-ENODEV);
}

static inline void *ion_map_kernel(struct ion_client *client,
				   struct ion_handle *handle)
{
	return ERR_PTR(-ENODEV);
}

/**
 * struct ion_page_pool - pagepool struct
 * @high_count:		number of highmem items in the pool
 * @low_count:		number of lowmem items in the pool
 * @high_items:		list of highmem items
 * @low_items:		list of lowmem items
 * @mutex:		lock protecting this struct and especially the count
 *			item list
 * @gfp_mask:		gfp_mask to use from alloc
 * @order:		order of pages in the pool
 * @list:		plist node for list of pools
 * @cached:		it's cached pool or not
 *
 * Allows you to keep a pool of pre allocated pages to use from your heap.
 * Keeping a pool of pages that is ready for dma, ie any cached mapping have
 * been invalidated from the cache, provides a significant performance benefit
 * on many systems
 */
struct ion_page_pool {
	int high_count;
	int low_count;
	bool cached;
	struct list_head high_items;
	struct list_head low_items;
	/* Protect the pool */
	spinlock_t lock;
	gfp_t gfp_mask;
	unsigned int order;
	struct plist_node list;
};

static inline int ion_share_dma_buf(struct ion_client *client,
				    struct ion_handle *handle)
{
	return -ENODEV;
}

static inline struct ion_handle *ion_import_dma_buf(struct ion_client *client,
						    int fd)
{
	return ERR_PTR(-ENODEV);
}

static inline int ion_handle_get_flags(struct ion_client *client,
				       struct ion_handle *handle,
				       unsigned long *flags)
{
	return -ENODEV;
}

/**
 * ion_pages_sync_for_device - cache flush pages for use with the specified
 *                             device
 * @dev:		the device the pages will be used with
 * @page:		the first page to be flushed
 * @size:		size in bytes of region to be flushed
 * @dir:		direction of dma transfer
 */
static inline void ion_pages_sync_for_device(struct device *dev,
					     struct page *page, size_t size,
					     enum dma_data_direction dir)
{
	struct scatterlist sg;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, page, size, 0);
	sg_dma_address(&sg) = page_to_phys(page);
	dma_sync_sg_for_device(dev, &sg, 1, dir);
}
#endif /* CONFIG_ION */
#endif /* _ION_H */
