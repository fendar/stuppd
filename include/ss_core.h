#ifndef __SS_CORE_H__
#define __SS_CORE_H__ 1
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>         //dirname() call
#include "ss_log.h"
#include "ss_file.h"
#include "ss_config.h"
#include "ss_str.h"
#include "ss_time.h"
#include "ss_mem.h"
#include "ss_event.h"
#include "ss_socket.h"

extern ss_log_t     errlog;
extern ss_map_t     *fileconf;

#endif
