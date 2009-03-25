/*	=============
 *	MemFullErr.cc
 *	=============
 */

// Nitrogen
#include "Nitrogen/MacErrors.h"

// Orion
#include "Orion/Main.hh"


namespace tool
{
	
	int Main( int argc, iota::argv_t argv )
	{
		throw Nitrogen::MemFullErr();
	}
	
}

