/*
** purcsex.c - A simple example of PurCMC.
**
** Copyright (C) 2021, 2022 FMSoft (http://www.fmsoft.cn)
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>

#include <purc/purc.h>

#include "purcmc_version.h"

#define MAX_NR_WINDOWS      8
#define DEF_LEN_ONE_WRITE   1024

enum {
    STATE_INITIAL = 0,
    STATE_WINDOW_CREATED,
    STATE_DOCUMENT_WROTTEN,
    STATE_DOCUMENT_LOADED,
    STATE_EVENT_LOOP,
    STATE_WINDOW_DESTROYED,
    STATE_FATAL,
};

struct client_info {
    bool running;
    bool use_cmdline;

    uint32_t nr_windows;
    uint32_t nr_destroyed_wins;

    time_t last_sigint_time;
    size_t run_times;

    char app_name[PURC_LEN_APP_NAME + 1];
    char runner_name[PURC_LEN_RUNNER_NAME + 1];
    char sample_name[PURC_LEN_IDENTIFIER + 1];

    purc_variant_t sample;
    purc_variant_t initialOps;
    purc_variant_t namedOps;
    purc_variant_t events;

    size_t nr_ops;
    size_t nr_events;

    size_t ops_issued;
    size_t nr_windows_created;

    char *doc_content[MAX_NR_WINDOWS];
    size_t len_content[MAX_NR_WINDOWS];
    size_t len_wrotten[MAX_NR_WINDOWS];

    // handles of windows.
    uint64_t win_handles[MAX_NR_WINDOWS];

    // handles of DOM.
    uint64_t dom_handles[MAX_NR_WINDOWS];

    char buff[32];
};

static void print_copying(void)
{
    fprintf(stdout,
        "\n"
        "purcsex - a simple examples interacting with the PurCMC renderer.\n"
        "\n"
        "Copyright (C) 2021, 2022 FMSoft <https://www.fmsoft.cn>\n"
        "\n"
        "This program is free software: you can redistribute it and/or modify\n"
        "it under the terms of the GNU General Public License as published by\n"
        "the Free Software Foundation, either version 3 of the License, or\n"
        "(at your option) any later version.\n"
        "\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program.  If not, see http://www.gnu.org/licenses/.\n"
        );
    fprintf(stdout, "\n");
}

/* Command line help. */
static void print_usage(void)
{
    printf("purcsex (%s) - "
            "a simple example interacting with the PurCMC renderer\n\n",
            MC_CURRENT_VERSION);

    printf(
            "Usage: "
            "purcsex [ options ... ]\n\n"
            ""
            "The following options can be supplied to the command:\n\n"
            ""
            "  -a --app=<app_name>          - Connect to PurcMC renderer with the specified app name.\n"
            "  -r --runner=<runner_name>    - Connect to PurcMC renderer with the specified runner name.\n"
            "  -n --name=<sample_name>      - The sample name like `shownews`.\n"
            "  -v --version                 - Display version information and exit.\n"
            "  -h --help                    - This help.\n"
            "\n"
            );
}

static char short_options[] = "a:r:n:vh";
static struct option long_opts[] = {
    {"app"            , required_argument , NULL , 'a' } ,
    {"runner"         , required_argument , NULL , 'r' } ,
    {"name"           , required_argument , NULL , 'n' } ,
    {"version"        , no_argument       , NULL , 'v' } ,
    {"help"           , no_argument       , NULL , 'h' } ,
    {0, 0, 0, 0}
};

static int read_option_args(struct client_info *client, int argc, char **argv)
{
    int o, idx = 0;

    while ((o = getopt_long(argc, argv, short_options, long_opts, &idx)) >= 0) {
        if (-1 == o || EOF == o)
            break;
        switch (o) {
        case 'h':
            print_usage();
            return -1;
        case 'v':
            fprintf(stdout, "purcsex: %s\n", MC_CURRENT_VERSION);
            return -1;
        case 'a':
            if (purc_is_valid_app_name(optarg))
                strcpy(client->app_name, optarg);
            break;
        case 'r':
            if (purc_is_valid_runner_name(optarg))
                strcpy(client->runner_name, optarg);
            break;
        case 'n':
            if (purc_is_valid_token(optarg, PURC_LEN_IDENTIFIER))
                strcpy(client->sample_name, optarg);
            else {
                print_usage();
                return -1;
            }
            break;
        case '?':
            print_usage ();
            return -1;
        default:
            return -1;
        }
    }

    if (optind < argc) {
        print_usage ();
        return -1;
    }

    return 0;
}

static void format_current_time(char* buff, size_t sz, bool has_second)
{
    struct tm tm;
    time_t curr_time = time(NULL);

    localtime_r(&curr_time, &tm);
    if (has_second)
        strftime(buff, sz, "%H:%M:%S", &tm);
    else
        strftime(buff, sz, "%H:%M", &tm);
}

static char *load_file_content(const char *file, size_t *length)
{
    FILE *f = fopen(file, "r");
    char *buf = NULL;

    if (f) {
        if (fseek(f, 0, SEEK_END))
            goto failed;

        long len = ftell(f);
        if (len < 0)
            goto failed;

        buf = malloc(len + 1);
        if (buf == NULL)
            goto failed;

        fseek(f, 0, SEEK_SET);
        if (fread(buf, 1, len, f) < (size_t)len) {
            free(buf);
            buf = NULL;
        }
        buf[len] = '\0';

        if (length)
            *length = (size_t)len;
failed:
        fclose(f);
    }

    return buf;
}

static bool load_sample(struct client_info *info)
{
    char file[strlen(info->sample_name) + 8];
    strcpy(file, info->sample_name);
    strcat(file, ".json");

    info->sample = purc_variant_load_from_json_file(file);
    if (info->sample == PURC_VARIANT_INVALID) {
        fprintf(stderr, "Failed to load the sample from JSON file (%s)\n",
                info->sample_name);
        return false;
    }

    info->nr_windows = 0;
    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(info->sample, "nrWindows"))) {
        uint32_t nr_windows;
        if (purc_variant_cast_to_uint32(tmp, &nr_windows, false))
            info->nr_windows = nr_windows;
    }

    if (info->nr_windows == 0 || info->nr_windows > MAX_NR_WINDOWS) {
        fprintf(stdout, "WARN: Wrong number of windows (%u)\n", info->nr_windows);
        info->nr_windows = 1;
    }

    info->initialOps = purc_variant_object_get_by_ckey(info->sample, "initialOps");
    if (info->initialOps == PURC_VARIANT_INVALID ||
            !purc_variant_array_size(info->initialOps, &info->nr_ops)) {
        fprintf(stderr, "No valid `initialOps` defined.\n");
        return false;
    }

    info->namedOps = purc_variant_object_get_by_ckey(info->sample, "namedOps");
    if (info->namedOps == PURC_VARIANT_INVALID ||
            !purc_variant_is_object(info->namedOps)) {
        fprintf(stdout, "WARN: `namedOps` defined but not an object.\n");
        info->namedOps = PURC_VARIANT_INVALID;
    }

    info->events = purc_variant_object_get_by_ckey(info->sample, "events");
    if (info->events == PURC_VARIANT_INVALID ||
            !purc_variant_array_size(info->events, &info->nr_events)) {
        fprintf(stdout, "WARN: No valid `events` defined.\n");
        info->events = PURC_VARIANT_INVALID;
        info->nr_events = 0;
    }

    return true;
}

static void unload_sample(struct client_info *info)
{
    for (int i = 0; i < info->nr_windows; i++) {
        if (info->doc_content[i]) {
            free(info->doc_content[i]);
        }
    }

    if (info->sample) {
        purc_variant_unref(info->sample);
    }

    memset(info, 0, sizeof(*info));
}

static int split_target(const char *target, char *target_name)
{
    char *sep = strchr(target, '/');
    if (sep == NULL)
        return -1;

    size_t name_len = sep - target;
    if (name_len > PURC_LEN_IDENTIFIER) {
        return -1;
    }

    strncpy(target_name, target, name_len);
    target_name[name_len] = 0;

    sep++;
    if (sep[0] == 0)
        return -1;

    char *end;
    long l = strtol(sep, &end, 10);
    if (*end == 0)
        return (int)l;

    return -1;
}

static int split_target_deep(const char *source, uint64_t *target_value)
{
    return -1;
}

static const char* split_element(const char *element, char *element_type)
{
    char *sep = strchr(element, '/');
    if (sep == NULL)
        return NULL;

    size_t type_len = sep - element;
    if (type_len > PURC_LEN_IDENTIFIER) {
        return NULL;
    }

    strncpy(element_type, element, type_len);
    element_type[type_len] = 0;

    sep++;
    if (sep[0] == 0)
        return NULL;

    return sep;
}

static const char *transfer_element_info(struct client_info *info,
        const char *element, int *element_type)
{
    const char *value;
    char buff[PURC_LEN_IDENTIFIER + 1];

    value = split_element(element, buff);
    if (value) {
        if (strcmp(buff, "handle") == 0) {
            *element_type = PCRDR_MSG_ELEMENT_TYPE_HANDLE;
        }
        else if (strcmp(buff, "plainwindow") == 0) {
            *element_type = PCRDR_MSG_ELEMENT_TYPE_HANDLE;

            int win = atoi(value);
            if (win < 0 || win >= info->nr_windows) {
                value = NULL;
            }
            else {
                sprintf(info->buff, "%llx", (long long)info->win_handles[win]);
                value = info->buff;
            }
        }
    }

    return value;
}

static int issue_operation(pcrdr_conn* conn, purc_variant_t op);

static inline int issue_first_operation(pcrdr_conn* conn)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    info->ops_issued = 0;

    purc_variant_t op;
    op = purc_variant_array_get(info->initialOps, info->ops_issued);
    assert(op);
    return issue_operation(conn, op);
}

static inline int issue_next_operation(pcrdr_conn* conn)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    if (info->ops_issued < info->nr_ops) {
        info->ops_issued++;
        purc_variant_t op;
        op = purc_variant_array_get(info->initialOps, info->ops_issued);
        assert(op);
        return issue_operation(conn, op);
    }

    return 0;
}

static int plainwin_created_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    int win = (int)(uintptr_t)context;
    assert(win < info->nr_windows);

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    printf("Got a response for request (%s) to create plainwin (%d): %d\n",
            purc_variant_get_string_const(response_msg->requestId), win,
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        info->nr_windows_created++;
        info->win_handles[win] = response_msg->resultValue;
        issue_next_operation(conn);
    }
    else {
        fprintf(stderr, "failed to create a plain window\n");
        info->running = false;
    }

    return 0;
}

static int create_plain_win(pcrdr_conn* conn, purc_variant_t op)
{
    pcrdr_msg *msg;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    if (info->nr_windows == info->nr_windows_created)
        goto failed;
    int win = info->nr_windows_created;

    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_CREATEPLAINWINDOW, NULL,
            PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        goto failed;
    }

    char name_buff[64];
    sprintf(name_buff, "the-plain-window-%d", win);

    purc_variant_t tmp;
    const char *title = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "title"))) {
        title = purc_variant_get_string_const(tmp);
    }
    if (title == NULL)
        title = "No Title";

    purc_variant_t data = purc_variant_make_object(2,
            purc_variant_make_string_static("name", false),
            purc_variant_make_string_static(name_buff, false),
            purc_variant_make_string_static("title", false),
            purc_variant_make_string_static(title, false));
    if (data == PURC_VARIANT_INVALID) {
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_EJSON;
    msg->data = data;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                plainwin_created_handler) < 0) {
        goto failed;
    }

    printf("Request (%s) `%s` for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), win);
    pcrdr_release_message(msg);
    return 0;

failed:
    printf("Failed call to (%s) for window %d\n", __func__, win);

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static int loaded_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    int win = (int)(uintptr_t)context;
    assert(win < info->nr_windows);

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    printf("Got a response for request (%s) to load document content (%d): %d\n",
            purc_variant_get_string_const(response_msg->requestId), win,
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        info->dom_handles[win] = response_msg->resultValue;

        free(info->doc_content[win]);
        info->doc_content[win] = NULL;

        issue_next_operation(conn);
    }
    else {
        fprintf(stderr, "failed to load document\n");
        info->running = false;
    }

    return 0;
}

static int write_more_doucment(pcrdr_conn* conn, int win);

static int wrotten_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    int win = (int)(uintptr_t)context;
    assert(win < info->nr_windows);

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    printf("Got a response for request (%s) to write content (%d): %d\n",
            purc_variant_get_string_const(response_msg->requestId), win,
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        if (info->len_wrotten[win] == info->len_content[win]) {
            info->dom_handles[win] = response_msg->resultValue;

            free(info->doc_content[win]);
            info->doc_content[win] = NULL;

            issue_next_operation(conn);
        }
        else {
            write_more_doucment(conn, win);
        }
    }
    else {
        fprintf(stderr, "failed to write content\n");
        info->running = false;
    }

    return 0;
}

static int write_more_doucment(pcrdr_conn* conn, int win)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(win < info->nr_windows && info);

    pcrdr_msg *msg = NULL;
    purc_variant_t data = PURC_VARIANT_INVALID;

    pcrdr_response_handler handler;
    if (info->len_wrotten[win] + DEF_LEN_ONE_WRITE > info->len_content[win]) {
        // writeEnd
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_WRITEEND, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        data = purc_variant_make_string(
                info->doc_content[win] + info->len_wrotten[win], false);
        info->len_wrotten[win] = info->len_content[win];
        handler = loaded_handler;
    }
    else {
        // writeMore
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_WRITEMORE, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        const char *start = info->doc_content[win] + info->len_wrotten[win];
        const char *end;
        pcutils_string_check_utf8_len(start, DEF_LEN_ONE_WRITE, NULL, &end);
        if (end > start) {
            size_t len_to_write = end - start;

            data = purc_variant_make_string_ex(start, len_to_write, false);
            info->len_wrotten[win] += len_to_write;
        }
        else {
            printf("In %s for window %d: no valid character\n", __func__, win);
            goto failed;
        }
        handler = wrotten_handler;
    }

    if (msg == NULL || data == PURC_VARIANT_INVALID) {
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    msg->data = data;
    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                handler) < 0) {
        goto failed;
    }

    printf("Request (%s) `%s` for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), win);
    pcrdr_release_message(msg);
    return 0;

failed:
    printf("Failed call to (%s) for window %d\n", __func__, win);

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static int load_or_write_doucment(pcrdr_conn* conn, purc_variant_t op)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t tmp;
    const char *target = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "target"))) {
        target = purc_variant_get_string_const(tmp);
    }

    if (target == NULL) {
        goto failed;
    }

    char target_name[PURC_LEN_IDENTIFIER + 1];
    int win = 0;
    win = split_target(target, target_name);
    if (win < 0 || win > info->nr_windows_created || !info->win_handles[win]) {
        goto failed;
    }

    if (strcmp(target_name, "plainwindow")) {
        goto failed;
    }

    if (info->doc_content[win] == NULL) {
        const char *content;
        if ((tmp = purc_variant_object_get_by_ckey(op, "content"))) {
            content = purc_variant_get_string_const(tmp);
        }

        if (content) {
            info->doc_content[win] = load_file_content(content,
                    &info->len_content[win]);
        }
    }

    if (info->doc_content[win] == NULL) {
        goto failed;
    }

    pcrdr_msg *msg = NULL;
    purc_variant_t data = PURC_VARIANT_INVALID;

    pcrdr_response_handler handler;
    if (info->len_content[win] > DEF_LEN_ONE_WRITE) {
        // use writeBegin
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_WRITEBEGIN, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        const char *start = info->doc_content[win];
        const char *end;
        pcutils_string_check_utf8_len(start, DEF_LEN_ONE_WRITE, NULL, &end);
        if (end > start) {
            size_t len_to_write = end - start;

            data = purc_variant_make_string_ex(start, len_to_write, false);
            info->len_wrotten[win] = len_to_write;
        }
        else {
            printf("In %s for window %d: no valid character\n", __func__, win);
            goto failed;
        }

        handler = wrotten_handler;
    }
    else {
        // use load
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_LOAD, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        data = purc_variant_make_string_static(info->doc_content[win], false);
        info->len_wrotten[win] = info->len_content[win];
        handler = loaded_handler;
    }

    if (msg == NULL || data == PURC_VARIANT_INVALID) {
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    msg->data = data;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                handler) < 0) {
        goto failed;
    }

    printf("Request (%s) `%s` for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), win);
    pcrdr_release_message(msg);
    return 0;

failed:
    printf("Failed call to (%s) for window %d\n", __func__, win);

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static pcrdr_msg *make_change_message(struct client_info *info,
        int op_id, const char *operation, purc_variant_t op, int win)
{
    purc_variant_t tmp;
    const char *element = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "element"))) {
        element = purc_variant_get_string_const(tmp);
    }

    if (element == NULL) {
        goto failed;
    }

    char element_type[PURC_LEN_IDENTIFIER + 1];
    const char *element_value;

    element_value = split_element(element, element_type);
    if (element_value == NULL) {
        goto failed;
    }

    if (strcmp(element_type, "handle")) {
        goto failed;
    }

    const char *property = NULL;
    const char *content;
    char *content_loaded = NULL;
    size_t content_length;
    if (op_id == PCRDR_K_OPERATION_UPDATE) {
        if ((tmp = purc_variant_object_get_by_ckey(op, "property"))) {
            property = purc_variant_get_string_const(tmp);
        }

        if ((tmp = purc_variant_object_get_by_ckey(op, "content"))) {
            content = purc_variant_get_string_const(tmp);
        }

        if (content == NULL) {
            goto failed;
        }
    }
    else if (op_id == PCRDR_K_OPERATION_ERASE ||
            op_id == PCRDR_K_OPERATION_CLEAR) {
        if ((tmp = purc_variant_object_get_by_ckey(op, "property"))) {
            property = purc_variant_get_string_const(tmp);
        }
    }
    else {
        if ((tmp = purc_variant_object_get_by_ckey(op, "content"))) {
            content = purc_variant_get_string_const(tmp);
        }

        if (content) {
            content_loaded = load_file_content(content, &content_length);
            content = content_loaded;
        }

        if (content == NULL) {
            goto failed;
        }
    }

    pcrdr_msg *msg;
    msg = pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            operation, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, element_value,
            property,
            content ? PCRDR_MSG_DATA_TYPE_TEXT : PCRDR_MSG_DATA_TYPE_VOID,
            content, content_length);

    if (content_loaded) {
        free(content_loaded);
    }

    return msg;

failed:
    return NULL;
}

static int changed_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    int win = (int)(uintptr_t)context;
    assert(win < info->nr_windows);

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    printf("Got a response for request (%s) to change document (%d): %d\n",
            purc_variant_get_string_const(response_msg->requestId), win,
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        issue_next_operation(conn);
    }
    else {
        fprintf(stderr, "failed to load document\n");
        info->running = false;
    }

    return 0;
}

static int change_document(pcrdr_conn* conn,
        int op_id, const char *operation, purc_variant_t op)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t tmp;
    const char *target = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "target"))) {
        target = purc_variant_get_string_const(tmp);
    }

    if (target == NULL) {
        goto failed;
    }

    char target_name[PURC_LEN_IDENTIFIER + 1];
    int win = 0;
    win = split_target(target, target_name);
    if (win < 0 || win > info->nr_windows_created || !info->win_handles[win]) {
        goto failed;
    }

    if (strcmp(target_name, "dom")) {
        goto failed;
    }

    pcrdr_msg *msg = make_change_message(info, op_id, operation, op, win);
    if (msg == NULL)
        return -1;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                changed_handler) < 0) {
        goto failed;
    }

    printf("Request (%s) `%s` (%s) for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation),
            msg->property?purc_variant_get_string_const(msg->property):"N/A",
            win);
    pcrdr_release_message(msg);
    return 0;

failed:
    printf("Failed call to (%s) for window %d\n", __func__, win);

    pcrdr_release_message(msg);
    return -1;
}


static int issue_operation(pcrdr_conn* conn, purc_variant_t op)
{
    // struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t tmp;
    const char *operation = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "operation"))) {
        operation = purc_variant_get_string_const(tmp);
    }

    if (operation == NULL) {
        fprintf(stderr, "No valid `operation` defined in the operation.\n");
        return -1;
    }

    unsigned int op_id;
    purc_atom_t op_atom = pcrdr_try_operation_atom(operation);
    if (op_atom == 0 || pcrdr_operation_from_atom(op_atom, &op_id) == NULL) {
        fprintf(stderr, "Unknown operation: %s.\n", operation);
    }

    int retv;
    switch (op_id) {
    case PCRDR_K_OPERATION_CREATEPLAINWINDOW:
        retv = create_plain_win(conn, op);
        break;

    case PCRDR_K_OPERATION_LOAD:
        retv = load_or_write_doucment(conn, op);
        break;

    case PCRDR_K_OPERATION_DISPLACE:
        retv = change_document(conn, op_id, operation, op);
        break;

    default:
        fprintf(stderr, "Not implemented operation: %s.\n", operation);
        retv = -1;
        break;
    }

    return retv;
}

static ssize_t stdio_write(void *ctxt, const void *buf, size_t count)
{
    return fwrite(buf, 1, count, (FILE *)ctxt);
}

static const char *match_event(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg)
{
    const char *event = NULL;
    const char *source = NULL;
    const char *element = NULL;
    const char *namedOp = NULL;

    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(evt_vrt, "event"))) {
        event = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(evt_vrt, "source"))) {
        source = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(evt_vrt, "element"))) {
        element = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(evt_vrt, "namedOp"))) {
        namedOp = purc_variant_get_string_const(tmp);
    }

    if (event == NULL || source == NULL || namedOp == NULL) {
        goto failed;
    }

    if (strcmp(event, purc_variant_get_string_const(evt_msg->event)))
        goto failed;

    int target_type;
    uint64_t target_value;
    target_type = split_target_deep(source, &target_value);
    if (target_type != evt_msg->target ||
            target_value != evt_msg->targetValue) {
        goto failed;
    }

    if (element) {
        int element_type;
        const char* element_value;
        element_value = transfer_element_info(pcrdr_conn_get_user_data(conn),
                element, &element_type);
        if (element_type != evt_msg->elementType || element_value == NULL ||
                strcmp(element_value,
                    purc_variant_get_string_const(evt_msg->element))) {
            goto failed;
        }
    }

    return namedOp;

failed:
    return NULL;
}

static void my_event_handler(pcrdr_conn* conn, const pcrdr_msg *msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    const char *op_name = NULL;
    for (size_t i = 0; i < info->nr_events; i++) {
        purc_variant_t event = purc_variant_array_get(info->events, i);
        op_name = match_event(conn, event, msg);
        if (op_name)
            break;
    }

    if (op_name == NULL) {
        printf("Got an event not intrested in (target: %d/%p): %s\n",
                msg->target, (void *)(uintptr_t)msg->targetValue,
                purc_variant_get_string_const(msg->event));

        if (msg->target == PCRDR_MSG_TARGET_DOM) {
            printf("    The handle of the source element: %s\n",
                purc_variant_get_string_const(msg->element));
        }

        if (msg->dataType == PCRDR_MSG_DATA_TYPE_TEXT) {
            printf("    The attached data is TEXT:\n%s\n",
                purc_variant_get_string_const(msg->data));
        }
        else if (msg->dataType == PCRDR_MSG_DATA_TYPE_EJSON) {
            purc_rwstream_t rws = purc_rwstream_new_for_dump(stdout, stdio_write);

            printf("    The attached data is EJSON:\n");
            purc_variant_serialize(msg->data, rws, 0, 0, NULL);
            purc_rwstream_destroy(rws);
            printf("\n");
        }
        else {
            printf("    The attached data is VOID\n");
        }

        return;
    }

    // reserved built-in operation.
    if (strcmp(op_name, "QUIT") == 0) {
        info->running = false;
    }
    else {
        purc_variant_t op;
        op = purc_variant_object_get_by_ckey(info->namedOps, op_name);
        if (op == PURC_VARIANT_INVALID || !purc_variant_is_object(op)) {
            fprintf(stderr, "Bad named operation: %s\n", op_name);
        }
    }

}

int main(int argc, char **argv)
{
    int ret, cnnfd = -1, maxfd;
    pcrdr_conn* conn;
    fd_set rfds;
    struct timeval tv;
    char curr_time[16];

    purc_instance_extra_info extra_info = {
        .renderer_prot = PURC_RDRPROT_PURCMC,
        .renderer_uri = "unix://" PCRDR_PURCMC_US_PATH,
    };

    print_copying();

    struct client_info client;
    memset(&client, 0, sizeof(client));

    if (read_option_args(&client, argc, argv)) {
        return EXIT_FAILURE;
    }

    if (!client.app_name[0])
        strcpy(client.app_name, "cn.fmsoft.hvml.purcmc");
    if (!client.runner_name[0])
        strcpy(client.runner_name, "sample");
    if (!client.sample_name[0])
        strcpy(client.sample_name, client.runner_name);

    ret = purc_init_ex(PURC_MODULE_PCRDR, client.app_name,
            client.runner_name, &extra_info);
    if (ret != PURC_ERROR_OK) {
        fprintf(stderr, "Failed to initialize the PurC instance: %s\n",
                purc_get_error_message(ret));
        return EXIT_FAILURE;
    }

    conn = purc_get_conn_to_renderer();
    if (conn == NULL) {
        fprintf(stderr, "Failed to connect PURCMC renderer: %s\n",
                extra_info.renderer_uri);
        purc_cleanup();
        return EXIT_FAILURE;
    }

    if (!load_sample(&client)) {
        purc_cleanup();
        return EXIT_FAILURE;
    }

    client.running = true;
    client.last_sigint_time = 0;

    conn = purc_get_conn_to_renderer();
    assert(conn != NULL);
    cnnfd = pcrdr_conn_socket_fd(conn);
    assert(cnnfd >= 0);

    pcrdr_conn_set_user_data(conn, &client);
    pcrdr_conn_set_event_handler(conn, my_event_handler);

    format_current_time(curr_time, sizeof (curr_time) - 1, false);

    issue_first_operation(conn);

    maxfd = cnnfd;
    do {
        int retval;
        char new_clock[16];
        time_t old_time;

        FD_ZERO(&rfds);
        FD_SET(cnnfd, &rfds);

        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000;
        retval = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        if (retval == -1) {
            if (errno == EINTR)
                continue;
            else
                break;
        }
        else if (retval) {
            if (FD_ISSET(cnnfd, &rfds)) {
                int err_code = pcrdr_read_and_dispatch_message(conn);
                if (err_code < 0) {
                    fprintf (stderr, "Failed to read and dispatch message: %s\n",
                            purc_get_error_message(purc_get_last_error()));
                    break;
                }
            }
        }
        else {
            format_current_time(new_clock, sizeof (new_clock) - 1, false);
            if (strcmp(new_clock, curr_time)) {
                strcpy(curr_time, new_clock);
                pcrdr_ping_renderer(conn);
            }

            time_t new_time = time(NULL);
            if (old_time != new_time) {
                old_time = new_time;
                    break;
            }
        }

        if (purc_get_monotoic_time() > client.last_sigint_time + 5) {
            // cancel quit
            client.last_sigint_time = 0;
        }

    } while (client.running);

    fputs ("\n", stderr);

    unload_sample(&client);

    purc_cleanup();

    return EXIT_SUCCESS;
}

#if 0
static inline char *load_doc_content(struct client_info *info, size_t *length)
{
    char file[strlen(info->sample_name) + 8];
    strcpy(file, info->sample_name);
    strcat(file, ".html");


    return load_file_content(file, length);
}

static inline char *load_doc_fragment(struct client_info *info, size_t *length)
{
    char file[strlen(info->sample_name) + 8];
    strcpy(file, info->sample_name);
    strcat(file, ".frag");

    return load_file_content(file, length);
}

static int my_response_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    int win = (int)(uintptr_t)context;

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    printf("Got a response for request (%s) for window %d: %d\n",
            purc_variant_get_string_const(response_msg->requestId), win,
            response_msg->retCode);

    info->wait[win] = false;
    switch (info->state[win]) {
        case STATE_INITIAL:
            info->state[win] = STATE_WINDOW_CREATED;
            info->win_handles[win] = response_msg->resultValue;
            break;

        case STATE_WINDOW_CREATED:
            if (info->len_wrotten[win] < info->len_content) {
                info->state[win] = STATE_DOCUMENT_WROTTEN;
            }
            else {
                info->state[win] = STATE_DOCUMENT_LOADED;
                info->dom_handles[win] = response_msg->resultValue;
            }
            break;

        case STATE_DOCUMENT_WROTTEN:
            break;

        case STATE_DOCUMENT_LOADED:
            info->state[win] = STATE_EVENT_LOOP;
            break;
    }

    // we only allow failed request when we are running testing.
    if (info->state[win] != STATE_EVENT_LOOP &&
            response_msg->retCode != PCRDR_SC_OK) {
        info->state[win] = STATE_FATAL;

        printf("Window %d encountered a fatal error\n", win);
    }

    return 0;
}

static int check_quit(pcrdr_conn* conn)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    if (info->nr_windows_created == 0) {
        printf("no window alive; quitting...\n");
        return -1;
    }

    return 0;
}

{
    int win = info->run_times % info->nr_windows;
    info->run_times++;

    switch (info->state[win]) {
    case STATE_INITIAL:
        if (info->wait[win])
            return 0;
        return create_plain_win(conn, win);
    case STATE_WINDOW_CREATED:
        if (info->wait[win])
            return 0;
        return load_or_write_doucment(conn, win);
    case STATE_DOCUMENT_WROTTEN:
        if (info->wait[win])
            return 0;
        return write_more_doucment(conn, win);
    case STATE_DOCUMENT_LOADED:
        if (info->wait[win])
            return 0;
        return change_document(conn, win);
    case STATE_EVENT_LOOP:
        if (info->wait[win])
            return 0;
        return check_quit(conn, win);
    case STATE_FATAL:
        return -1;
    }

    return -1;
}

    switch (msg->target) {
    case PCRDR_MSG_TARGET_PLAINWINDOW:
        printf("Got an event to plainwindow (%p): %s\n",
                (void *)(uintptr_t)msg->targetValue,
                purc_variant_get_string_const(msg->event));

        int win = -1;
        for (int i = 0; i < info->nr_windows; i++) {
            if (info->win_handles[i] == msg->targetValue) {
                win = i;
                break;
            }
        }

        if (win >= 0) {
            info->win_handles[win] = 0;
            info->nr_windows_created--;
        }
        else {
            printf("Window not found: (%p)\n",
                    (void *)(uintptr_t)msg->targetValue);
        }
        break;

    case PCRDR_MSG_TARGET_SESSION:
    case PCRDR_MSG_TARGET_WORKSPACE:
    case PCRDR_MSG_TARGET_PAGE:
    case PCRDR_MSG_TARGET_DOM:
    default:
        printf("Got an event not intrested in (target: %d/%p): %s\n",
                msg->target, (void *)(uintptr_t)msg->targetValue,
                purc_variant_get_string_const(msg->event));

        if (msg->target == PCRDR_MSG_TARGET_DOM) {
            printf("    The handle of the source element: %s\n",
                purc_variant_get_string_const(msg->element));
        }

        if (msg->dataType == PCRDR_MSG_DATA_TYPE_TEXT) {
            printf("    The attached data is TEXT:\n%s\n",
                purc_variant_get_string_const(msg->data));
        }
        else if (msg->dataType == PCRDR_MSG_DATA_TYPE_EJSON) {
            purc_rwstream_t rws = purc_rwstream_new_for_dump(stdout, stdio_write);

            printf("    The attached data is EJSON:\n");
            purc_variant_serialize(msg->data, rws, 0, 0, NULL);
            purc_rwstream_destroy(rws);
            printf("\n");
        }
        else {
            printf("    The attached data is VOID\n");
        }
        break;
    }

#endif

