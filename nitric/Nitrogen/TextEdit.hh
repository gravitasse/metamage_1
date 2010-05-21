// Nitrogen/TextEdit.hh
// --------------------
//
// Maintained by Joshua Juran

// Part of the Nitrogen project.
//
// Written 2004-2006 by Joshua Juran.
//
// This code was written entirely by the above contributor, who places it
// in the public domain.


#ifndef NITROGEN_TEXTEDIT_HH
#define NITROGEN_TEXTEDIT_HH

// Standard C/C++
#include <cstring>

// Mac OS
#ifndef __TEXTEDIT__
#include <TextEdit.h>
#endif

// iota
#include "iota/distance.hh"
#include "iota/string_traits.hh"

// Debug
#include "debug/assert.hh"

// nucleus
#include "nucleus/enumeration_traits.hh"
#include "nucleus/owned.hh"

// Nitrogen
#ifndef NITROGEN_OSSTATUS_HH
#include "Nitrogen/OSStatus.hh"
#endif


namespace Nitrogen
{
	
	enum Justification
	{
		teJustLeft     = ::teJustLeft,
		teJustCenter   = ::teJustCenter,
		teJustRight    = ::teJustRight,
		teForceLeft    = ::teForceLeft,
		
		teFlushDefault = ::teFlushDefault,
		teCenter       = ::teCenter,
		teFlushRight   = ::teFlushRight,
		teFlushLeft    = ::teFlushLeft,
		
		kJustification_Max = nucleus::enumeration_traits< short >::max
	};
	
	using ::TEHandle;
	using ::CharsHandle;  // identical to ::Handle
	
}

namespace nucleus
{
	template <>
	struct disposer< TEHandle >
	{
		typedef TEHandle  argument_type;
		typedef void      result_type;
		
		void operator()( TEHandle teH ) const
		{
			::TEDispose( teH );
		}
	};
}

namespace Nitrogen
{
	
	// TEScrapHandle
	// TEGetScrapLength
	
	nucleus::owned< TEHandle > TENew( const Rect& destRect, const Rect& viewRect );
	
	inline nucleus::owned< TEHandle > TENew( const Rect& rect )
	{
		return TENew( rect, rect );
	}
	
	inline void TEDispose( nucleus::owned< TEHandle > )  {}
	
	inline void TESetText( const void* text, std::size_t length, TEHandle hTE )
	{
		::TESetText( text, length, hTE );
	}
	
	template < class String >
	inline void TESetText( const String& text, TEHandle hTE )
	{
		TESetText( iota::get_string_data( text ),
		           iota::get_string_size( text ),
		           hTE );
	}
	
	using ::TEGetText;
	
	inline void TEGetText( void* text, std::size_t length, TEHandle hTE )
	{
		::Handle h = hTE[0]->hText;
		
		std::memcpy( text, *h, length );
	}
	
	using ::TEIdle;
	
	inline void TESetSelect( std::size_t  selStart,
	                         std::size_t  selEnd,
	                         TEHandle     hTE )
	{
		::TESetSelect( selStart, selEnd, hTE );
	}
	
	using ::TEActivate;
	using ::TEDeactivate;
	
	inline void TEKey( char key, TEHandle hTE )
	{
		::TEKey( key, hTE );
	}
	
	using ::TECut;
	using ::TECopy;
	using ::TEPaste;
	using ::TEDelete;
	
	inline void TEInsert( const void* text, std::size_t length, TEHandle hTE )
	{
		::TEInsert( text, length, hTE );
	}
	
	template < class String >
	inline void TEInsert( const String& text, TEHandle hTE )
	{
		TEInsert( iota::get_string_data( text ),
		          iota::get_string_size( text ),
		          hTE );
	}
	
	inline void TESetAlignment( Justification just, TEHandle hTE )
	{
		::TESetAlignment( just, hTE );
	}
	
	inline void TEUpdate( const Rect& rUpdate, TEHandle hTE )
	{
		::TEUpdate( &rUpdate, hTE );
	}
	
	inline void TETextBox( const void*    text,
	                       std::size_t    length,
	                       const Rect&    box,
	                       Justification  just = teFlushLeft )
	{
		::TETextBox( text, length, &box, just );
	}
	
	template < class String >
	inline void TETextBox( const String&  text,
	                       const Rect&    box,
	                       Justification  just = teFlushLeft )
	{
		TETextBox( iota::get_string_data( text ),
		           iota::get_string_size( text ),
		           box,
		           just );
	}
	
	using ::TEScroll;
	using ::TESelView;
	using ::TEPinScroll;
	using ::TEAutoView;
	using ::TECalText;
	using ::TEGetOffset;
	using ::TEGetPoint;
	
	// 1650
	inline void TEClick( Point pt, bool extendFlag, TEHandle hTE )
	{
		::TEClick( pt, extendFlag, hTE );
	}
	
	// TEStyleNew
	// TESetStyleHandle
	// TEGetStyleHandle
	// TEGetStyle
	// TEStylePaste
	// TESetStyle
	// TEReplaceStyle
	// TEGetStyleScrapHandle
	// TEStyleInsert
	// TEGetHeight
	// TEContinuousStyle
	// TEUseStyleScrap
	// TECustomHook
	// TENumStyles
	// TEFeatureFlag
	// TEGetHiliteRgn
	// TESetScrapLength
	
	// 1913
	inline void TEFromScrap()
	{
		ThrowOSStatus( ::TEFromScrap() );
	}
	
	// 1925
	inline void TEToScrap()
	{
		ThrowOSStatus( ::TEToScrap() );
	}
	
	// TESetClickLoop
	// TEGetDoTextHook
	// TESetDoTextHook
	// TEGetRecalcHook
	// TESetRecalcHook
	// TEGetFindWordHook
	// TESetFindWordHook
	// TEGetScrapHandle
	// TESetScrapHandle
	// LMGetWordRedraw
	// LMSetWordRedraw
	
	class TESetText_Putter
	{
		private:
			TEHandle itsTE;
		
		public:
			TESetText_Putter( TEHandle hTE ) : itsTE( hTE )
			{
			}
			
			void operator()( const void *begin, const void *end ) const
			{
				TESetText( begin, iota::distance( begin, end ), itsTE );
			}
	};
	
	class TEGetText_Getter
	{
		private:
			TEHandle itsTE;
		
		public:
			TEGetText_Getter( TEHandle hTE ) : itsTE( hTE )
			{
			}
			
			std::size_t size() const
			{
				std::size_t length = itsTE[0]->teLength;
				
				ASSERT( GetHandleSize( itsTE[0]->hText ) >= length );
				
				return length;
			}
			
			void operator()( void *begin, void *end ) const
			{
				TEGetText( begin, iota::distance( begin, end ), itsTE );
			}
	};
	
	class TEInsert_Putter
	{
		private:
			TEHandle itsTE;
		
		public:
			TEInsert_Putter( TEHandle hTE ) : itsTE( hTE )
			{
			}
			
			void operator()( const void *begin, const void *end ) const
			{
				TEInsert( begin, iota::distance( begin, end ), itsTE );
			}
	};
	
}

#endif

