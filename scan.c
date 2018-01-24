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

#ifdef __linux__
# define _XOPEN_SOURCE 500
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ftw.h>
#include <err.h>
#include <assert.h>

#include <gawen/safe-call.h>
#include <gawen/string.h>
#include <gawen/htable.h>
#include <gawen/common.h>
#include <gawen/iobuf.h>

#include "main.h"
#include "scan.h"

#define HT_SIZE 8092
#define KEY_ALLOC_BLOCK_SIZE 1024

struct hardlink {
  __dev_t st_dev; /* hardlink inode's device */
  ino_t   st_ino; /* hardlink inode's number */
};

/* We use our own memory allocator for the keys
   so we don't rely too much on malloc/free. */
struct key_alloc_block {
  struct key_alloc_block *next;
  struct hardlink block[KEY_ALLOC_BLOCK_SIZE];
};

static struct key_alloc_block *alloc_head;
static int alloc_idx;

/* hardlinks hashtable with inode
   as key and original path as data. */
static htable_t hardlinks;

/* buffered stdout */
static iofile_t out;

/* options */
static int opt_quiet;

/* Dan Bernstein's hash */
uint32_t djb2_hardlink_hash(const void *key)
{
  const char *o = key;
  register uint32_t hash = 5381;
  unsigned int i;

  for(i = 0 ; i < sizeof(sizeof(struct hardlink)); i++)
    hash = ((hash << 5) + hash) + o[i]; /* hash * 33 + c */

  return hash;
}

static bool hardlink_cmp(const void *k1, const void *k2)
{
  const struct hardlink *hl_k1 = k1;
  const struct hardlink *hl_k2 = k2;

  return hl_k1->st_ino == hl_k2->st_ino;
}

static inline const struct hardlink * create_temporary_key(const struct stat *stat)
{
  alloc_head->block[alloc_idx] = (struct hardlink){ .st_dev = stat->st_dev, .st_ino = stat->st_ino };
  return &alloc_head->block[alloc_idx];
}

static inline void commit_key(void)
{
  if(alloc_idx < KEY_ALLOC_BLOCK_SIZE)
    alloc_idx++;
  else {
    struct key_alloc_block *alloc_new = xmalloc(sizeof(struct key_alloc_block));

    alloc_new->next = alloc_head;
    alloc_head     = alloc_new;
    alloc_idx      = 0;
  }
}

static void init_keys(void)
{
  alloc_head      = xmalloc(sizeof(struct key_alloc_block));
  alloc_head->next = NULL;
  alloc_idx       = 0;
}

static void free_keys(void)
{
  struct key_alloc_block *alloc;

  while(alloc_head) {
    alloc      = alloc_head;
    alloc_head = alloc_head->next;
    free(alloc);
  }
}

static int scan_file(const char *path, const struct stat *stat, int flag, struct FTW *ftw)
{
  static char escaped_buffer[PATH_MAX * 2 + 3]; /* escaping + "" + \0 */
  const char *source_path;
  const struct hardlink *devino;
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

  /* create hardlink key */
  devino = create_temporary_key(stat);

  /* first encounter -> save
     otherwise display link */
  source_path = ht_search(hardlinks, devino, NULL);
  if(!source_path) {
    ht_search(hardlinks, devino, strdup(path));
    commit_key();
  }
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

  /* init keys allocator */
  init_keys();

  hardlinks = ht_create(HT_SIZE,
                        djb2_hardlink_hash,
                        hardlink_cmp,
                        free);
  if(!hardlinks)
    errx(EXIT_FAILURE, "cannot create htable");

  n = nftw(path, scan_file, 1, ftw_flags);
  if(n)
    err(EXIT_FAILURE, "cannot traverse directory");

  iobuf_close(out);
  ht_destroy(hardlinks);
  free_keys();

  return 0;
}
