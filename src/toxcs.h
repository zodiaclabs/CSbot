/*  toxcs.h
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

#ifndef TOXBOT_H
#define TOXBOT_H

#include <stdint.h>
#include <tox/tox.h>
#include <stdbool.h>

struct Tox_Bot {
    uint64_t start_time;
    size_t num_friends;
    int num_online_friends;
    int server_pid;
};

int load_Masters(const char *path);
int save_data(Tox *m, const char *path);
bool friend_is_master(Tox *m, uint32_t friendnumber);

#endif /* TOXBOT_H */
