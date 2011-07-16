#ifndef __HASH_H__
#define __HASH_H__

/*
 * Fast, fixed-size hash table.
 * There is no collision handling, new values simply overwrite
 * old ones.
 */


typedef unsigned long long hash_key;
// max_size is in megabytes
void init_hash( unsigned int max_size, unsigned int itemSize );

bool lookup( hash_key key, unsigned char *const data );

void store( hash_key key, unsigned char const* const data );

void free_hash();

void clearData();

unsigned long long max_hash_entry_count();

#endif //__HASH_H__
