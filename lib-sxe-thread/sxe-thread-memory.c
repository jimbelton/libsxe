/* Copyright (c) 2023 Jim Belton
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "sxe-alloc.h"
#include "sxe-thread.h"

static struct sxe_thread_memory *trackers = NULL;                  // Head of the list of per thread memory trackers
static __thread pid_t            tid      = -1;                    // Set to the tid of the thread when it first allocates

/**
 * Allocate memory and tracker memory whose address is to be stored in a per thread pointer so that the main thread can free it
 *
 * @param size        Number of bytes to allocate
 * @param obj_free    A free function to call on the allocated object before freeing, or NULL to call sxe_free
 * @param tracker_out If not NULL, set to the address of an allocated tracking structure; need if memory is to be realloced
 *
 * @return Pointer to the memory allocated or NULL on failure to allocate
 *
 * @note The implementation of thread identity that is safe to use from another thread is very Linux specific
 */
void *
sxe_thread_malloc(size_t size, void (*obj_free)(void *), struct sxe_thread_memory **tracker_out)
{
    struct sxe_thread_memory *tracker = sxe_malloc(sizeof(**tracker_out));

    if (tracker && (tracker->memory = sxe_malloc(size))) {    // If tracker and memory were allocated
        if (tid < 0) {
            tid = syscall(SYS_gettid);                        // gettid() is not available in glibc as of Debian 11
            SXEL7(": first call from tid %d", tid);
        }

        tracker->tid  = tid;
        tracker->free = obj_free;
        tracker->next = trackers;

        /* If first tracker in list remains unchanged by another thread, atomically replace it with tracker.
         */
        while (!__sync_bool_compare_and_swap(&trackers, tracker->next, tracker))
            tracker->next = trackers;    /* COVERAGE EXCLUSION: this only gets hit in a race condition */

        if (tracker_out)
            *tracker_out = tracker;
    }

    return tracker->memory;
}

/**
 * Reallocate tracked per thread memory
 *
 * @param tracker A tracking structure previously returned from sxe_thread_malloc
 * @param size    Number of bytes to reallocate to
 *
 * @return Pointer to the reallocated memory or NULL on failure to reallocate
 */
void *
sxe_thread_realloc(struct sxe_thread_memory *tracker, size_t size)
{
    void *memory = sxe_realloc(tracker->memory, size);

    if (memory)
        tracker->memory = memory;

    return memory;
}

/**
 * Free per thread memory for any threads that are no longer alive
 *
 * @param One of SXE_THREAD_MEMORY_UNUSED or SXE_THREAD_MEMORY_ALL (to include memory from the calling thread)
 *
 * @return the number of tracked per thread memory allocations remaining unfreed
 *
 * @note This function should only be called from the main thread
 */
unsigned
sxe_thread_memory_free(unsigned what)
{
    struct stat               status;
    struct sxe_thread_memory *tracker;
    struct sxe_thread_memory *next;
    struct sxe_thread_memory *keepers = NULL;    // List of tackers not freed
    struct sxe_thread_memory *last    = NULL;    // Last tracker in keepers list
    unsigned                  unfreed = 0;
    int                       ret;
    bool                      reap;
    char                      task_dir[64];

    SXEE6("(what=%s)", what == SXE_THREAD_MEMORY_UNUSED ? "UNUSED" : "ALL");

    /* Atomically acquire the list of trackers
     */
    for (tracker = trackers; !__sync_bool_compare_and_swap(&trackers, tracker, NULL); )
        tracker = trackers;    /* COVERAGE EXCLUSION: this only gets hit in a race condition */

    for (; tracker; tracker = next) {    // For each tracker in the list
        next = tracker->next;
        reap = false;

        if (tid == tracker->tid)    // If it's the calling thread, only reap it's memory if freeing ALL
            reap = what == SXE_THREAD_MEMORY_ALL;
        else {    // Otherwise, if the threads task file is not present
            snprintf(task_dir, sizeof(task_dir), "/proc/%d/task/%d", getpid(), tracker->tid);
            reap = (ret = stat(task_dir, &status)) < 0 && errno == ENOENT;
        }

        /* If not attempting to free all per thread memory and memory was allocated by this thread, or the allocating thread is
         * not dead, or an error occurred trying to verify the status of the thread, don't free the memory
         */
        if (!reap) {
            if (tid != tracker->tid) {
                if (ret >= 0)
                    SXEL7(": thread %d is alive", tracker->tid);
                else
                    SXEL2(": Can't stat %s; error: %s", task_dir, strerror(errno));    /* COVERAGE EXCLUSION: Shouldn't happen */
            }

            last          = last ?: tracker;
            tracker->next = keepers;
            keepers       = tracker;
            unfreed++;
            continue;
        }

        if (tid != tracker->tid)
            SXEL7(": thread %d is dead", tracker->tid);

        if (tracker->free)
            (*tracker->free)(tracker->memory);
        else
            sxe_free(tracker->memory);

        sxe_free(tracker);
    }

    if (keepers) {
        tracker = trackers;

        /* Attempt to replace trackers list with keepers followed by trackers, retrying if another thread mucks with trackers
         */
        for (last->next = tracker; !__sync_bool_compare_and_swap(&trackers, tracker, keepers); last->next = tracker)
            tracker = trackers;    /* COVERAGE EXCLUSION: this only gets hit in a race condition */
    }

    SXER6("return unfreed=%u", unfreed);
    return unfreed;
}
