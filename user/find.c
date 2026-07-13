#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/*
 * 执行 find 的 -exec 命令。
 *
 * cmdargv 例如：
 *   {"echo", "hi", 0}
 *
 * path 例如：
 *   "./wc"
 *
 * 最终执行：
 *   echo hi ./wc
 */
void
runexec(char **cmdargv, int cmdargc, char *path)
{
  char *args[MAXARG];
  int i;

  if(cmdargc + 1 >= MAXARG){
    fprintf(2, "find: too many arguments\n");
    return;
  }

  for(i = 0; i < cmdargc; i++){
    args[i] = cmdargv[i];
  }

  args[cmdargc] = path;
  args[cmdargc + 1] = 0;

  int pid = fork();

  if(pid < 0){
    fprintf(2, "find: fork failed\n");
    return;
  }

  if(pid == 0){
    exec(args[0], args);
    fprintf(2, "find: exec %s failed\n", args[0]);
    exit(1);
  }

  wait(0);
}

/*
 * 处理找到的匹配文件。
 *
 * execmode == 0：直接打印路径
 * execmode == 1：执行 -exec 后面的命令
 */
void
found(char *path, int execmode, char **cmdargv, int cmdargc)
{
  if(execmode){
    runexec(cmdargv, cmdargc, path);
  } else {
    printf("%s\n", path);
  }
}

/*
 * 从完整路径中取得最后一个文件名。
 *
 * 例如：
 *   "./a/aa/b" -> "b"
 */
char *
filename(char *path)
{
  char *p;

  for(p = path + strlen(path); p >= path && *p != '/'; p--)
    ;

  return p + 1;
}

/*
 * 递归查找目录树。
 */
void
find(char *path, char *target,
     int execmode, char **cmdargv, int cmdargc)
{
  int fd;
  struct stat st;
  struct dirent de;
  char buf[512];
  char *p;

  fd = open(path, O_RDONLY);

  if(fd < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){

  case T_DEVICE:
  case T_FILE:
    if(strcmp(filename(path), target) == 0){
      found(path, execmode, cmdargv, cmdargc);
    }
    break;

  case T_DIR:
    /*
     * 路径、斜杠、目录项名称和字符串结尾必须能放进 buf。
     */
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
      fprintf(2, "find: path too long\n");
      break;
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0){
        continue;
      }

      /*
       * xv6目录项名称可能没有 '\0'，
       * 所以先复制固定的 DIRSIZ 字节，再补 '\0'。
       */
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = '\0';

      /*
       * 不能递归进入当前目录和父目录，
       * 否则会无限递归。
       */
      if(strcmp(p, ".") == 0 || strcmp(p, "..") == 0){
        continue;
      }

      find(buf, target, execmode, cmdargv, cmdargc);
    }

    break;
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  int execindex = -1;
  int i;

  if(argc < 3){
    fprintf(2, "usage: find path name [-exec command args...]\n");
    exit(1);
  }

  /*
   * 查找是否存在 -exec。
   */
  for(i = 3; i < argc; i++){
    if(strcmp(argv[i], "-exec") == 0){
      execindex = i;
      break;
    }
  }

  if(execindex == -1){
    /*
     * 普通模式：
     * find . b
     */
    find(argv[1], argv[2], 0, 0, 0);
  } else {
    /*
     * -exec后面至少必须有一个程序名。
     */
    if(execindex + 1 >= argc){
      fprintf(2, "find: missing command after -exec\n");
      exit(1);
    }

    /*
     * argv[execindex + 1] 开始就是命令及其参数。
     */
    find(argv[1], argv[2], 1,
         &argv[execindex + 1],
         argc - execindex - 1);
  }

  exit(0);
}
