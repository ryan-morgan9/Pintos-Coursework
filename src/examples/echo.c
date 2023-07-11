#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  int i;
  printf("DEBUG - %d\n", argc);

  for (i = 0; i < argc; i++)
    printf ("%s \n", argv[i]);

  return EXIT_SUCCESS;
}
