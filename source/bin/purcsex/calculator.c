/*
** calculator.c - event handlers for calculator
**
** Copyright (C) 2022 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of PurC Midnight Commander.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#include "purcsex.h"
#include "calculator.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define LEN_EXPRESSION  1024

struct sample_data {
    unsigned fraction;
    char expression[LEN_EXPRESSION];
};

struct sample_data *sample_initializer(const char *name)
{
    struct sample_data *data;

    LOG_DEBUG("%s is allocating buffer for expression\n", name);
    data = calloc(1, sizeof(struct sample_data));
    data->fraction = 10;

    return data;
}

void sample_terminator(const char *name, struct sample_data *data)
{
    LOG_DEBUG("%s is freeing buffer for expression\n", name);
    if (data)
        free(data);
}

void calc_change_fraction(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg)
{
    LOG_INFO("called\n");
}

void calc_click_digit(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg)
{
    LOG_INFO("called\n");
}

void calc_click_sign(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg)
{
    LOG_INFO("called\n");
}

void calc_click_clear(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg)
{
    LOG_INFO("called\n");
}

void calc_click_equal(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg)
{
    LOG_INFO("called\n");
}

