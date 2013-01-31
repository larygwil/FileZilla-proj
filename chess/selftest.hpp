#ifndef __SELFTEST_H__
#define __SELFTEST_H__

template<bool split_movegen>
void perft( std::size_t max_depth = 999999 );

bool selftest();

#endif
