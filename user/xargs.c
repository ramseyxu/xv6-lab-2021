#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
main(int argc, char *argv[])
{

  /*
    read from stdin, fd = 0 
    split input into lines
    foreach line, add line to argv, then call exec use new argv
  */
  // 1. read stdin
  // 2. split into lines
  // 3. fork process run each line
  // 4. wait process end
  // printf("argc %d\n", argc);
  // for (int i = 0; i < argc; i++)
  //   printf("%s ", argv[i]);
  // printf("\n");
  char buf[1024];
  int n = 0, tmp;
  while ( (tmp = read(0, buf + n, 1) ) != 0 ) {
    n+=tmp;
  }
  if (n <= 0)
    panic("no input from stdin");
  // else {
  //   printf("read %d bytes\n", n);
  //   for (int i = 0; i < n; i++)
  //     printf("%c %d\n", buf[i], buf[i]);
  // }
  buf[n] = 0;
  int lines_start[128];
  int lines = 1;
  lines_start[0] = 0;
  for (int i = 0; i < n; i++) {
    if (buf[i] == '\n') {
      buf[i] = '\0';
      if (i+1 == n)
        continue;
      lines_start[lines] = i+1;
      // printf("line %d start %d\n", lines, i+1);
      lines++;
    }
  }
  // printf("read %d lines\n", lines);

  char *new_argv[MAXARG];
  for (int i = 1; i < argc; i++)
    new_argv[i - 1] = argv[i];
  new_argv[argc] = 0;
  
  for (int i = 0; i < lines; i++) {
    new_argv[argc - 1] = buf + lines_start[i];
    // for (int j = 0; new_argv[j]; j++)
    //   printf("%s len:%d; ", new_argv[j], strlen(new_argv[j]));
    // printf("\n");
    if (fork() == 0) {
      exec(new_argv[0], new_argv);
      exit(0);
    }
  }

  for (int i = 0; i < lines; i++)
    wait(0);

  exit(0);
  return 0;
}
