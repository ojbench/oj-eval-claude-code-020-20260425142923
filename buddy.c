#include "buddy.h"
#include <stdlib.h>
#define NULL ((void *)0)

#define PAGE_SIZE (4096)
#define MAX_RANK (16)

// Simple buddy system implementation
// We'll track allocation of each page
static void *base_addr = NULL;
static int total_pages = 0;
static char *allocated = NULL;  // 0=free, 1=allocated

static void *page_to_addr(int page) {
    if (!base_addr || page < 0 || page >= total_pages) {
        return NULL;
    }
    return base_addr + page * PAGE_SIZE;
}

static int addr_to_page(void *addr) {
    if (!base_addr || addr < base_addr ||
        addr >= base_addr + total_pages * PAGE_SIZE) {
        return -1;
    }
    return (addr - base_addr) / PAGE_SIZE;
}

int init_page(void *p, int pgcount) {
    if (!p || pgcount <= 0) return -EINVAL;

    base_addr = p;
    total_pages = pgcount;

    // Allocate tracking array
    allocated = calloc(total_pages, sizeof(char));
    if (!allocated) return -ENOSPC;

    // All pages start as free
    for (int i = 0; i < total_pages; i++) {
        allocated[i] = 0;
    }

    return OK;
}

void *alloc_pages(int rank) {
    if (rank < 1 || rank > MAX_RANK) return ERR_PTR(-EINVAL);

    int size = 1 << (rank - 1);

    // For rank 1, we need to return consecutive pages
    if (rank == 1) {
        for (int i = 0; i < total_pages; i++) {
            if (!allocated[i]) {
                allocated[i] = 1;
                return page_to_addr(i);
            }
        }
    } else {
        // For higher ranks, find consecutive free pages
        for (int i = 0; i <= total_pages - size; i++) {
            int all_free = 1;
            for (int j = 0; j < size; j++) {
                if (allocated[i + j]) {
                    all_free = 0;
                    break;
                }
            }
            if (all_free) {
                // Mark all as allocated
                for (int j = 0; j < size; j++) {
                    allocated[i + j] = 1;
                }
                return page_to_addr(i);
            }
        }
    }

    return ERR_PTR(-ENOSPC);
}

int return_pages(void *p) {
    if (!p) return -EINVAL;

    int page = addr_to_page(p);
    if (page < 0 || page >= total_pages || !allocated[page]) {
        return -EINVAL;
    }

    // Find the size of the allocated block by checking consecutive allocated pages
    int size = 1;
    int max_size = total_pages - page;

    // Find the maximum power of 2 block size
    for (int rank = 1; rank <= MAX_RANK && size <= max_size; rank++) {
        int block_size = 1 << (rank - 1);
        if (page + block_size > total_pages) break;

        int all_allocated = 1;
        for (int i = 0; i < block_size; i++) {
            if (!allocated[page + i]) {
                all_allocated = 0;
                break;
            }
        }

        if (all_allocated) {
            size = block_size;
        } else {
            break;
        }
    }

    // Free the pages
    for (int i = 0; i < size; i++) {
        allocated[page + i] = 0;
    }

    return OK;
}

int query_ranks(void *p) {
    if (!p) return -EINVAL;

    int page = addr_to_page(p);
    if (page < 0 || page >= total_pages) {
        return -EINVAL;
    }

    // Find the largest rank for which this page belongs to a completely free block
    for (int rank = MAX_RANK; rank >= 1; rank--) {
        int size = 1 << (rank - 1);
        int block_start = (page / size) * size;

        // Check if block is within bounds
        if (block_start + size > total_pages) {
            continue;
        }

        // Check if all pages in block are free
        int all_free = 1;
        for (int i = 0; i < size; i++) {
            if (allocated[block_start + i]) {
                all_free = 0;
                break;
            }
        }

        if (all_free) {
            return rank;
        }
    }

    return 1;
}

int query_page_counts(int rank) {
    if (rank < 1 || rank > MAX_RANK) return -EINVAL;

    if (rank == 1) {
        // Count individual free pages
        int count = 0;
        for (int i = 0; i < total_pages; i++) {
            if (!allocated[i]) {
                count++;
            }
        }
        return count;
    }

    // For larger ranks, count how many blocks of this size are completely free
    int size = 1 << (rank - 1);
    int count = 0;

    for (int i = 0; i <= total_pages - size; i += size) {
        int all_free = 1;
        for (int j = 0; j < size; j++) {
            if (allocated[i + j]) {
                all_free = 0;
                break;
            }
        }
        if (all_free) {
            count++;
            // Skip over this block to avoid double counting
            i += size - 1;
        }
    }

    return count;
}
