/*  commands.c
 *
 *
 *  Copyright (C) 2016 toxcs All Rights Reserved.
 *
 *  This file is part of toxcs.
 *
 *  toxcs is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  toxcs is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with toxcs. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>
#include <signal.h>

#include <tox/tox.h>

#include "cs.h"
#include "toxcs.h"
#include "misc.h"

#define MAX_COMMAND_LENGTH TOX_MAX_MESSAGE_LENGTH
#define MAX_NUM_ARGS 4

extern struct Tox_Bot Tox_Bot;

static void authent_failed(Tox *m, int friendnum)
{
    const char *outmsg = "Invalid command.";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
}

static void cmd_start(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: a starting map is required (try 'cstrike de_dust2' or 'cstrike cs_assault')";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (!map_exists(argv[1])) {
        const char *fmt = "Error: map '%s' doesn't exist";
        char em[strlen(fmt) + strlen(argv[1])];
        sprintf(em, fmt, argv[1]);

        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) em, strlen(em), NULL);
        return;
    }

    if (Tox_Bot.server_pid != -1) {
        outmsg = "Server is already running";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int now_pid = forkserver(argv[1]);
    if (now_pid == -1) {
        outmsg = "Failed to execute system command";
    } else {
        outmsg = "Started server";
        Tox_Bot.server_pid = now_pid;
    }

    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    char *play_msg = "CSBot Has started, It's time to PLAY!";
    for(int i = Tox_Bot.num_friends; i >= 0; i-- ) {
        if (tox_friend_get_connection_status(m, i, NULL)){
            tox_friend_send_message(m, i, TOX_MESSAGE_TYPE_NORMAL, (uint8_t*)play_msg, strlen(play_msg), NULL);
        }
    }
}

static void cmd_stop(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (Tox_Bot.server_pid == -1) {
        outmsg = "Server is not running";
    } else {
        outmsg = "Shutting down server";
        int ok = kill(Tox_Bot.server_pid, SIGTERM);
        if (ok == -1) {
            perror("kill failed");
            outmsg = "kill failed";
        }
    }

    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
}

static void cmd_status(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = (Tox_Bot.server_pid != -1) ? "Server is running" : "Server is not running";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
}

static void cmd_maplist(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    map_list(m, friendnum);
}

/* Parses input command and puts args into arg array.
   Returns number of arguments on success, -1 on failure. */
static int parse_command(const char *input, char (*args)[MAX_COMMAND_LENGTH])
{
    char *cmd = strdup(input);

    if (cmd == NULL)
        exit(EXIT_FAILURE);

    int num_args = 0;
    int i = 0;    /* index of last char in an argument */

    /* characters wrapped in double quotes count as one arg */
    while (num_args < MAX_NUM_ARGS) {
        int qt_ofst = 0;    /* set to 1 to offset index for quote char at end of arg */

        if (*cmd == '\"') {
            qt_ofst = 1;
            i = char_find(1, cmd, '\"');

            if (cmd[i] == '\0') {
                free(cmd);
                return -1;
            }
        } else {
            i = char_find(0, cmd, ' ');
        }

        memcpy(args[num_args], cmd, i + qt_ofst);
        args[num_args++][i + qt_ofst] = '\0';

        if (cmd[i] == '\0')    /* no more args */
            break;

        char tmp[MAX_COMMAND_LENGTH];
        snprintf(tmp, sizeof(tmp), "%s", &cmd[i + 1]);
        strcpy(cmd, tmp);    /* tmp will always fit inside cmd */
    }

    free(cmd);
    return num_args;
}

static struct {
    const char *name;
    void (*func)(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH]);
} commands[] = {
    { "start",            cmd_start   },
    { "stop",             cmd_stop    },
    { "status",           cmd_status  },
    { "maplist",          cmd_maplist },
    { NULL,               NULL        },
};

static int do_command(Tox *m, int friendnum, int num_args, char (*args)[MAX_COMMAND_LENGTH])
{
    int i;

    for (i = 0; commands[i].name; ++i) {
        if (strcmp(args[0], commands[i].name) == 0) {
            (commands[i].func)(m, friendnum, num_args - 1, args);
            return 0;
        }
    }

    return -1;
}

int execute(Tox *m, int friendnum, const char *input, int length)
{
    if (length >= MAX_COMMAND_LENGTH)
        return -1;

    char args[MAX_NUM_ARGS][MAX_COMMAND_LENGTH];
    int num_args = parse_command(input, args);

    if (num_args == -1)
        return -1;

    return do_command(m, friendnum, num_args, args);
}
