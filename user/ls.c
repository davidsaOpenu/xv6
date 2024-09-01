#include "fs.h"
#include "lib/user.h"
#include "param.h"
#include "stat.h"
#include "types.h"

void ls(char *path) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  char cg_file_name[MAX_CGROUP_FILE_NAME_LENGTH];
  char proc_file_name[MAX_PROC_FILE_NAME_LENGTH];
  char temp_name_buffer[DIRSIZ + 1];

  if ((fd = open(path, 0)) < 0) {
    printf(stderr, "ls: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    printf(stderr, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type) {
    case T_FILE:
      if (!fmtname(path, temp_name_buffer, sizeof(temp_name_buffer))) {
        printf(stdout, "ls: path too long\n");
        break;
      }
      printf(stdout, "%s %d %d %d\n", temp_name_buffer, st.type, st.ino,
             st.size);
      break;

    case T_DIR:
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf(stdout, "ls: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
          printf(stdout, "ls: cannot stat %s\n", buf);
          continue;
        }
        if (!fmtname(buf, temp_name_buffer, sizeof(temp_name_buffer))) {
          printf(stdout, "ls: path too long\n");
          continue;
        }
        printf(stdout, "%s %d %d %d\n", temp_name_buffer, st.type, st.ino,
               st.size);
      }
      break;

    case T_CGFILE:
      if (!fmtname(path, temp_name_buffer, sizeof(temp_name_buffer))) {
        printf(stdout, "ls: path too long\n");
        break;
      }
      printf(stdout, "%s %d %d\n", temp_name_buffer, st.type, st.size);
      break;

    case T_CGDIR:
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf(stdout, "ls: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while (read(fd, cg_file_name, sizeof(cg_file_name)) ==
                 MAX_CGROUP_FILE_NAME_LENGTH &&
             cg_file_name[0] != ' ') {
        memmove(p, cg_file_name, MAX_CGROUP_FILE_NAME_LENGTH);
        p[MAX_CGROUP_FILE_NAME_LENGTH] = 0;
        int i = MAX_CGROUP_FILE_NAME_LENGTH - 1;
        while (p[i] == ' ') i--;
        p[i + 1] = 0;
        if (stat(buf, &st) < 0) {
          printf(stdout, "ls: cannot stat %s\n", buf);
          continue;
        }
        p[i + 1] = ' ';
        if (!fmtname(buf, temp_name_buffer, sizeof(temp_name_buffer))) {
          printf(stdout, "ls: path too long\n");
          continue;
        }
        printf(stdout, "%s %d %d\n", temp_name_buffer, st.type, st.size);
      }
      break;

    case T_PROCFILE:
      if (!fmtname(path, temp_name_buffer, sizeof(temp_name_buffer))) {
        printf(stdout, "ls: path too long\n");
        break;
      }
      printf(stdout, "%s %d %d\n", temp_name_buffer, st.type, st.size);
      break;

    case T_PROCDIR:
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf(stdout, "ls: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while (read(fd, proc_file_name, sizeof(proc_file_name)) ==
                 MAX_PROC_FILE_NAME_LENGTH &&
             proc_file_name[0] != ' ') {
        memmove(p, proc_file_name, MAX_PROC_FILE_NAME_LENGTH);
        p[MAX_PROC_FILE_NAME_LENGTH] = 0;
        int i = MAX_PROC_FILE_NAME_LENGTH - 1;
        while (p[i] == ' ') i--;
        p[i + 1] = 0;
        if (stat(buf, &st) < 0) {
          printf(stdout, "ls: cannot stat %s\n", buf);
          continue;
        }
        p[i + 1] = ' ';
        if (!fmtname(buf, temp_name_buffer, sizeof(temp_name_buffer))) {
          printf(stdout, "ls: path %s too long\n", buf);
          continue;
        }
        printf(stdout, "%s %d %d\n", temp_name_buffer, st.type, st.size);
      }
      break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  int i;

  if (argc < 2) {
    ls(".");
    exit(1);
  }
  for (i = 1; i < argc; i++) ls(argv[i]);
  exit(0);
}
