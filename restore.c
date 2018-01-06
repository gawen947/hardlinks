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
#include <string.h>
#include <err.h>

#include "string-utils.h"
#include "safe-call.h"
#include "iobuf.h"

#define BUFFER_SIZE (PATH_MAX * 2 + 6) /* "<src>" "<dst>"\n */

static void restore_file(const char *path, const char *src, const char *dst, int flags)
{
  xunlink(dst);
  xlink(src, dst);
}

int restore(const char *path, int flags)
{
  char read_buf[BUFFER_SIZE];
  char src_buf[BUFFER_SIZE];
  char dst_buf[BUFFER_SIZE];
  const char *s;
  iofile_t in;
  ssize_t  n;

  /* re-open buffered stdin */
  in = iobuf_dopen(STDIN_FILENO);
  if(!in)
    errx(EXIT_FAILURE, "cannot re-open stdin");

  while((n = iobuf_gets(in, read_buf, BUFFER_SIZE))) {
    if(n < 0)
      err(EXIT_FAILURE, "read error");

    /* strip newline */
    read_buf[n - 1] = '\0';

    /* all link are in the format: "<src>" "<dst>" */
    s = strunesc(src_buf, read_buf);

    if(*s++ != ' ')
      errx(EXIT_FAILURE, "'%s': Invalid line", read_buf);

    s = strunesc(dst_buf, s);

    if(*s != '\0')
      warnx("'%s': Garbage after line", read_buf);

    restore_file(path, src_buf, dst_buf, flags);
  }

  iobuf_close(in);

  return 0;
}
