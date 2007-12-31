/*	=================
 *	FSSpecForkUser.cc
 *	=================
 */

#include "Genie/FileSystem/FSSpecForkUser.hh"

// POSIX
#include "fcntl.h"

// Genie
#include "Genie/IO/MacFile.hh"


namespace Genie
{
	
	namespace N = Nitrogen;
	namespace NN = Nucleus;
	
	
	boost::shared_ptr< IOHandle > FSSpecForkUser::OpenFileHandle( const FSSpec& fileSpec, OpenFlags flags ) const
	{
		N::FSIOPermissions rdPerm = N::FSIOPermissions( flags + 1  &  FREAD  );
		N::FSIOPermissions wrPerm = N::FSIOPermissions( flags + 1  &  FWRITE );
		
		bool nonblocking = flags & O_NONBLOCK;
		bool appending   = flags & O_APPEND;
		// ...
		bool creating    = flags & O_CREAT;
		bool truncating  = flags & O_TRUNC;
		bool excluding   = flags & O_EXCL;
		// ...
		
		NN::Owned< N::FSFileRefNum > fileHandle = OpenFork( fileSpec, rdPerm | wrPerm );
		
		if ( truncating )
		{
			N::SetEOF( fileHandle, 0 );
		}
		else if ( appending )
		{
			N::SetFPos( fileHandle, N::fsFromLEOF, 0 );
		}
		
		return NewFileHandle( fileHandle );
	}
	
	boost::shared_ptr< IOHandle > DataForkUser::NewFileHandle( NN::Owned< N::FSFileRefNum > refNum ) const
	{
		return boost::shared_ptr< IOHandle >( new MacDataForkHandle( refNum ) );
	}
	
	boost::shared_ptr< IOHandle > ResourceForkUser::NewFileHandle( NN::Owned< N::FSFileRefNum > refNum ) const
	{
		return boost::shared_ptr< IOHandle >( new MacRsrcForkHandle( refNum ) );
	}
	
}

