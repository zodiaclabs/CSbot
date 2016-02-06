#ifndef CS_H
#define CS_H

void enumerate_dir(char *root,
                   void (*cb)(const char *name, int *stop, void *context),
                   void *context);

int map_exists(const char *map_name);
void map_list(Tox *tox, int friendnum);

int forkserver(const char *mapname);

#endif