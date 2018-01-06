/* Copyright (c) 2016-2018, David Hauweele <david@hauweele.net>
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

const char * basename(const char *s)
{
  const char *base = (const char *)strrchr(s, '/');
  base = base ? (base + 1) : s;
  return base;
}

unsigned int stresc(char *buf, const char *str)
{
  char *b       = buf;
  const char *s = str;

  *b++ = '\"';

  while(*s) {
    switch(*s) {
    case '\"':
    case '\\':
      *b++ = '\\';
    default:
      *b++ = *s++;
      break;
    }
  }

  *b++ = '\"';
  *b   = '\0';

  return b - buf;
}

const char * strunesc(char *buf, const char *str)
{
  char       *b = buf;
  const char *s = str;

  if(*s != '\"')
    goto ERROR;
  s++;

  while(*s) {
    switch(*s) {
    case '\"':
      goto EXIT;
    case '\\':
      s++;
    default:
      *b++ = *s;
      s++;
      break;
    }
  }

ERROR:
  errx(EXIT_FAILURE, "cannot parse string '%s'", str);

EXIT:
  s++;
  *b = '\0';
  return s;
}

