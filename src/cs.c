#include <tox/tox.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "cs.h"

char *const MAP_DIR = "/opt/hl/game/cstrike/maps";
char *const SERVER_HOME = "/opt/hl/game";
char *const SERVER_BINARY = "/opt/hl/game/hlds_run";

void enumerate_dir(char *root, void (*cb)(const char *name, int *stop, void *context), void *context) {
    DIR *directory = opendir(root);

    if (!directory)
        return;

    int stop = 0;
    struct dirent *record = readdir(directory);
    while (record != NULL && !stop) {
        if (!strcmp(record->d_name, ".") || !strcmp(record->d_name, "..")) {
            record = readdir(directory);
            continue;
        }

        char *pth = calloc(strlen(root) + strlen(record->d_name) + 2, 1);
        strcat(pth, root);
        strcat(pth, "/");
        strcat(pth, record->d_name);

        struct stat attrs;
        if (stat(pth, &attrs) == 0) {
            if (attrs.st_mode & S_IFREG) {
                cb(record->d_name, &stop, context);
            }
        } else {
            perror(record->d_name);
        }

        record = readdir(directory);
        free(pth);
    }

    closedir(directory);
}


struct map_exists_context { int found; const char *search_filename; };
static void map_exists_callback(const char *name, int *stop, struct map_exists_context *context) {
    if (!strcmp(name, context->search_filename)) {
        context->found = 1;
        *stop = 1;
    }
}
int map_exists(const char *map_name) {
    char sfn[strlen(map_name) + 5];
    sprintf(sfn, "%s", map_name);
    strcat(sfn, ".bsp");

    struct map_exists_context ctx = {
        .found = 0,
        .search_filename = sfn
    };

    enumerate_dir(MAP_DIR, (void *) &map_exists_callback, &ctx);
    return ctx.found;
}


struct map_list_context { Tox *tox; int friend; };
static void map_list_callback(const char *name, int *stop, struct map_list_context *context) {
    if (strlen(name) > 4 && !strncmp(".bsp", name + strlen(name) - 4, 4))
        tox_friend_send_message(context->tox, context->friend, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) name, strlen(name), NULL);
}
void map_list(Tox *tox, int friendnum) {
    struct map_list_context ctx = {
        .tox = tox,
        .friend = friendnum,
    };

    enumerate_dir(MAP_DIR, (void *) &map_list_callback, &ctx);
}

int forkserver(const char *mapname) {
    pid_t pid;

    if((pid = fork()) == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        chdir(SERVER_HOME);
        execl(SERVER_BINARY, SERVER_BINARY, "-game", "cstrike", "+maxplayers", "10", "+map", mapname, "+exec", "server.cfg", (char *)NULL);
    } else {
        return pid;
    }

    return 0;
}
