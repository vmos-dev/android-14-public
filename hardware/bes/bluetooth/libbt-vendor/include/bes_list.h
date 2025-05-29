/***************************************************************************
 *
 * Copyright 2015-2019 BES.
 * All rights reserved. All unpublished rights reserved.
 *
 * No part of this work may be used or reproduced in any form or by any
 * means, or stored in a database or retrieval system, without prior written
 * permission of BES.
 *
 * Use of this work is governed by a license granted by BES.
 * This work contains confidential and proprietary information of
 * BES. which is protected by copyright, trade secret,
 * trademark and other intellectual property rights.
 *
 ****************************************************************************/
#pragma once

#ifndef __LIST_H__
#define __LIST_H__

#include <stdbool.h>
#include <stdlib.h>
#include <malloc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*list_free_cb)(void *data);
typedef bool (*list_iter_cb)(void *data);

typedef struct bes_list_node_st {
  struct bes_list_node_st *next;
  void *data;
} bes_list_node_t;

typedef struct bes_list_st {
  bes_list_node_t *head;
  bes_list_node_t *tail;
  size_t length;
  list_free_cb free_cb;
} bes_list_t;

// Lifecycle.
bes_list_t *bes_list_new(list_free_cb callback);
void bes_list_free(bes_list_t *list);

// Accessors.
bool bes_list_is_empty(const bes_list_t *list);
size_t bes_list_length(const bes_list_t *list);
void *bes_list_front(const bes_list_t *list);
void *bes_list_back(const bes_list_t *list);

// Mutators.
bool bes_list_insert_after(bes_list_t *list, bes_list_node_t *prev_node, void *data);
bool bes_list_prepend(bes_list_t *list, void *data);
bool bes_list_append(bes_list_t *list, void *data);
bool bes_list_remove(bes_list_t *list, void *data);
void bes_list_clear(bes_list_t *list);

// Iteration.
void bes_list_foreach(const bes_list_t *list, list_iter_cb callback);

bes_list_node_t *bes_list_begin(const bes_list_t *list);
bes_list_node_t *bes_list_end(const bes_list_t *list);
bes_list_node_t *bes_list_next(const bes_list_node_t *node);
void *bes_list_node(const bes_list_node_t *node);

#ifdef __cplusplus
	}
#endif

#endif//__FMDEC_H__


