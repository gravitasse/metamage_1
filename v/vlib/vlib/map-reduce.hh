/*
	map-reduce.hh
	-------------
*/

#ifndef VLIB_MAPREDUCE_HH
#define VLIB_MAPREDUCE_HH


namespace vlib
{
	
	class Value;
	
	Value map   ( const Value& container, const Value& f );
	Value reduce( const Value& container, const Value& f );
	
}

#endif
