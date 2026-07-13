#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void
sixfive(int fd)
{
  char c;
  char number[32];
  int index = 0;
  int valid = 1;

  while(read(fd, &c, 1) == 1){

    if(c >= '0' && c <= '9'){
      if(index < sizeof(number) - 1){
        number[index++] = c;
      }
    }
    else if(strchr("-\r\t\n./,", c)){
      if(index > 0 && valid){
        number[index] = '\0';

        int value = atoi(number);

        if(value % 5 == 0 || value % 6 == 0){
          printf("%d\n", value);
        }
      }

      index = 0;
      valid = 1;
    }
    else{
      valid = 0;
    }
  }

  if(index > 0 && valid){
    number[index] = '\0';

    int value = atoi(number);

    if(value % 5 == 0 || value % 6 == 0){
      printf("%d\n", value);
    }
  }
}

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "usage: sixfive file...\n");
    exit(1);
  }

  for(int i = 1; i < argc; i++){
    int fd = open(argv[i], 0);

    if(fd < 0){
      fprintf(2, "sixfive: cannot open %s\n", argv[i]);
      continue;
    }

    sixfive(fd);
    close(fd);
  }

  exit(0);
}
