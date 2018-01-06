/* Copyright (c) 2018, David Hauweele <david@hauweele.net>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ftw.h>
#include <err.h>
#include <assert.h>

#include "string-utils.h"
#include "safe-call.h"
#include "common.h"
#include "htable.h"
#include "iobuf.h"
#include "main.h"
#include "scan.h"

#define HT_SIZE 8092

/* hardlinks hashtable with inode
   as key and original path as data. */
static htable_t hardlinks;

/* buffered stdout */
static iofile_t out;

/* options */
static int opt_quiet;

/* Robert Jenkins's integer hash */
uint32_t jenkins_hash(const void *key)
{
  register uint32_t a = (unsigned long long)key;

  a = (a+0x7ed55d16) + (a<<12);
  a = (a^0xc761c23c) ^ (a>>19);
  a = (a+0x165667b1) + (a<<5);
  a = (a+0xd3a2646c) ^ (a<<9);
  a = (a+0xfd7046c5) + (a<<3);
  a = (a^0xb55a4f09) ^ (a>>16);

  return a;
}

static bool int_cmp(const void *k1, const void *k2)
{
  return k1 == k2;
}

static int scan_file(const char *path, const struct stat *stat, int flag, struct FTW *ftw)
{
  static char escaped_buffer[PATH_MAX * 2 + 3]; /* escaping + "" + \0 */
  const char *source_path;
  int n;

  UNUSED(ftw);

  if(strlen(path) > PATH_MAX)
    errx(EXIT_FAILURE, "%s: Path too long", path);

  switch(flag) {
  case FTW_F:
  case FTW_SL:
  case FTW_SLN:
    break;
  case FTW_NS:
  case FTW_DNR:
    if(!opt_quiet)
      /* FIXME: "or cross-mount?" */
      /* FIXME: can't we use st_dev for cross-mount links? */
      warnx("%s: Permission denied", path);
    return 0;
  default:
    return 0;
  }

  /* not a hardlink */
  if(stat->st_nlink < 2)
    return 0;

  /* first encounter -> save
     otherwise display link */
  source_path = ht_search(hardlinks, HT_INT(stat->st_ino), NULL);
  if(!source_path)
    ht_search(hardlinks, HT_INT(stat->st_ino), strdup(path));
  else {
    n = stresc(escaped_buffer, source_path);
    iobuf_write(out, escaped_buffer, n);
    iobuf_putc(' ', out);

    n = stresc(escaped_buffer, path);
    iobuf_write(out, escaped_buffer, n);
    iobuf_putc('\n', out);
  }

  return 0;
}

int scan(const char *index_file, const char *path, int ftw_flags, int flags)
{
  int n;

  /* configure options */
  if(!path)
    path = ".";

  switch(flags) {
  case OPT_QUIET:
    opt_quiet = 1;
    break;
  default:
    break;
  }

  /* re-open buffered stdout */
  if(!index_file)
    out = iobuf_dopen(STDOUT_FILENO);
  else
    out = iobuf_open(index_file, O_WRONLY, 0);
  if(!out)
    errx(EXIT_FAILURE, "cannot re-open stdout");

  /* FIXME: do we really need the jenkins hash?
            wouldn't anything simpler also do? */
  hardlinks = ht_create(HT_SIZE,
                        jenkins_hash,
                        int_cmp,
                        free);
  if(!hardlinks)
    errx(EXIT_FAILURE, "cannot create htable");

  n = nftw(path, scan_file, 1, ftw_flags);
  if(n)
    err(EXIT_FAILURE, "cannot traverse directory");

  iobuf_close(out);
  ht_destroy(hardlinks);

  return 0;
}
