/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Eric van Gyzen
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/mman.h>

#include <machine/sysarch.h>

#include <atf-c.h>
#include <errno.h>
#include <stdio.h>

#define MEM_SIZE (2*1024*1024)

static char *
pkru_super_setup(void)
{
	char *mem;

	if (x86_pkru_set_perm(1, 1, 0)) {
		if (errno == EOPNOTSUPP) {
			atf_tc_skip("requires PKRU support");
		}
		ATF_REQUIRE_MSG(false, "x86_pkru_set_perm: %s",
		    strerror(errno));
	}

	mem = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANON | MAP_ALIGNED(21), -1, 0);
	ATF_REQUIRE_MSG(mem != MAP_FAILED, "mmap: %s", strerror(errno));

	return (mem);
}

static void
pkru_super_test(char *mem, bool super_expected)
{
	char incore[MEM_SIZE / PAGE_SIZE];
	int sum;

	sum = 0;
	for (int i = 0; i < MEM_SIZE; i += PAGE_SIZE) {
		sum += mem[i];
	}
	// Prevent dead-store warnings or excessive optimization.
	printf("sum %d\n", sum);

	ATF_REQUIRE_EQ_MSG(mincore(mem, MEM_SIZE, incore), 0,
	    "%s", strerror(errno));
	ATF_CHECK_EQ(incore[0] & MINCORE_SUPER,
	     super_expected ? MINCORE_SUPER : 0);
	ATF_REQUIRE_EQ_MSG(munmap(mem, MEM_SIZE), 0, "%s", strerror(errno));
}

#if 0
ATF_TC_WITHOUT_HEAD(pkru_super_default);
ATF_TC_BODY(pkru_super_default, tc)
{
	char *mem;

	mem = pkru_super_setup();
	// Keep the default PKRU protections.
	pkru_super_test(mem, true);
}
#endif

ATF_TC_WITHOUT_HEAD(pkru_super_first_page);
ATF_TC_BODY(pkru_super_first_page, tc)
{
	char *mem;

	mem = pkru_super_setup();
	ATF_REQUIRE_EQ_MSG(0,
	    x86_pkru_protect_range(mem, PAGE_SIZE, 1, 0),
	    "x86_pkru_protect_range: %s", strerror(errno));
	pkru_super_test(mem, false);
}

ATF_TC_WITHOUT_HEAD(pkru_super_second_page);
ATF_TC_BODY(pkru_super_second_page, tc)
{
	char *mem;

	mem = pkru_super_setup();
	ATF_REQUIRE_EQ_MSG(0,
	    x86_pkru_protect_range(mem + PAGE_SIZE, PAGE_SIZE, 1, 0),
	    "x86_pkru_protect_range: %s", strerror(errno));
	pkru_super_test(mem, false);
}

#if 0
ATF_TC_WITHOUT_HEAD(pkru_super_whole_range);
ATF_TC_BODY(pkru_super_whole_range, tc)
{
	char *mem;

	mem = pkru_super_setup();
	ATF_REQUIRE_EQ_MSG(0,
	    x86_pkru_protect_range(mem, MEM_SIZE, 1, 0),
	    "x86_pkru_protect_range: %s", strerror(errno));
	pkru_super_test(mem, true);
}
#endif

ATF_TP_ADD_TCS(tp)
{
	// This test does not pass yet.  MINCORE_SUPER is not set,
	// though it should be.
	// ATF_TP_ADD_TC(tp, pkru_super_default);
	ATF_TP_ADD_TC(tp, pkru_super_first_page);
	ATF_TP_ADD_TC(tp, pkru_super_second_page);
	// Ditto pkru_super_default.
	// ATF_TP_ADD_TC(tp, pkru_super_whole_range);

	return (atf_no_error());
}
