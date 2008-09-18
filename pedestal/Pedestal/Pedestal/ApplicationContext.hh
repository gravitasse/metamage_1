/*	=====================
 *	ApplicationContext.hh
 *	=====================
 */

#ifndef PEDESTAL_APPLICATIONCONTEXT_HH
#define PEDESTAL_APPLICATIONCONTEXT_HH

// Nitrogen
#include "Nitrogen/Resources.h"

#if PRAGMA_ONCE
#pragma once
#endif


namespace Pedestal
{
	
	namespace N = Nitrogen;
	
	struct MacToolboxInit
	{
		MacToolboxInit();
	};
	
	struct MemoryInit
	{
		MemoryInit( std::size_t moreMasters );
	};
	
	class ApplicationContext
	{
		private:
			MacToolboxInit    itsMacToolboxInit;
			MemoryInit        itsMemoryInit;
			N::ResFileRefNum  itsResFileRefNum;
			VersRec           itsVersion;
		
		public:
			ApplicationContext();
			
			N::ResFileRefNum    ResFileAccessPath() const  { return itsResFileRefNum; }
			const VersRec&      Version          () const  { return itsVersion;       }
	};
	
}

#endif

