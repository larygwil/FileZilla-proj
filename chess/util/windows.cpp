#include "platform.hpp"

#include <windows.h>

#include <iostream>

namespace tr1 {
}
namespace std {
using namespace tr1;
}


uint64_t timer_precision()
{
	return 1000000;
}


uint64_t get_time() {
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);

	uint64_t t = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) + ft.dwLowDateTime;
	return t / 10; // From 100 nanoseconds to microseconds.
}


void console_init()
{
	//std::cout.setf(std::ios::unitbuf);
}


unsigned int get_cpu_count()
{
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = 0;
	DWORD len = 0;

	while( !GetLogicalProcessorInformation( buffer, &len ) ) {
		if( buffer ) {
			free(buffer);
		}

		if( GetLastError() != ERROR_INSUFFICIENT_BUFFER ) {
			return 1;
		}

		buffer = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(malloc( len ));
	}

	int count = 1;

	if( buffer != 0 ) {
		DWORD offset = 0;
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = buffer;
		while( offset < len ) {
			switch (ptr->Relationship) {
				case RelationProcessorCore:
					// As get_cpu_count is used in early initialization, use generic popcount.
					count += static_cast<int>(generic_popcount( ptr->ProcessorMask ));
					break;
				default:
					break;
			}
			offset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
			++ptr;
		}

		free( buffer );
	}

	return count;
}

// In MiB
int get_system_memory()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(MEMORYSTATUSEX);

	GlobalMemoryStatusEx( &status );

	return static_cast<int>(status.ullTotalPhys / 1024 / 1024);
}


void* page_aligned_malloc( uint64_t size )
{
	uint64_t page_size = get_page_size();

	uint64_t alloc = size;
	if( size % page_size ) {
		alloc += page_size - size % page_size;
	}

	void* p = VirtualAlloc( 0, alloc, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

	if( p ) {
		return p;
	}
	else {
		DWORD err = GetLastError();
		std::cerr << "Memory allocation failed: " << err << std::endl;
		return 0;
	}
}


void aligned_free( void* p )
{
	if( p ) {
		BOOL res = VirtualFree( p, 0, MEM_RELEASE );
		if( !res ) {
			DWORD err = GetLastError();
			std::cerr << "Deallocation failed: " << err << std::endl;
		}
	}
}


uint64_t get_page_size()
{
	SYSTEM_INFO info;
	memset( &info, 0, sizeof(SYSTEM_INFO) );
	GetSystemInfo( &info );
	return info.dwPageSize;
}

bool uses_native_popcnt()
{
#if HAS_NATIVE_POPCOUNT
	return true;
#else
	return false;
#endif
}

bool cpu_has_popcnt()
{
	// eax, ebx, ecx, edx
	int info[4] = {0, 0, 0, 0};
	__cpuid( info, 0x1 );

	// Bit 23 indicates POPCNT support
	return (info[3] & (1 << 23)) != 0;
}

void usleep( uint64_t usecs )
{
	Sleep( static_cast<DWORD>(usecs / 1000) );
}
