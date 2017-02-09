/**
  Language: C

  License: BSD License
  (c) HPCN 2014-2017

  Author: Rafael Leira
  E-Mail: rafael.leira@uam.es

  Description: The autoconfigure tool for latency-calibration purposes
*/
#ifndef __LATENCY__AUTOCONF__H__
#define __LATENCY__AUTOCONF__H__

#include <rte_memory.h>

#include "main.h"

void app_autoconf_init (void);
void app_autoconf (void);

#endif