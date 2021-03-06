/*
	Genie/FS/sys/mac/user/home.hh
	-----------------------------
*/

#ifndef GENIE_FS_SYS_MAC_USER_HOME_HH
#define GENIE_FS_SYS_MAC_USER_HOME_HH

// plus
#include "plus/string.hh"

// vfs
#include "vfs/node_ptr.hh"


namespace Genie
{
	
	vfs::node_ptr New_FSTree_sys_mac_user_home( const vfs::node*     parent,
	                                            const plus::string&  name,
	                                            const void*          args );
	
}

#endif
