/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/
#include <string.h>
#include <sys/time.h>

#include "define.h"
#include "errcodes.h"

#include "group.h"

typedef struct tag_elem {
    char *name;

    neu_datatag_t *tag;

    UT_hash_handle hh;
} tag_elem_t;

struct neu_group {
    char *name;

    tag_elem_t *tags;
    uint32_t    interval;

    int64_t timestamp;
};

static UT_array *to_array(tag_elem_t *tags);
static UT_array *to_read_array(tag_elem_t *tags);
static void      split_static_array(tag_elem_t *tags, UT_array **static_tags,
                                    UT_array **other_tags);
static void      update_timestamp(neu_group_t *group);

neu_group_t *neu_group_new(const char *name, uint32_t interval)
{
    neu_group_t *group = calloc(1, sizeof(neu_group_t));

    group->name     = strdup(name);
    group->interval = interval;

    return group;
}

void neu_group_destroy(neu_group_t *group)
{
    tag_elem_t *el = NULL, *tmp = NULL;

    HASH_ITER(hh, group->tags, el, tmp)
    {
        HASH_DEL(group->tags, el);
        free(el->name);
        neu_tag_free(el->tag);
        free(el);
    }

    free(group->name);
    free(group);
}

const char *neu_group_get_name(const neu_group_t *group)
{
    return group->name;
}

uint32_t neu_group_get_interval(const neu_group_t *group)
{
    uint32_t interval = 0;

    interval = group->interval;

    return interval;
}

void neu_group_set_interval(neu_group_t *group, uint32_t interval)
{
    group->interval = interval;
}

int neu_group_update(neu_group_t *group, uint32_t interval)
{
    if (group->interval != interval) {
        group->interval = interval;
        update_timestamp(group);
    }

    return 0;
}

int neu_group_add_tag(neu_group_t *group, const neu_datatag_t *tag)
{
    tag_elem_t *el = NULL;

    HASH_FIND_STR(group->tags, tag->name, el);
    if (el != NULL) {
        return NEU_ERR_TAG_NAME_CONFLICT;
    }

    el       = calloc(1, sizeof(tag_elem_t));
    el->name = strdup(tag->name);
    el->tag  = neu_tag_dup(tag);

    HASH_ADD_STR(group->tags, name, el);
    update_timestamp(group);

    return 0;
}

int neu_group_update_tag(neu_group_t *group, const neu_datatag_t *tag)
{
    tag_elem_t *el  = NULL;
    int         ret = NEU_ERR_TAG_NOT_EXIST;

    HASH_FIND_STR(group->tags, tag->name, el);
    if (el != NULL) {
        neu_tag_copy(el->tag, tag);

        update_timestamp(group);
        ret = NEU_ERR_SUCCESS;
    }

    return ret;
}

int neu_group_del_tag(neu_group_t *group, const char *tag_name)
{
    tag_elem_t *el  = NULL;
    int         ret = NEU_ERR_TAG_NOT_EXIST;

    HASH_FIND_STR(group->tags, tag_name, el);
    if (el != NULL) {
        HASH_DEL(group->tags, el);
        free(el->name);
        neu_tag_free(el->tag);
        free(el);

        update_timestamp(group);
        ret = NEU_ERR_SUCCESS;
    }

    return ret;
}

UT_array *neu_group_get_tag(const neu_group_t *group)
{
    UT_array *array = NULL;

    array = to_array(group->tags);

    return array;
}

UT_array *neu_group_query_tag(neu_group_t *group, const char *name)
{
    tag_elem_t *el = NULL, *tmp = NULL;
    UT_array *  array = NULL;

    utarray_new(array, neu_tag_get_icd());
    HASH_ITER(hh, group->tags, el, tmp)
    {
        if (strstr(el->tag->name, name) != NULL) {
            utarray_push_back(array, el->tag);
        }
    }

    return array;
}

UT_array *neu_group_get_read_tag(neu_group_t *group)
{
    UT_array *array = NULL;

    array = to_read_array(group->tags);

    return array;
}

uint16_t neu_group_tag_size(const neu_group_t *group)
{
    uint16_t size = 0;

    size = HASH_COUNT(group->tags);

    return size;
}

neu_datatag_t *neu_group_find_tag(neu_group_t *group, const char *tag)
{
    tag_elem_t *   find   = NULL;
    neu_datatag_t *result = NULL;

    HASH_FIND_STR(group->tags, tag, find);
    if (find != NULL) {
        result = neu_tag_dup(find->tag);
    }

    return result;
}

void neu_group_split_static_tags(neu_group_t *group, UT_array **static_tags,
                                 UT_array **other_tags)
{
    return split_static_array(group->tags, static_tags, other_tags);
}

void neu_group_change_test(neu_group_t *group, int64_t timestamp, void *arg,
                           neu_group_change_fn fn)
{
    if (group->timestamp != timestamp) {
        UT_array *static_tags = NULL, *other_tags = NULL;
        split_static_array(group->tags, &static_tags, &other_tags);
        fn(arg, group->timestamp, static_tags, other_tags, group->interval);
    }
}

bool neu_group_is_change(neu_group_t *group, int64_t timestamp)
{
    bool change = false;

    change = group->timestamp != timestamp;

    return change;
}

static void update_timestamp(neu_group_t *group)
{
    struct timeval tv = { 0 };

    gettimeofday(&tv, NULL);

    group->timestamp = (int64_t) tv.tv_sec * 1000 * 1000 + (int64_t) tv.tv_usec;
}

static UT_array *to_array(tag_elem_t *tags)
{
    tag_elem_t *el = NULL, *tmp = NULL;
    UT_array *  array = NULL;

    utarray_new(array, neu_tag_get_icd());
    HASH_ITER(hh, tags, el, tmp) { utarray_push_back(array, el->tag); }

    return array;
}

static UT_array *to_read_array(tag_elem_t *tags)
{
    tag_elem_t *el = NULL, *tmp = NULL;
    UT_array *  array = NULL;

    utarray_new(array, neu_tag_get_icd());
    HASH_ITER(hh, tags, el, tmp)
    {
        if (neu_tag_attribute_test(el->tag, NEU_ATTRIBUTE_READ) ||
            neu_tag_attribute_test(el->tag, NEU_ATTRIBUTE_SUBSCRIBE) ||
            neu_tag_attribute_test(el->tag, NEU_ATTRIBUTE_STATIC)) {
            utarray_push_back(array, el->tag);
        }
    }

    return array;
}

static void split_static_array(tag_elem_t *tags, UT_array **static_tags,
                               UT_array **other_tags)
{
    tag_elem_t *el = NULL, *tmp = NULL;

    utarray_new(*static_tags, neu_tag_get_icd());
    utarray_new(*other_tags, neu_tag_get_icd());
    HASH_ITER(hh, tags, el, tmp)
    {
        if (neu_tag_attribute_test(el->tag, NEU_ATTRIBUTE_STATIC)) {
            utarray_push_back(*static_tags, el->tag);
        } else {
            utarray_push_back(*other_tags, el->tag);
        }
    }
}
