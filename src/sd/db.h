/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef DB_H
#define DB_H

#include <inttypes.h>
#include <stdbool.h>

struct db;

struct client;
struct service;
struct sub;

struct db *db_create(void);
void db_destroy(struct db *db);

bool db_has_client(struct db *db, int64_t client_id);
struct client *db_get_client(struct db *db, int64_t client_id);
void db_add_client(struct db *db, int64_t client_id, struct client *client);
void db_del_client(struct db *db, int64_t client_id);

typedef bool (*db_foreach_client_cb)(int64_t client_id,
				      struct client *client,
				      void *foreach_cb_data);
void db_foreach_client(struct db *db, db_foreach_client_cb foreach_cb,
			void *foreach_cb_data);

bool db_has_service(struct db *db, int64_t service_id);
struct service *db_get_service(struct db *db, int64_t service_id);
void db_add_service(struct db *db, int64_t service_id, struct service *service);
void db_del_service(struct db *db, int64_t service_id);

typedef bool (*db_foreach_service_cb)(int64_t service_id,
				      struct service *service,
				      void *foreach_cb_data);
void db_foreach_service(struct db *db, db_foreach_service_cb foreach_cb,
			void *foreach_cb_data);

bool db_has_sub(struct db *db, int64_t sub_id);
struct sub *db_get_sub(struct db *db, int64_t sub_id);
void db_add_sub(struct db *db, int64_t sub_id, struct sub *sub);
void db_del_sub(struct db *db, int64_t sub_id);

typedef bool (*db_foreach_sub_cb)(int64_t sub_id, struct sub *sub,
				  void *foreach_cb_data);
void db_foreach_sub(struct db *db, db_foreach_sub_cb foreach_cb,
			void *foreach_cb_data);

#endif
