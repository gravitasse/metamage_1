/*	=========================
 *	FSTree_new_scrollframe.cc
 *	=========================
 */

#include "Genie/FileSystem/FSTree_new_scrollframe.hh"

// Nucleus
#include "Nucleus/Saved.h"

// Pedestal
#include "Pedestal/EmptyView.hh"
#include "Pedestal/Scrollbar.hh"
#include "Pedestal/Scroller_beta.hh"

// Genie
#include "Genie/FileSystem/FSTree_Directory.hh"
#include "Genie/FileSystem/FSTree_Property.hh"
#include "Genie/FileSystem/FSTree_new_scroller.hh"
#include "Genie/FileSystem/FSTree_sys_window_REF.hh"
#include "Genie/FileSystem/ResolvePathname.hh"
#include "Genie/FileSystem/ScrollerBase.hh"
#include "Genie/FileSystem/TrackScrollbar.hh"


namespace Genie
{
	
	namespace N = Nitrogen;
	namespace NN = Nucleus;
	namespace Ped = Pedestal;
	
	
	struct ScrollFrameParameters
	{
		bool  itHasVertical;
		bool  itHasHorizontal;
		
		boost::shared_ptr< Ped::View >  itsSubview;
		
		std::string  itsTargetPath;
		
		ScrollerProxy  itsTargetProxy;
		
		ScrollFrameParameters() : itHasVertical(),
		                          itHasHorizontal(),
		                          itsSubview( Ped::EmptyView::Get() )
		{
		}
	};
	
	typedef std::map< const FSTree*, ScrollFrameParameters > ScrollFrameParametersMap;
	
	static ScrollFrameParametersMap gScrollFrameParametersMap;
	
	
	
	class Scrollbar : public Ped::Scrollbar
	{
		private:
			Scrollbar_UserData itsUserData;
		
		public:
			void Update( const Rect& bounds, const FSTree* viewKey );
	};
	
	static Ped::ScrollerAPI* RecoverScrollerFromControl( ControlRef control )
	{
		Scrollbar_UserData* userData = N::GetControlReference( control );
		
		ASSERT( userData      != NULL );
		ASSERT( userData->key != NULL );
		
		const FSTree* scrollFrame = userData->key;
		
		ScrollFrameParametersMap::iterator it = gScrollFrameParametersMap.find( scrollFrame );
		
		if ( it != gScrollFrameParametersMap.end() )
		{
			Ped::ScrollerAPI* proxy = &it->second.itsTargetProxy;
			
			return proxy;
		}
		
		return NULL;
	}
	
	static void DebriefAfterTrack( ControlRef control )
	{
		Scrollbar_UserData* userData = N::GetControlReference( control );
		
		ASSERT( userData      != NULL );
		ASSERT( userData->key != NULL );
		
		const bool vertical = userData->vertical;
		
		const FSTree* scrollFrame = userData->key;
		
		ScrollFrameParametersMap::iterator it = gScrollFrameParametersMap.find( scrollFrame );
		
		if ( it != gScrollFrameParametersMap.end() )
		{
			if ( Ped::ScrollerAPI* proxy = &it->second.itsTargetProxy )
			{
				int clientLength = vertical ? proxy->ClientHeight() : proxy->ClientWidth();
				int viewLength   = vertical ? proxy->ViewHeight  () : proxy->ViewWidth  ();
				int offset       = vertical ? proxy->GetVOffset  () : proxy->GetHOffset ();
				
				short max_offset = std::max( clientLength - viewLength, offset );
				
				N::SetControlMaximum( control, max_offset );
			}
		}
	}
	
	void Scrollbar::Update( const Rect& bounds, const FSTree* viewKey )
	{
		if ( Get() == NULL )
		{
			itsUserData.trackHook = &Ped::TrackScrollbar;
			itsUserData.fetchHook = &RecoverScrollerFromControl;
			itsUserData.afterHook = &DebriefAfterTrack;
			itsUserData.vertical  = bounds.bottom - bounds.top > bounds.right - bounds.left;
			itsUserData.key       = viewKey;
			
			Create( bounds, &itsUserData );
		}
		else
		{
			UpdateBounds( bounds );
		}
	}
	
	
	class ScrollFrame : public Ped::ScrollFrame
	{
		private:
			typedef const FSTree* Key;
			
			Key itsKey;
			
			Scrollbar  itsVertical;
			Scrollbar  itsHorizontal;
			
			Rect itsLastBounds;
		
		public:
			ScrollFrame( Key key ) : itsKey( key )
			{
			}
			
			const Rect& Bounds() const  { return itsLastBounds; }
			
			void SetBounds( const Rect& bounds );
			
			Scrollbar& GetHorizontal()  { return itsHorizontal; }
			Scrollbar& GetVertical  ()  { return itsVertical;   }
			
			void UpdateScrollbars();
			
			void Draw( const Rect& bounds, bool erasing );
			
			Ped::View& Subview();
	};
	
	
	void ScrollFrame::SetBounds( const Rect& bounds )
	{
		itsLastBounds = bounds;
		
		Rect aperture = bounds;
		
		const short kScrollbarThickness = 16;
		
		const short kOverlap = 1;
		
		const short kFootprint = kScrollbarThickness - kOverlap;
		
		const ScrollFrameParameters& params = gScrollFrameParametersMap[ itsKey ];
		
		if ( params.itHasHorizontal )
		{
			aperture.bottom -= kFootprint;
			
			Rect scrollbarBounds = itsLastBounds;
			
			scrollbarBounds.top = aperture.bottom;
			
			scrollbarBounds.left   -= kOverlap;
			scrollbarBounds.bottom += kOverlap;
			scrollbarBounds.right  += kOverlap - kFootprint;
			
			itsHorizontal.Update( scrollbarBounds, itsKey );
		}
		else
		{
			itsHorizontal.Clear();
		}
		
		if ( params.itHasVertical )
		{
			aperture.right -= kFootprint;
			
			Rect scrollbarBounds = itsLastBounds;
			
			scrollbarBounds.left = aperture.right;
			
			scrollbarBounds.top    -= kOverlap;
			scrollbarBounds.right  += kOverlap;
			scrollbarBounds.bottom += kOverlap - kFootprint;
			
			itsVertical.Update( scrollbarBounds, itsKey );
		}
		else
		{
			itsVertical.Clear();
		}
		
		Subview().SetBounds( aperture );
		
		UpdateScrollbars();
	}
	
	void ScrollFrame::UpdateScrollbars()
	{
		const ScrollFrameParameters& params = gScrollFrameParametersMap[ itsKey ];
		
		const FSTree* target = params.itsTargetProxy.Get();
		
		itsHorizontal.Adjust( GetScrollerClientWidth( target ),
		                      GetScrollerHOffset    ( target ),
		                      itsLastBounds.right - itsLastBounds.left );
		
		itsVertical.Adjust( GetScrollerClientHeight( target ),
		                    GetScrollerVOffset     ( target ),
		                    itsLastBounds.bottom - itsLastBounds.top );
	}
	
	
	void ScrollFrame::Draw( const Rect& bounds, bool erasing )
	{
		const ScrollFrameParameters& params = gScrollFrameParametersMap[ itsKey ];
		
		const bool rebind =    params.itHasHorizontal != !!itsHorizontal.Get()
		                    || params.itHasVertical   != !!itsVertical  .Get();
		
		if ( rebind )
		{
			SetBounds( bounds );
		}
		
		Subview().Draw( ApertureFromBounds( bounds ), erasing );
		
		UpdateScrollbars();
	}
	
	Ped::View& ScrollFrame::Subview()
	{
		boost::shared_ptr< Ped::View >& subview = gScrollFrameParametersMap[ itsKey ].itsSubview;
		
		if ( subview.get() == NULL )
		{
			subview = Ped::EmptyView::Get();
		}
		
		return *subview;
	}
	
	boost::shared_ptr< Ped::View > ScrollFrameFactory( const FSTree* delegate )
	{
		return boost::shared_ptr< Ped::View >( new ScrollFrame( delegate ) );
	}
	
	
	void FSTree_new_scrollframe::DestroyDelegate( const FSTree* delegate )
	{
		gScrollFrameParametersMap.erase( delegate );
	}
	
	
	class FSTree_ScrollFrame_target : public FSTree
	{
		public:
			FSTree_ScrollFrame_target( const FSTreePtr&    parent,
			                           const std::string&  name ) : FSTree( parent, name )
			{
			}
			
			bool Exists() const;
			
			void Delete() const;
			
			void SymLink( const std::string& target ) const;
			
			std::string ReadLink() const;
	};
	
	bool FSTree_ScrollFrame_target::Exists() const
	{
		const FSTree* view = GetViewKey( this );
		
		return gScrollFrameParametersMap[ view ].itsTargetProxy.Get() != NULL;
	}
	
	void FSTree_ScrollFrame_target::Delete() const
	{
		const FSTree* view = GetViewKey( this );
		
		ScrollFrameParameters& params = gScrollFrameParametersMap[ view ];
		
		params.itsTargetPath.clear();
		
		params.itsTargetProxy = ScrollerProxy( NULL );
		
		InvalidateWindowForView( view );
	}
	
	void FSTree_ScrollFrame_target::SymLink( const std::string& target_path ) const
	{
		FSTreePtr target = ResolvePathname( target_path, ParentRef() );
		
		FSTreePtr delegate = target->Lookup( "." );
		
		const FSTree* view = GetViewKey( this );
		
		ScrollFrameParameters& params = gScrollFrameParametersMap[ view ];
		
		params.itsTargetPath  = target_path;
		params.itsTargetProxy = ScrollerProxy( delegate.get() );
		
		InvalidateWindowForView( view );
	}
	
	std::string FSTree_ScrollFrame_target::ReadLink() const
	{
		const FSTree* view = GetViewKey( this );
		
		return gScrollFrameParametersMap[ view ].itsTargetPath;
	}
	
	
	namespace
	{
		
		boost::shared_ptr< Ped::View >& GetView( const FSTree* key )
		{
			return gScrollFrameParametersMap[ key ].itsSubview;
		}
		
		bool& Vertical( const FSTree* view )
		{
			return gScrollFrameParametersMap[ view ].itHasVertical;
		}
		
		bool& Horizontal( const FSTree* view )
		{
			return gScrollFrameParametersMap[ view ].itHasHorizontal;
		}
		
	}
	
	
	template < class Property >
	static FSTreePtr Property_Factory( const FSTreePtr&    parent,
	                                   const std::string&  name )
	{
		return FSTreePtr( new FSTree_Property( parent,
		                                       name,
		                                       &Property::Get,
		                                       &Property::Set ) );
	}
	
	const FSTree_Premapped::Mapping ScrollFrame_view_Mappings[] =
	{
		{ "horizontal", &Property_Factory< View_Property< Boolean_Scribe, Horizontal > > },
		{ "vertical",   &Property_Factory< View_Property< Boolean_Scribe, Vertical   > > },
		
		{ "target", &Basic_Factory< FSTree_ScrollFrame_target >, true },
		
		{ "v", &Basic_Factory< FSTree_X_view< GetView > >, true },
		
		{ NULL, NULL }
	};
	
}

