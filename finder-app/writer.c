#include <stdio.h>
#include <syslog.h>

#define ERROR   1
#define SUCCESS 0

int main(int argc, char** argv) {
  char *filePath    = argv[1];
  char *writeString = argv[2];
  int ret           = SUCCESS;
  FILE *fptr        = NULL;

  openlog(NULL, LOG_PID, LOG_USER);

  if (argc < 3) {
    syslog(LOG_ERR, "Requires two arguments");
    ret = ERROR;
    goto exit;
  }

  fptr = fopen(filePath, "w");
  fprintf(fptr, "%s\n", writeString);
  syslog(LOG_DEBUG, "Writing %s to %s\n", filePath, writeString);

exit:
  return ret;
}
