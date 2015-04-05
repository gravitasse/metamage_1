/*
	bridge.cc
	---------
*/

// Standard C
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

// POSIX
#include <unistd.h>
#include <sys/time.h>
#include <sys/uio.h>

// v68k
#include "v68k/endian.hh"

// v68k-auth
#include "auth/auth.hh"

// v68k-syscalls
#include "syscall/bridge.hh"


#pragma exceptions off


using v68k::auth::fully_authorized;


uint32_t errno_ptr_addr;

int32_t fake_pid = 0;  // fake PID for getpid(), unless 0 or -1


static int32_t emulated_pid()
{
	return + fake_pid > 0 ? fake_pid
	       : fake_pid < 0 ? getpid()
	       :                12345;
}

static bool get_stacked_args( const v68k::processor_state& s, uint32_t* out, int n )
{
	uint32_t sp = s.a(7);
	
	while ( n > 0 )
	{
		sp += 4;
		
		if ( !s.mem.get_long( sp, *out, s.data_space() ) )
		{
			return false;
		}
		
		++out;
		--n;
	}
	
	return true;
}

static void set_errno( v68k::processor_state& s )
{
	s.d(1) = errno;
	
	uint32_t errno_ptr;
	
	if ( s.mem.get_long( errno_ptr_addr, errno_ptr, s.data_space() )  &&  errno_ptr != 0 )
	{
		s.mem.put_long( errno_ptr, errno, s.data_space() );
	}
}

static inline bool set_result( v68k::processor_state& s, int result )
{
	s.d(0) = result;
	
	if ( result < 0 )
	{
		set_errno( s );
	}
	
	return true;
}

static bool emu_exit( v68k::processor_state& s )
{
	uint32_t args[1];  // status
	
	if ( !get_stacked_args( s, args, 1 ) )
	{
		return s.bus_error();
	}
	
	const int status = int32_t( args[0] );
	
	exit( status );
	
	// Not reached
	return false;
}

static bool emu_read( v68k::processor_state& s )
{
	uint32_t args[3];  // fd, buffer, length
	
	if ( !get_stacked_args( s, args, 3 ) )
	{
		return s.bus_error();
	}
	
	const int fd = int32_t( args[0] );
	
	const uint32_t buffer = args[1];
	
	const size_t length = args[2];
	
	uint8_t* p = s.mem.translate( buffer, length, s.data_space(), v68k::mem_write );
	
	int result;
	
	if ( p == NULL )
	{
		result = -1;
		
		errno = EFAULT;
	}
	else
	{
		result = read( fd, p, length );
	}
	
	return set_result( s, result );
}

static bool emu_write( v68k::processor_state& s )
{
	uint32_t args[3];  // fd, buffer, length
	
	if ( !get_stacked_args( s, args, 3 ) )
	{
		return s.bus_error();
	}
	
	const int fd = int32_t( args[0] );
	
	const uint32_t buffer = args[1];
	
	const size_t length = args[2];
	
	const uint8_t* p = s.mem.translate( buffer, length, s.data_space(), v68k::mem_read );
	
	int result;
	
	if ( p == NULL )
	{
		result = -1;
		
		errno = EFAULT;
	}
	else
	{
		result = write( fd, p, length );
	}
	
	return set_result( s, result );
}

static bool emu_getpid( v68k::processor_state& s )
{
	int32_t result = emulated_pid();
	
	return set_result( s, result );
}

static bool emu_pause( v68k::processor_state& s )
{
	int result = pause();
	
	return set_result( s, result );
}

static bool emu_kill( v68k::processor_state& s )
{
	uint32_t args[2];  // pid, sig
	
	if ( !get_stacked_args( s, args, 2 ) )
	{
		return s.bus_error();
	}
	
	pid_t     pid = int32_t( args[0] );
	const int sig = int32_t( args[1] );
	
	int result;
	
	if ( fully_authorized  ||  pid == emulated_pid() )
	{
		if ( fake_pid > 0  &&  pid == fake_pid )
		{
			pid = getpid();
		}
		
		result = kill( pid, sig );
	}
	else
	{
		result = -1;
		
		errno = EPERM;
	}
	
	return set_result( s, result );
}

static bool emu_gettimeofday( v68k::processor_state& s )
{
	uint32_t args[2];  // tv, tz
	
	if ( !get_stacked_args( s, args, 2 ) )
	{
		return s.bus_error();
	}
	
	timeval tv;
	
	int result = gettimeofday( &tv, NULL );
	
	const uint32_t tv_addr = args[0];
	
	if ( result == 0 )
	{
		const bool ok = s.mem.put_long( tv_addr,     tv.tv_sec,  s.data_space() )
		              & s.mem.put_long( tv_addr + 4, tv.tv_usec, s.data_space() );
		
		if ( !ok )
		{
			errno = EFAULT;
			
			result = -1;
		}
	}
	
	return set_result( s, result );
}

static void load_fdset( fd_set& native, const uint32_t* emulated, int n )
{
	FD_ZERO( &native );
	
	while ( --n >= 0 )
	{
		const uint32_t unit = iota::u32_from_big( emulated[ n >> 5 ] );
		
		if ( unit & (1 << (n & 31)) )
		{
			FD_SET( n, &native );
		}
	}
}

static int load_fdset( v68k::processor_state&  s,
                       int                     n_fds,
                       int                     n_bytes,
                       uint32_t                addr,
                       fd_set&                 native )
{
	if ( addr == 0 )
	{
		return 0;
	}
	
	const uint32_t* fdset_mem = (uint32_t*) s.mem.translate( addr,
	                                                         n_bytes,
	                                                         s.data_space(),
	                                                         v68k::mem_read );
	
	if ( fdset_mem == NULL  ||  (uintptr_t) fdset_mem & 0x3 )
	{
		errno = EFAULT;
		
		return -1;
	}
	
	load_fdset( native, fdset_mem, n_fds );
	
	return 1;
}

static void store_fdset( uint32_t* emulated, const fd_set& native, int n )
{
	while ( --n >= 0 )
	{
		if ( FD_ISSET( n, &native ) )
		{
			emulated[ n >> 5 ] |= iota::big_u32( 1 << (n & 31) );
		}
	}
}

static int store_fdset( v68k::processor_state&  s,
                        int                     n_fds,
                        int                     n_bytes,
                        uint32_t                addr,
                        fd_set*                 native )
{
	if ( native == NULL )
	{
		return 0;
	}
	
	uint32_t* fdset_mem = (uint32_t*) s.mem.translate( addr,
	                                                   n_bytes,
	                                                   s.data_space(),
	                                                   v68k::mem_write );
	
	if ( fdset_mem == NULL  ||  (uintptr_t) fdset_mem & 0x3 )
	{
		errno = EFAULT;
		
		return -1;
	}
	
	store_fdset( fdset_mem, *native, n_fds );
	
	s.mem.translate( addr, n_bytes, s.data_space(), v68k::mem_update );
	
	return 1;
}

static bool emu_select( v68k::processor_state& s )
{
	fd_set read_fds;
	fd_set write_fds;
	fd_set except_fds;
	
	fd_set* r = NULL;
	fd_set* w = NULL;
	fd_set* x = NULL;
	
	timeval* t = NULL;
	
	uint32_t args[ 5 ];  // n, r, w, x, t
	
	if ( !get_stacked_args( s, args, 5 ) )
	{
		return s.bus_error();
	}
	
	const int n = args[ 0 ];
	
	const int n_fdset_bytes = n + 31 >> 5;
	
	const uint32_t rfd_addr = args[ 1 ];
	const uint32_t wfd_addr = args[ 2 ];
	const uint32_t xfd_addr = args[ 3 ];
	
	int result;
	
	if ( n > 0 )
	{
		result = load_fdset( s, n, n_fdset_bytes, rfd_addr, read_fds );
		
		if ( result < 0 )  return set_result( s, result );
		if ( result > 0 )  r = &read_fds;
		
		result = load_fdset( s, n, n_fdset_bytes, wfd_addr, write_fds );
		
		if ( result < 0 )  return set_result( s, result );
		if ( result > 0 )  w = &write_fds;
		
		result = load_fdset( s, n, n_fdset_bytes, xfd_addr, except_fds );
		
		if ( result < 0 )  return set_result( s, result );
		if ( result > 0 )  x = &except_fds;
	}
	
	if ( const uint32_t timeout_addr = args[ 4 ] )
	{
		uint32_t seconds;
		uint32_t useconds;
		
		const bool ok = s.mem.get_long( timeout_addr,     seconds,  s.data_space() )
		              & s.mem.get_long( timeout_addr + 4, useconds, s.data_space() );
		
		if ( !ok )
		{
			errno = EFAULT;
			
			return set_result( s, -1 );
		}
		
		timeval tv;
		
		tv.tv_sec  = seconds;
		tv.tv_usec = useconds;
		
		t = &tv;
	}
	
	result = select( n, r, w, x, t );
	
	if ( n > 0 )
	{
		store_fdset( s, n, n_fdset_bytes, rfd_addr, r );
		store_fdset( s, n, n_fdset_bytes, wfd_addr, w );
		store_fdset( s, n, n_fdset_bytes, xfd_addr, x );
	}
	
	return set_result( s, result );
}

struct iovec_68k
{
	uint32_t ptr;
	uint32_t len;
};

static bool emu_writev( v68k::processor_state& s )
{
	uint32_t args[3];  // fd, iov, n
	
	if ( !get_stacked_args( s, args, 3 ) )
	{
		return s.bus_error();
	}
	
	int result = -1;
	
	const int fd = int32_t( args[0] );
	
	const uint32_t iov_addr = args[1];
	
	const size_t n = args[2];
	
	const size_t size = n * sizeof (iovec_68k);
	
	struct iovec* iov = (struct iovec*) malloc( sizeof (struct iovec) * n );
	
	const iovec_68k* iov_mem = (iovec_68k*) s.mem.translate( iov_addr,
	                                                         size,
	                                                         s.data_space(),
	                                                         v68k::mem_read );
	
	if ( iov == NULL )
	{
		errno = ENOMEM;
		
		goto end;
	}
	
	if ( iov_mem == NULL )
	{
		errno = EFAULT;
		
		goto end;
	}
	
	for ( int i = 0;  i < n;  ++i )
	{
		const uint32_t ptr = v68k::longword_from_big( iov_mem[i].ptr );
		const uint32_t len = v68k::longword_from_big( iov_mem[i].len );
		
		const uint8_t* p = s.mem.translate( ptr, len, s.data_space(), v68k::mem_read );
		
		if ( p == NULL )
		{
			errno = EFAULT;
			
			goto end;
		}
		
		iov[i].iov_base = (void*) p;
		iov[i].iov_len  = len;
	}
	
	result = writev( fd, iov, n );
	
end:
	
	free( iov );
	
	return set_result( s, result );
}

static bool emu_nanosleep( v68k::processor_state& s )
{
	uint32_t args[2];  // requested, remaining
	
	if ( !get_stacked_args( s, args, 2 ) )
	{
		return s.bus_error();
	}
	
	uint32_t requested = args[0];
	
	uint32_t requested_seconds;
	uint32_t requested_nanoseconds;
	
	const bool ok = s.mem.get_long( requested,     requested_seconds,     s.data_space() )
	              & s.mem.get_long( requested + 4, requested_nanoseconds, s.data_space() );
	
	if ( !ok )
	{
		errno = EFAULT;
		
		return set_result( s, -1 );
	}
	
	if ( requested_nanoseconds >= 1000 * 1000 * 1000 )
	{
		errno = EINVAL;
		
		return set_result( s, -1 );
	}
	
	const timespec request_ts = { requested_seconds, requested_nanoseconds };
	
	timespec remain_ts;
	
	int result = nanosleep( &request_ts, &remain_ts );
	
	const uint32_t remaining = args[1];
	
	if ( result < 0  &&  remaining != 0 )
	{
		const bool ok = s.mem.put_long( remaining,     remain_ts.tv_sec,  s.data_space() )
		              & s.mem.put_long( remaining + 4, remain_ts.tv_nsec, s.data_space() );
		
		if ( !ok )
		{
			errno = EFAULT;
			
			result = -1;
		}
	}
	
	return set_result( s, result );
}

bool bridge_call( v68k::processor_state& s )
{
	const uint16_t call_number = s.d(0);
	
	switch ( call_number )
	{
		case 1:  return emu_exit ( s );
		case 3:  return emu_read ( s );
		case 4:  return emu_write( s );
		
		case 20:  return emu_getpid( s );
		case 29:  return emu_pause ( s );
		case 37:  return emu_kill  ( s );
		case 78:  return emu_gettimeofday( s );
		case 82:  return emu_select( s );
		
		case 146:  return emu_writev( s );
		case 162:  return emu_nanosleep( s );
		
		default:
			return false;
	}
}
