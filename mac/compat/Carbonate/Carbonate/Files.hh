// Carbonate/Files.hh

#ifndef CARBONATE_FILES_HH
#define CARBONATE_FILES_HH

#ifndef __FILES__
#include <Files.h>
#endif

#if TARGET_API_MAC_CARBON
#error Configuration error:  This file is for classic only
#endif

#if TARGET_CPU_68K && TARGET_RT_MAC_CFM

#endif


// We need these definitions under the following conditions:
// 1. Accessors are functions on 68K (no CarbonAccessors.o), for compiling
// 2. Accessors are not functions, for including

// CARBONATE_LINKAGE is only defined (to 'pascal') if we're compiling.

#if defined(CARBONATE_LINKAGE) && ACCESSOR_CALLS_ARE_FUNCTIONS && TARGET_CPU_68K || !defined(CARBONATE_LINKAGE) && !ACCESSOR_CALLS_ARE_FUNCTIONS

#ifndef CARBONATE_LINKAGE
#define CARBONATE_LINKAGE inline pascal
#endif


#undef CARBONATE_LINKAGE

#endif

#endif
