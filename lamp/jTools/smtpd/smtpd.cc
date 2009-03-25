/*	========
 *	smtpd.cc
 *	========
 */

// Standard C++
#include <algorithm>
#include <functional>
#include <list>
#include <sstream>
#include <string>
#include <vector>

// Standard C/C++
#include <cstdlib>

// POSIX
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

// Iota
#include "iota/strings.hh"

// Nucleus
#include "Nucleus/Shared.h"

// Io
#include "io/io.hh"
#include "io/spew.hh"

// Nitrogen
#include "Nitrogen/DateTimeUtils.h"
#include "Nitrogen/Folders.h"

// Io: MacFiles
#include "MacFiles.hh"

// POSeven
#include "POSeven/FileDescriptor.hh"
#include "POSeven/types/exit_t.hh"

// MoreFunctional
#include "FunctionalExtensions.hh"
#include "PointerToFunction.hh"

// Nitrogen Extras / Iteration
#include "Iteration/FSContents.h"

// Io
#include "Io/TextInput.hh"

// BitsAndBytes
#include "DecimalStrings.hh"
#include "StringFilters.hh"

// Orion
#include "Orion/Main.hh"


namespace io
{
	
	inline FSSpec path_descent( const Nitrogen::FSDirSpec& dir, const unsigned char* name )
	{
		return Nitrogen::FSMakeFSSpec( dir, name );
	}
	
	inline FSSpec path_descent( const FSSpec& dir, const unsigned char* name )
	{
		return path_descent( Nucleus::Convert< Nitrogen::FSDirSpec >( dir ), name );
	}
	
}

namespace Nitrogen
{
	
	struct RecursiveFSDeleter
	{
		void operator()( const FSDirSpec& dir ) const
		{
			io::recursively_delete_directory( dir );
		}
	};
	
}

namespace Nucleus
{
	
	template <>
	struct LivelinessTraits< Nitrogen::FSDirSpec, Nitrogen::RecursiveFSDeleter >   { typedef SeizedValuesAreLive LivelinessTest; };
	
}

namespace tool
{
	
	namespace N = Nitrogen;
	namespace NN = Nucleus;
	namespace p7 = poseven;
	
	using namespace io::path_descent_operators;
	
	using BitsAndBytes::EncodeDecimal2;
	using BitsAndBytes::EncodeDecimal4;
	using BitsAndBytes::q;
	
	
	// E.g. "19840124.183000"
	static std::string DateFormattedForFilename( unsigned long clock )
	{
		DateTimeRec date = N::SecondsToDate( clock );
		
		std::ostringstream stamp;
		
		stamp << EncodeDecimal4( date.year   )
		      << EncodeDecimal2( date.month  )
		      << EncodeDecimal2( date.day    )
		      << "."
		      << EncodeDecimal2( date.hour   )
		      << EncodeDecimal2( date.minute )
		      << EncodeDecimal2( date.second );
		
		return stamp.str();
	}
	
	static std::string MakeMessageName()
	{
		static unsigned long stamp = N::GetDateTime();
		static int serial = 0;
		
		unsigned long now = N::GetDateTime();
		
		if ( stamp == now )
		{
			++serial;
		}
		else
		{
			stamp = now;
			serial = 1;
		}
		
		return DateFormattedForFilename( now ) + "-" + EncodeDecimal2( serial );
	}
	
	inline unsigned int IP( unsigned char a,
	                        unsigned char b,
	                        unsigned char c,
	                        unsigned char d )
	{
		return (a << 24) | (b << 16) | (c << 8) | d;
	}
	
	
	static std::string GetForwardPath( const std::string& rcptLine )
	{
		return rcptLine.substr( std::strlen( "RCPT TO:" ), rcptLine.npos );
	}
	
	static std::string GetReversePath( const std::string& fromLine )
	{
		return fromLine.substr( std::strlen( "MAIL FROM:" ), fromLine.npos );
	}
	
	static void CreateOneLiner( const FSSpec& file, const std::string& line )
	{
		typedef NN::StringFlattener< std::string > Flattener;
		
		std::string output = line + "\n";
		
		io::spew_file< Flattener >( N::FSpCreate( file,
		                                          N::OSType( 'R*ch' ),
		                                          N::OSType( 'TEXT' ) ),
		                            output );
	}
	
	static void CreateDestinationFile( const N::FSDirSpec& destFolder, const std::string& dest )
	{
		CreateOneLiner( destFolder / dest.substr( 0, 31 ),
		                dest );
	}
	
	static N::FSDirSpec QueueDirectory()
	{
		return NN::Convert< N::FSDirSpec >( io::system_root< N::FSDirSpec >() / "j" / "var" / "spool" / "jmail" / "queue" );
	}
	
	
	class PartialMessage
	{
		private:
			NN::Owned< N::FSDirSpec, N::RecursiveFSDeleter > dir;
			NN::Owned< N::FSFileRefNum > out;
			unsigned int bytes;
		
		public:
			PartialMessage()  {}
			PartialMessage( const FSSpec& dir );
			
			N::FSDirSpec Dir() const  { return dir; }
			unsigned int Bytes() const  { return bytes; }
			void WriteLine( const std::string& line );
			
			void Finished();
	};
	
	PartialMessage::PartialMessage( const FSSpec& dirLoc )
	:
		dir( NN::Owned< N::FSDirSpec, N::RecursiveFSDeleter >::Seize( N::FSpDirCreate( dirLoc ) ) ), 
		out( io::open_for_writing( N::FSpCreate( dir.Get() / "Message",
		                                         N::OSType( 'R*ch' ),
		                                         N::OSType( 'TEXT' ) ) ) )
	{
		//
	}
	
	void PartialMessage::WriteLine( const std::string& line )
	{
		//static unsigned int lastFlushKBytes = 0;
		std::string terminatedLine = line + "\r\n";
		
		io::write( out, terminatedLine.data(), terminatedLine.size() );
		
		bytes += terminatedLine.size();
		
		/*
		unsigned int kBytes = bytes / 1024;
		
		if ( kBytes - lastFlushKBytes >= 4 )
		{
			Io::Err << ".";
			//IO::Flush(out);
			lastFlushKBytes = kBytes;
		}
		*/
	}
	
	void PartialMessage::Finished()
	{
		dir.Release();
	}
	
	
	std::string myHello;
	std::string myFrom;
	std::list< std::string > myTo;
	PartialMessage myMessage;
	bool dataMode = false;
	
	
	static void QueueMessage()
	{
		N::FSDirSpec dir = myMessage.Dir();
		
		// Create the Destinations subdirectory.
		N::FSDirSpec destFolder = N::FSpDirCreate( dir / "Destinations" );
		
		// Create the destination files.
		std::for_each( myTo.begin(),
		               myTo.end(),
		               more::compose1( std::bind1st( more::ptr_fun( CreateDestinationFile ),
		                                             destFolder ),
		                               more::ptr_fun( GetForwardPath ) ) );
		
		// Create the Return-Path file.
		// Write this last so the sender won't delete the message prematurely.
		CreateOneLiner( dir / "Return-Path", 
		                GetReversePath( myFrom ) );
		
	}
	
	static void DoCommand( const std::string& command )
	{
		std::string word = command.substr( 0, command.find(' ') );
		
		if ( false )
		{
			//
		}
		else if ( word == "MAIL" )
		{
			myFrom = command;
			
			p7::write( p7::stdout_fileno, STR_LEN( "250 Sender ok, probably"  "\r\n" ) );
		}
		else if ( word == "RCPT" )
		{
			myTo.push_back( command );
			
			p7::write( p7::stdout_fileno, STR_LEN( "250 Recipient ok, I guess"  "\r\n" ) );
		}
		else if ( word == "DATA" )
		{
			PartialMessage msg( QueueDirectory() / MakeMessageName() );
			
			myMessage = msg;
			dataMode  = true;
			
			p7::write( p7::stdout_fileno, STR_LEN( "354 I'm listening"  "\r\n" ) );
		}
		else if ( word == "HELO" )
		{
			myHello = command;
			
			p7::write( p7::stdout_fileno, STR_LEN( "250 Hello there"  "\r\n" ) );
		}
		else if ( word == "RSET" )
		{
			myFrom = "";
			myTo.clear();
			//myData.clear();
			
			p7::write( p7::stdout_fileno, STR_LEN( "250 Reset"  "\r\n" ) );
		}
		else if ( word == "QUIT" )
		{
			//isComplete = true;
			p7::write( p7::stdout_fileno, STR_LEN( "221 Closing connection"  "\r\n" ) );
			
			throw p7::exit_success;
		}
		else
		{
			if ( word != "EHLO" )
			{
				std::fprintf( stderr, "Unrecognized command '%s'\n", word.c_str() );
			}
			
			p7::write( p7::stdout_fileno, STR_LEN( "500 Unrecognized command"  "\r\n" ) );
		}
	}
	
	static void DoData( const std::string& data )
	{
		myMessage.WriteLine( data );
		
		if ( data == "." )
		{
			p7::write( p7::stderr_fileno, STR_LEN( "done\n" ) );
			
			dataMode = false;
			
			bool queued = false;
			
			try
			{
				QueueMessage();
				myMessage.Finished();
				queued = true;
			}
			catch ( ... )
			{
				
			}
			
			const char* message = queued ? "250 Message accepted"      "\r\n"
			                             : "554 Can't accept message"  "\r\n";
			
			p7::write( p7::stdout_fileno, message, std::strlen( message ) );
		}
		else
		{
			//
		}
	}
	
	static void DoLine( const std::string& line )
	{
		if ( dataMode )
		{
			DoData( line );
		}
		else
		{
			DoCommand( line );
		}
	}
	
	int Main( int argc, iota::argv_t argv )
	{
		sockaddr_in peer;
		socklen_t peerlen = sizeof peer;
		
		if ( getpeername( 0, (sockaddr*)&peer, &peerlen ) == 0 )
		{
			std::fprintf( stderr, "Connection from %s, port %d\n",
			                                       inet_ntoa( peer.sin_addr ),
			                                                peer.sin_port );
		}
		
		Io::TextInputAdapter< p7::fd_t > input( p7::stdin_fileno );
		
		const char* hostname = "temporarily.out.of.order";
		
		std::printf( "220 %s ready"  "\r\n", hostname );
		
		while ( input.Ready() )
		{
			DoLine( input.Read() );
		}
		
		return 0;
	}

}

