/*
	posixfs.hh
	----------
*/

#ifndef POSIXFS_HH
#define POSIXFS_HH

// vlib
#include "vlib/proc_info.hh"


namespace vlib
{
	
	extern const proc_info proc_dirname;
	extern const proc_info proc_fstat;
	extern const proc_info proc_listdir;
	extern const proc_info proc_load;
	extern const proc_info proc_lstat;
	extern const proc_info proc_stat;
	
}

#endif
