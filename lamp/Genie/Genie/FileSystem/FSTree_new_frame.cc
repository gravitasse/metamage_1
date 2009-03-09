/*	===================
 *	FSTree_new_frame.cc
 *	===================
 */

#include "Genie/FileSystem/FSTree_new_frame.hh"

// POSIX
#include <fcntl.h>

// Pedestal
#include "Pedestal/EmptyView.hh"
#include "Pedestal/Frame.hh"

// Genie
#include "Genie/FileSystem/FSTree_Directory.hh"
#include "Genie/FileSystem/FSTree_Property.hh"
#include "Genie/FileSystem/FSTree_sys_window_REF.hh"


namespace Genie
{
	
	namespace N = Nitrogen;
	namespace NN = Nucleus;
	namespace p7 = poseven;
	namespace Ped = Pedestal;
	
	
	struct FrameParameters
	{
		short  itsMargin;
		bool   itsMarginChanged;
		
		boost::shared_ptr< Ped::View >  itsSubview;
		
		FrameParameters() : itsMargin( 0 ),
		                    itsMarginChanged(),
		                    itsSubview( Ped::EmptyView::Get() )
		{
		}
	};
	
	typedef std::map< const FSTree*, FrameParameters > FrameParametersMap;
	
	static FrameParametersMap gFrameParametersMap;
	
	
	class Frame : public Ped::Frame
	{
		private:
			typedef const FSTree* Key;
			
			Key itsKey;
		
		public:
			typedef Key Initializer;
			
			Frame( Key key ) : itsKey( key )
			{
			}
			
			void Draw( const Rect& bounds, bool erasing );
			
			short Margin() const;
			
			Ped::View& Subview();
	};
	
	void Frame::Draw( const Rect& bounds, bool erasing )
	{
		FrameParameters& params = gFrameParametersMap[ itsKey ];
		
		if ( params.itsMarginChanged )
		{
			params.itsMarginChanged = false;
			
			SetBounds( bounds );
		}
		
		Ped::Frame::Draw( bounds, erasing );
	}
	
	short Frame::Margin() const
	{
		FrameParametersMap::const_iterator it = gFrameParametersMap.find( itsKey );
		
		if ( it != gFrameParametersMap.end() )
		{
			return it->second.itsMargin;
		}
		
		return 0;
	}
	
	Ped::View& Frame::Subview()
	{
		boost::shared_ptr< Ped::View >& subview = gFrameParametersMap[ itsKey ].itsSubview;
		
		if ( subview.get() == NULL )
		{
			subview = Ped::EmptyView::Get();
		}
		
		return *subview;
	}
	
	boost::shared_ptr< Ped::View > FrameFactory( const FSTree* delegate )
	{
		return boost::shared_ptr< Ped::View >( new Frame( delegate ) );
	}
	
	
	void FSTree_new_frame::DestroyDelegate( const FSTree* delegate )
	{
		gFrameParametersMap.erase( delegate );
	}
	
	
	static std::string ReadMargin( const FSTree* that, bool binary )
	{
		const FSTree* view = GetViewKey( that );
		
		return NN::Convert< std::string >( gFrameParametersMap[ view ].itsMargin );
	}
	
	static void WriteMargin( const FSTree* that, const char* begin, const char* end, bool binary )
	{
		const FSTree* view = GetViewKey( that );
		
		// *end == '\n'
		
		FrameParameters& params = gFrameParametersMap[ view ];
		
		params.itsMargin = std::atoi( begin );
		
		params.itsMarginChanged = true;
		
		InvalidateWindowForView( view );
	}
	
	
	namespace
	{
		
		boost::shared_ptr< Ped::View >& GetView( const FSTree* key )
		{
			return gFrameParametersMap[ key ].itsSubview;
		}
		
	}
	
	
	static FSTreePtr MarginFactory( const FSTreePtr&    parent,
	                                const std::string&  name )
	{
		return FSTreePtr( new FSTree_Property( parent,
		                                       name,
		                                       &ReadMargin,
		                                       &WriteMargin ) );
	}
	
	const FSTree_Premapped::Mapping Frame_view_Mappings[] =
	{
		{ "margin", &MarginFactory },
		
		{ "v", &Basic_Factory< FSTree_X_view< GetView > >, true },
		
		{ NULL, NULL }
	};
	
}

