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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <err.h>
#include <ftw.h>

#include "string-utils.h"
#include "version.h"
#include "restore.h"
#include "scan.h"
#include "help.h"
#include "main.h"

static void print_help(const char *name)
{
  struct opt_help messages[] = {
    { 'h', "help",    "Show this help message" },
    { 'V', "version", "Show version information" },
    { 'v', "verbose", "Be a bit more verbose" },
#ifdef COMMIT
    { 0,   "commit",  "Display commit information" },
#endif /* COMMIT */
    { 'F', "follow",  "Follow symlinks."},
    { 'q', "quiet",   "Do not warning messages while scanning" },
    { 'f', "force",   "Do not abort on restore error" },
    { 0, NULL, NULL }
  };

  help(name, "[options] scan|restore [path]", messages);
}

int main(int argc, char *argv[])
{
  const char *prog_name;
  const char *command;
  const char *path = NULL;
  int exit_status  = EXIT_FAILURE;
  int ftw_flags    = FTW_PHYS | FTW_MOUNT;
  int flags        = 0;

  enum opt {
    OPT_COMMIT = 0x100
  };

  struct option opts[] = {
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'V' },
    { "verbose", no_argument, NULL, 'v' },
#ifdef COMMIT
    { "commit", no_argument, NULL, OPT_COMMIT },
#endif /* COMMIT */
    { "follow", no_argument, NULL, 'F' },
    { "quiet", no_argument, NULL, 'q' },
    { "force", no_argument, NULL, 'f' },
    { NULL, 0, NULL, 0 }
  };

#ifdef __Linux__
  setproctitle_init(argc, argv, environ); /* libbsd needs that */
#endif
  prog_name = basename(argv[0]);

  while(1) {
    int c = getopt_long(argc, argv, "hVvFqf", opts, NULL);

    if(c == -1)
      break;

    switch(c) {
    case 'F':
      ftw_flags &= ~FTW_PHYS;
      break;
    case 'v':
      flags |= OPT_VERBOSE;
      break;
    case 'q':
      flags |= OPT_QUIET;
      break;
    case 'f':
      flags |= OPT_FORCE;
      break;
    case 'r':
      flags |= OPT_REMOVE;
      break;
    case 'V':
      version();
      exit_status = EXIT_SUCCESS;
      goto EXIT;
#ifdef COMMIT
    case OPT_COMMIT:
      commit();
      exit_status = EXIT_SUCCESS;
      goto EXIT;
#endif /* COMMIT */
    case 'h':
      exit_status = EXIT_SUCCESS;
    default:
      print_help(prog_name);
      goto EXIT;
    }
  }

  argc -= optind;
  argv += optind;

  switch(argc) {
  case 2:
    path    = argv[1];
  case 1:
    command = argv[0];
    break;
  default:
    print_help(prog_name);
    goto EXIT;
  }

  if(!strcmp(command, "scan"))
    exit_status = scan(path, ftw_flags, flags);
  else if(!strcmp(command, "restore"))
    exit_status = restore(path, flags);
  else
    errx(EXIT_FAILURE, "unknown command (use 'scan' or 'restore')");

EXIT:
  exit(exit_status);
}