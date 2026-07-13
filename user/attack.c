#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  char *mem;
  char *hint = "This may help.";
  int hintlen = strlen(hint);
  int nbytes = 64 * PGSIZE;

  mem = sbrk(nbytes);
  if(mem == SBRK_ERROR)
    exit(1);

  for(int i = 0; i <= nbytes - hintlen - 16; i++) {
    if(memcmp(mem + i, hint, hintlen) == 0) {
      printf("%s\n", mem + i + 16);
      exit(0);
    }
  }

  exit(1);
}
