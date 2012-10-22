#include <unistd.h>
#include <stdio.h>

int main()
{
  printf("#define PAGESIZE %d\n", getpagesize());
  return 0;
}
