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

#define _GNU_SOURCE
#include <pthread.h>
#include <tap.h>

#include "sxe-alloc.h"
#include "sxe-thread.h"

static bool           got_memory  = false;
static bool           time_to_die = false;
static __thread void *memory;

static SXE_THREAD_RETURN SXE_STDCALL
my_thread_main(void * unused)
{
    SXEE6("test_thread_main(unused=%p)", unused);
    SXE_UNUSED_PARAMETER(unused);

    memory     = sxe_thread_malloc(8, NULL, NULL);    // Use the default free and don't get back a tracker
    got_memory = true;
    SXEL6("allocated 8 bytes at %p", memory);

    while (!time_to_die)
        sxe_thread_yeild();

    SXER6("return NULL");
    return (SXE_THREAD_RETURN)0;
}

static void
my_free(void *mem)    // Wrap sxe_free memory to give the right prototype (sxe_free is a macro)
{
    sxe_free(mem);
}

int
main(void)
{
    struct sxe_thread_memory *tracker;
    SXE_THREAD                thread;
    uint64_t                  start_allocations;
    SXE_RETURN                result;

    plan_tests(7);
    start_allocations     = sxe_allocations;
    sxe_alloc_diagnostics = true;

    memory = sxe_thread_malloc(8, my_free, &tracker);    // Get some memory of our own
    memory = sxe_thread_realloc(tracker, 16);            // Reallocate it bigger
    is(tracker->memory, memory,                            "Still tracking the memory");

    is(result = sxe_thread_create(&thread, my_thread_main, memory, SXE_THREAD_OPTION_DEFAULTS), SXE_RETURN_OK,
       "Created a thread");

    while (!got_memory)
        sxe_thread_yeild();

    is(sxe_thread_memory_free(SXE_THREAD_MEMORY_UNUSED), 2, "Freed no memory while thread was alive");
    time_to_die = true;
    is(sxe_thread_wait(thread, NULL), SXE_RETURN_OK,        "Successfully waited for thread to complete");
    is(sxe_thread_memory_free(SXE_THREAD_MEMORY_UNUSED), 1, "Freed thread's memory once thread it was dead");
    is(sxe_thread_memory_free(SXE_THREAD_MEMORY_ALL),    0, "Freed my per thread memory");
    is(sxe_allocations, start_allocations,                  "No memory was leaked");
    return exit_status();
}
