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

/* Ultra minimal emulation of BSD function. */

#include <string.h>
#include "fnmatch.h"

int
fnmatch(const char * pattern, const char * buffer, int unused)
{
	char * star;
	int    len;

    (void)unused; /* keep quiet mingw32-gcc.exe */

	/* If there is a leading subpattern.
	 */
	if ((star = strchr(pattern, '*')) != NULL) {
		len = star - pattern;
		if (strncmp(pattern, buffer, len) != 0) {
			return 1;
		}

		pattern += len + 1;
		buffer  += len;
	}

	/* If the star matches anything, skip it.
	 */
	if ((len = strlen(buffer) - strlen(pattern)) > 0) {
		buffer += len;
	}

	return strcmp(buffer, pattern);
}
