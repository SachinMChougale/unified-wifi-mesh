/**
 * Copyright 2023 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>
 #include "db_client.h"
 #include "em_base.h"

struct result_context_t {
    sqlite3_stmt *statement;
    bool stepped;
};

static const char *default_database_path = "/var/lib/unified-wifi-mesh/unified_wifi_mesh.db";

 int db_client_t::recreate_db()
 {
     if (!m_con) {
         printf("%s:%d: No database connection\n", __func__, __LINE__);
         return -1;
     }

    std::lock_guard<std::mutex> lock(m_mutex);
     sqlite3_stmt *statement = NULL;
     if (sqlite3_prepare_v2(m_con, "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%'", -1, &statement, NULL) != SQLITE_OK) {
         printf("%s:%d: Error listing tables: %s\n", __func__, __LINE__, sqlite3_errmsg(m_con));
         return -1;
     }
     std::vector<std::string> table_names;
     while (sqlite3_step(statement) == SQLITE_ROW) {
         const char *name = reinterpret_cast<const char *>(sqlite3_column_text(statement, 0));
         table_names.emplace_back(name != NULL ? name : "");
     }
     sqlite3_finalize(statement);
     for (const std::string &name : table_names) {
         char query[1024];
         snprintf(query, sizeof(query), "DROP TABLE IF EXISTS \"%s\"", name.c_str());
         if (sqlite3_exec(m_con, query, NULL, NULL, NULL) != SQLITE_OK) {
             printf("%s:%d: Error dropping table: %s\n", __func__, __LINE__, sqlite3_errmsg(m_con));
             return -1;
         }
     }
     return 0;
 }

 int db_client_t::begin_transaction()
 {
     return execute("BEGIN IMMEDIATE TRANSACTION") == NULL && sqlite3_get_autocommit(m_con) == 0 ? 0 : -1;
 }

 int db_client_t::commit()
 {
     return execute("COMMIT") == NULL && sqlite3_get_autocommit(m_con) != 0 ? 0 : -1;
 }

 int db_client_t::rollback()
 {
     return execute("ROLLBACK") == NULL && sqlite3_get_autocommit(m_con) != 0 ? 0 : -1;
 }

 void *db_client_t::execute(const char *query)
 {
     if (!m_con) {
         printf("%s:%d: Query: %s m_con is NULL, exiting\n", __func__, __LINE__, query);
         return NULL;
     }

    std::lock_guard<std::mutex> lock(m_mutex);
     sqlite3_stmt *statement = NULL;
     int rc = sqlite3_prepare_v2(m_con, query, -1, &statement, NULL);
     if (rc != SQLITE_OK) {
         printf("%s:%d: Query failed: %s\n", __func__, __LINE__, query);
         printf("%s:%d: Error: %s\n", __func__, __LINE__, sqlite3_errmsg(m_con));
         return NULL;
     }
    rc = sqlite3_step(statement);
     if (rc == SQLITE_DONE) {
         sqlite3_finalize(statement);
         return NULL;
     }
     if (rc != SQLITE_ROW) {
         printf("%s:%d: Error: %s\n", __func__, __LINE__, sqlite3_errmsg(m_con));
         sqlite3_finalize(statement);
         return NULL;
     }
     result_context_t *ctx = new result_context_t;
     ctx->statement = statement;
    ctx->stepped = false;

     return ctx;
 }

 bool db_client_t::next_result(void *ctx)
 {
     if (ctx == NULL) {
         return false;
     }

     result_context_t *res_ctx = static_cast<result_context_t *>(ctx);
    std::lock_guard<std::mutex> lock(m_mutex);
     int rc = res_ctx->stepped ? sqlite3_step(res_ctx->statement) : SQLITE_ROW;
     res_ctx->stepped = true;
     if (rc != SQLITE_ROW) {
         sqlite3_finalize(res_ctx->statement);
         delete res_ctx;
         return false;
     }

     return true;
 }

 void db_client_t::free_result(void *ctx)
 {
     if (ctx == NULL) {
         return;
     }
     std::lock_guard<std::mutex> lock(m_mutex);
     result_context_t *res_ctx = static_cast<result_context_t *>(ctx);
     sqlite3_finalize(res_ctx->statement);
     delete res_ctx;
 }

 char *db_client_t::get_string(void *ctx, char *str, unsigned int col)
 {
     if (ctx == NULL) {
         return NULL;
     }

     result_context_t *res_ctx = static_cast<result_context_t *>(ctx);

    std::lock_guard<std::mutex> lock(m_mutex);

     assert(col > 0);
     const unsigned char *value = sqlite3_column_text(res_ctx->statement, static_cast<int>(col - 1));
     if (value == NULL) {
         return NULL;
     }
     snprintf(str, static_cast<size_t>(sqlite3_column_bytes(res_ctx->statement, static_cast<int>(col - 1))) + 1, "%s", value);
     return str;
 }

 int db_client_t::get_number(void *ctx, unsigned int col)
 {
     assert(ctx != NULL);

     result_context_t *res_ctx = static_cast<result_context_t *>(ctx);

    std::lock_guard<std::mutex> lock(m_mutex);

     if (col == 0 || sqlite3_column_type(res_ctx->statement, static_cast<int>(col - 1)) == SQLITE_NULL) {
         return 0;
     }
     return sqlite3_column_int(res_ctx->statement, static_cast<int>(col - 1));
 }

 int db_client_t::connect(const char *path)
 {
     const char *database_path = (path != NULL && path[0] != '\0') ? path : default_database_path;
     snprintf(m_path, sizeof(m_path), "%s", database_path);
     std::filesystem::path file_path(database_path);
     std::error_code error;
     if (file_path.has_parent_path()) {
         std::filesystem::create_directories(file_path.parent_path(), error);
         if (error) {
             printf("%s:%d: Error creating database directory: %s\n", __func__, __LINE__, error.message().c_str());
             return -1;
         }
     }
     if (sqlite3_open(database_path, &m_con) != SQLITE_OK) {
         printf("%s:%d: sqlite3_open() failed: %s\n", __func__, __LINE__, sqlite3_errmsg(m_con));
         sqlite3_close(m_con);
         m_con = NULL;
         return -1;
     }
     sqlite3_busy_timeout(m_con, 5000);
     return 0;
 }

 int db_client_t::init(const char *path)
 {
     if (connect(path) != 0) {
         printf("%s:%d: Connect failed\n", __func__, __LINE__);
         return -1;
     }

     return 0;
 }

 db_client_t::db_client_t()
 {
     m_con = NULL;
 }

 db_client_t::~db_client_t()
 {
     if (m_con) {
         sqlite3_close(m_con);
         m_con = NULL;
     }
 }
