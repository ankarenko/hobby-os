#include <dirent.h>
#include <sys/stat.h>
#include <kernel/util/ansi_codes.h>
#include <stdio.h>

void main(int argc, char** argv) {
  dbg_log("\nHello %s", "my friend");
  DIR* dirp;
  bool has_arg = argc > 1;
  int size = 128;
  char path[size];
  memset(&path, 0, size);
  sprintf(&path, "%s/", !has_arg? "." : argv[1]);
  int base_path_len = strlen(path);
  char *base_path = &path[base_path_len];

  if ((dirp = opendir(path)) == NULL)
    return -1; 
    
  int maxlen = 16;
  char name[maxlen + 1];
  struct dirent* p_dirent = NULL;
  struct stat st;
  char res[32];
  struct tm lt;
  
  while ((p_dirent = readdir(dirp)) != NULL) {
    memset(&name, ' ', maxlen);
    memcpy(&name, p_dirent->d_name, strlen(p_dirent->d_name));
    name[maxlen] = '\0';
    
    sprintf(base_path, "%s\0", p_dirent->d_name);
    //printf(path);

    if (stat(path, &st) != 0) {
      printf(RED"\n[ERROR]:"COLOR_RESET "unable to fetch stats for teh file %s", p_dirent->d_name);
      goto error;
    }

    if (S_ISDIR(st.st_mode)) {
      printf("\n%s", name);
    } else if (S_ISCHR(st.st_mode)) {
      printf(BLU"\n%s", name);
      localtime_r(&st.st_ctime, &lt);
      strftime(&res, sizeof(res), "%H:%M %b %d", &lt);
      printf(res);
      printf(COLOR_RESET);

    } else if (S_ISREG(st.st_mode)) {
      printf(RED"\n%s", name);
      localtime_r(&st.st_ctime, &lt);
      strftime(&res, sizeof(res), "%H:%M %b %d", &lt);
      printf(res);
      printf("   %u bytes", st.st_size);
      printf(COLOR_RESET);
    }
  }

  if (closedir(dirp) < 0)
    goto error;

  _exit(0);
error:
  _exit(-1); 
}