/* Copyright (c) 2010 Sophos Group.
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

#ifndef __SXE_THREAD_H__
#define __SXE_THREAD_H__

#include "sxe-log.h"
#include "sxe-socket.h"

#define SXE_THREAD_OPTION_DEFAULTS 0

#ifdef _WIN32
#   include <windows.h>
#   define __thread   __declspec( thread )
    typedef HANDLE    SXE_THREAD;
    typedef DWORD     SXE_THREAD_RETURN;

#else
#   include <pthread.h>
    typedef pthread_t SXE_THREAD;
    typedef void *    SXE_THREAD_RETURN;
#endif

#define SXE_THREAD_MEMORY_UNUSED 1                           // Free thread memory of dead threads
#define SXE_THREAD_MEMORY_ALL    2                           // Free thread memory of dead threads and the current thread

struct sxe_thread_memory {
    void                     *memory;                        // Allocated memory
    void                    (*free)(void *);                 // Function to call to free memory or NULL to call sxe_free
    struct sxe_thread_memory *next;                          // Pointer to next tracker or NULL
    pid_t                     tid;                           // The tid of the thread that allocated the memory
};

#include "lib-sxe-thread-proto.h"

#endif
