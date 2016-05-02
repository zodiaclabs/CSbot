/*  toxcs.c
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
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <tox/tox.h>

#include "misc.h"
#include "commands.h"
#include "toxcs.h"

bool FLAG_EXIT = false;    /* set on SIGINT */
char *DATA_FILE = "toxcs_save";
char *MASTERLIST_FILE = "masterkeys";

struct Tox_Bot Tox_Bot;

static void init_toxcs_state(void)
{
    Tox_Bot.start_time = (uint64_t) time(NULL);
    Tox_Bot.num_online_friends = 0;
    Tox_Bot.server_pid = -1;
}

static void catch_SIGINT(int sig)
{
    FLAG_EXIT = true;
}

static void exit_toxcs(Tox *m)
{
    save_data(m, DATA_FILE);
    tox_kill(m);
    exit(EXIT_SUCCESS);
}

/* Returns true if friendnumber's Tox ID is in the masterkeys list, false otherwise.
   Note that it only compares the public key portion of the IDs. */
bool friend_is_master(Tox *m, uint32_t friendnumber)
{
    if (!file_exists(MASTERLIST_FILE)) {
        FILE *fp = fopen(MASTERLIST_FILE, "w");

        if (fp == NULL) {
            fprintf(stderr, "Warning: failed to create masterkeys file\n");
            return false;
        }

        fclose(fp);
        fprintf(stderr, "Warning: creating new masterkeys file. Did you lose the old one?\n");
        return false;
    }

    FILE *fp = fopen(MASTERLIST_FILE, "r");

    if (fp == NULL) {
        fprintf(stderr, "Warning: failed to read masterkeys file\n");
        return false;
    }

    char friend_key[TOX_PUBLIC_KEY_SIZE];
    if (tox_friend_get_public_key(m, friendnumber, (uint8_t *) friend_key, NULL) == 0) {
        fclose(fp);
        return false;
    }

    char id[256];

    while (fgets(id, sizeof(id), fp)) {
        int len = strlen(id);

        if (--len < TOX_PUBLIC_KEY_SIZE)
            continue;

        char *key_bin = hex_string_to_bin(id);

        if (memcmp(key_bin, friend_key, TOX_PUBLIC_KEY_SIZE) == 0) {
            free(key_bin);
            fclose(fp);
            return true;
        }

        free(key_bin);
    }

    fclose(fp);
    return false;
}

/* START CALLBACKS */
static void cb_self_connection_change(Tox *m, TOX_CONNECTION connection_status, void *userdata)
{
    switch (connection_status) {
        case TOX_CONNECTION_NONE:
            fprintf(stderr, "Connection to Tox network has been lost\n");
            break;

        case TOX_CONNECTION_TCP:
            fprintf(stderr, "Connection to Tox network is weak (using TCP)\n");
            break;

        case TOX_CONNECTION_UDP:
            fprintf(stderr, "Connection to Tox network is strong (using UDP)\n");
            break;
    }
}

static void cb_friend_connection_change(Tox *m, uint32_t friendnumber, TOX_CONNECTION connection_status, void *userdata)
{
    /* Count the number of online friends.
     *
     * We have to do this the hard way because our convenient API function to get
     * the number of online friends has mysteriously vanished
     */

    Tox_Bot.num_online_friends = 0;

    size_t i, size = tox_self_get_friend_list_size(m);

    if (size == 0)
        return;

    Tox_Bot.num_friends = size;

    uint32_t list[size];
    tox_self_get_friend_list(m, list);

    for (i = 0; i < size; ++i) {
        if (tox_friend_get_connection_status(m, list[i], NULL) != TOX_CONNECTION_NONE)
            ++Tox_Bot.num_online_friends;
    }
}

static void cb_friend_request(Tox *m, const uint8_t *public_key, const uint8_t *data, size_t length,
                              void *userdata)
{
    TOX_ERR_FRIEND_ADD err;
    tox_friend_add_norequest(m, public_key, &err);

    if (err != TOX_ERR_FRIEND_ADD_OK)
        fprintf(stderr, "tox_friend_add_norequest failed (error %d)\n", err);

    save_data(m, DATA_FILE);
}

static void cb_friend_message(Tox *m, uint32_t friendnumber, TOX_MESSAGE_TYPE type, const uint8_t *string,
                              size_t length, void *userdata)
{
    if (type != TOX_MESSAGE_TYPE_NORMAL)
        return;

    const char *outmsg;
    char message[TOX_MAX_MESSAGE_LENGTH];
    length = copy_tox_str(message, sizeof(message), (const char *) string, length);
    message[length] = '\0';

    if (length && execute(m, friendnumber, message, length) == -1) {
        outmsg = "Invalid command. Type help for a list of commands";
        tox_friend_send_message(m, friendnumber, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
    }
}

/* END CALLBACKS */

int save_data(Tox *m, const char *path)
{
    if (path == NULL)
        goto on_error;

    FILE *fp = fopen(path, "wb");

    if (fp == NULL)
        return -1;

    size_t data_len = tox_get_savedata_size(m);
    char *data = malloc(data_len);

    if (data == NULL)
        goto on_error;

    tox_get_savedata(m, (uint8_t *) data);

    if (fwrite(data, data_len, 1, fp) != 1) {
        free(data);
        fclose(fp);
        goto on_error;
    }

    free(data);
    fclose(fp);
    return 0;

on_error:
    fprintf(stderr, "Warning: save_data failed\n");
    return -1;
}

static Tox *load_tox(struct Tox_Options *options, char *path)
{
    FILE *fp = fopen(path, "rb");
    Tox *m = NULL;

    if (fp == NULL) {
        TOX_ERR_NEW err;
        m = tox_new(options, &err);

        if (err != TOX_ERR_NEW_OK) {
            fprintf(stderr, "tox_new failed with error %d\n", err);
            return NULL;
        }

        save_data(m, path);
        return m;
    }

    off_t data_len = file_size(path);

    if (data_len == 0) {
        fclose(fp);
        return NULL;
    }

    char data[data_len];

    if (fread(data, sizeof(data), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    TOX_ERR_NEW err;
    options->savedata_type = TOX_SAVEDATA_TYPE_TOX_SAVE;
    options->savedata_data = (uint8_t *) data;
    options->savedata_length = data_len;

    m = tox_new(options, &err);

    if (err != TOX_ERR_NEW_OK) {
        fprintf(stderr, "tox_new failed with error %d\n", err);
        return NULL;
    }

    fclose(fp);
    return m;
}

static Tox *init_tox(void)
{
    struct Tox_Options tox_opts;
    memset(&tox_opts, 0, sizeof(struct Tox_Options));
    tox_options_default(&tox_opts);

    Tox *m = load_tox(&tox_opts, DATA_FILE);

    if (!m)
        return NULL;

    tox_callback_self_connection_status(m, cb_self_connection_change, NULL);
    tox_callback_friend_connection_status(m, cb_friend_connection_change, NULL);
    tox_callback_friend_request(m, cb_friend_request, NULL);
    tox_callback_friend_message(m, cb_friend_message, NULL);

    size_t s_len = tox_self_get_status_message_size(m);

    if (1 || s_len == 0) {
        const char *statusmsg = "Connect to cs.kirara.ca:27015 | Check server status with the 'status' command.";
        tox_self_set_status_message(m, (uint8_t *) statusmsg, strlen(statusmsg), NULL);
    }

    size_t n_len = tox_self_get_name_size(m);

    if (1 || n_len == 0)
        tox_self_set_name(m, (uint8_t *) "CSbot", strlen("CSbot"), NULL);

    return m;
}

/* TODO: hardcoding is bad stop being lazy */
static struct toxNodes {
    const char *ip;
    uint16_t    port;
    const char *key;
} nodes[] = {
    { "144.76.60.215",   33445, "04119E835DF3E78BACF0F84235B300546AF8B936F035185E2A8E9E0A67C8924F" },
    { "192.210.149.121", 33445, "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67" },
    { "195.154.119.113", 33445, "E398A69646B8CEACA9F0B84F553726C1C49270558C57DF5F3C368F05A7D71354" },
    { "46.38.239.179",   33445, "F5A1A38EFB6BD3C2C8AF8B10D85F0F89E931704D349F1D0720C3C4059AF2440A" },
    { "192.99.168.140",  33445, "6A4D0607A296838434A6A7DDF99F50EF9D60A2C510BBF31FE538A25CB6B4652F" },
    { NULL, 0, NULL },
};

static void bootstrap_DHT(Tox *m)
{
    int i;

    for (i = 0; nodes[i].ip; ++i) {
        char *key = hex_string_to_bin(nodes[i].key);

        TOX_ERR_BOOTSTRAP err;
        tox_bootstrap(m, nodes[i].ip, nodes[i].port, (uint8_t *) key, &err);
        free(key);

        if (err != TOX_ERR_BOOTSTRAP_OK)
            fprintf(stderr, "Failed to bootstrap DHT via: %s %d (error %d)\n", nodes[i].ip, nodes[i].port, err);
    }
}

static void print_profile_info(Tox *m)
{
    printf("ID: ");

    char address[TOX_ADDRESS_SIZE];
    tox_self_get_address(m, (uint8_t *) address);
    int i;

    for (i = 0; i < TOX_ADDRESS_SIZE; ++i) {
        char d[3];
        snprintf(d, sizeof(d), "%02X", address[i] & 0xff);
        printf("%s", d);
    }

    printf("\n");

    char name[TOX_MAX_NAME_LENGTH];
    size_t len = tox_self_get_name_size(m);
    tox_self_get_name(m, (uint8_t *) name);
    name[len] = '\0';

    size_t numfriends = tox_self_get_friend_list_size(m);
    printf("Name: %s\n", name);
    printf("Contacts: %d\n", (int) numfriends);
}

#define REC_TOX_DO_LOOPS_PER_SEC 25

/* Adjusts usleep value so that tox_do runs close to the recommended number of times per second */
static useconds_t optimal_msleepval(uint64_t *looptimer, uint64_t *loopcount, uint64_t cur_time, useconds_t msleepval)
{
    useconds_t new_sleep = MAX(msleepval, 3);
    ++(*loopcount);

    if (*looptimer == cur_time)
        return new_sleep;

    if (*loopcount != REC_TOX_DO_LOOPS_PER_SEC)
        new_sleep *= (double) *loopcount / REC_TOX_DO_LOOPS_PER_SEC;

    *looptimer = cur_time;
    *loopcount = 0;
    return new_sleep;
}

int main(int argc, char **argv)
{
    signal(SIGINT, catch_SIGINT);
    umask(S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    Tox *m = init_tox();

    if (m == NULL)
        exit(EXIT_FAILURE);

    init_toxcs_state();
    print_profile_info(m);
    bootstrap_DHT(m);

    uint64_t looptimer = (uint64_t) time(NULL);
    useconds_t msleepval = 40000;
    uint64_t loopcount = 0;

    while (!FLAG_EXIT) {
        tox_iterate(m);

        if (Tox_Bot.server_pid != -1 && waitpid(Tox_Bot.server_pid, NULL, WNOHANG) == Tox_Bot.server_pid) {
            Tox_Bot.server_pid = -1;
            puts("server died");
        }

        uint64_t cur_time = (uint64_t) time(NULL);
        msleepval = optimal_msleepval(&looptimer, &loopcount, cur_time, msleepval);
        usleep(msleepval);
    }

    exit_toxcs(m);
    return 0;
}
