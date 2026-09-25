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

static const char *default_database_path = "/var/lib/unified-wifi-mesh/unified_wifi_mesh.db";

// Not being used
int db_client_t::recreate_db()
{
    if (!m_con)
    {
        printf("%s:%d: No database connection\n", __func__, __LINE__);
        return -1;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(m_con, "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%'", -1, &statement, NULL) != SQLITE_OK)
    {
        printf("%s:%d: Error listing tables: %s\n", __func__, __LINE__, sqlite3_errmsg(m_con));
        return -1;
    }
    std::vector<std::string> table_names;
    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        const char *name = reinterpret_cast<const char *>(sqlite3_column_text(statement, 0));
        table_names.emplace_back(name != NULL ? name : "");
    }
    sqlite3_finalize(statement);
    for (const std::string &name : table_names)
    {
        char query[1024];
        snprintf(query, sizeof(query), "DROP TABLE IF EXISTS \"%s\"", name.c_str());
        if (sqlite3_exec(m_con, query, NULL, NULL, NULL) != SQLITE_OK)
        {
            printf("%s:%d: Error dropping table: %s\n", __func__, __LINE__, sqlite3_errmsg(m_con));
            return -1;
        }
    }
    return 0;
}

// Not being used
int db_client_t::begin_transaction()
{
    if (!m_con)
    {
        return -1;
    }
    QueryResult result = execute("BEGIN IMMEDIATE TRANSACTION");
    return !result.valid() && sqlite3_get_autocommit(m_con) == 0 ? 0 : -1;
}

// Not being used
int db_client_t::commit()
{
    if (!m_con)
    {
        return -1;
    }
    QueryResult result = execute("COMMIT");
    return !result.valid() && sqlite3_get_autocommit(m_con) != 0 ? 0 : -1;
}

// Not being used
int db_client_t::rollback()
{
    if (!m_con)
    {
        return -1;
    }
    QueryResult result = execute("ROLLBACK");
    return !result.valid() && sqlite3_get_autocommit(m_con) != 0 ? 0 : -1;
}

QueryResult db_client_t::execute(const char *query)
{
    // Ensure the database connection is valid before trying to execute SQL.
    if (!m_con)
    {
        printf("%s:%d: Query: %s m_con is NULL, exiting\n", __func__, __LINE__, query);
        return QueryResult(nullptr, nullptr, SQLITE_MISUSE);
    }

    // Serialize access to the SQLite handle so concurrent callers do not race.
    std::lock_guard<std::mutex> lock(m_mutex);

    // Prepare the SQL text into a compiled SQLite statement object.
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v2(m_con, query, -1, &statement, NULL);
    if (rc != SQLITE_OK)
    {
        printf("%s:%d: Query failed: %s\n", __func__, __LINE__, query);
        printf("%s:%d: Error: %s\n", __func__, __LINE__, sqlite3_errmsg(m_con));
        return QueryResult(m_con, nullptr, rc);
    }

    // Execute the prepared statement once.
    rc = sqlite3_step(statement);

    // Non-SELECT statements like INSERT/UPDATE/DELETE typically finish with SQLITE_DONE.
    if (rc == SQLITE_DONE)
    {
        if (sqlite3_column_count(statement) > 0)
        {
            return QueryResult(m_con, statement, SQLITE_DONE);
        }
        sqlite3_finalize(statement);
        return QueryResult(m_con, nullptr, SQLITE_DONE);
    }

    // Any other result type besides ROW means the query failed or returned an unexpected value.
    if (rc != SQLITE_ROW)
    {
        printf("%s:%d: Error: %s\n", __func__, __LINE__, sqlite3_errmsg(m_con));
        sqlite3_finalize(statement);
        return QueryResult(m_con, nullptr, rc);
    }

    return QueryResult(m_con, statement, rc);
}

int db_client_t::connect(const char *path)
{
    // Use the caller-provided database path, or fall back to the default location.
    const char *environment_database_path = getenv("EASYMESH_DB_FILE");
    const char *database_path = (path != NULL && path[0] != '\0') ? path :
        ((environment_database_path != NULL && environment_database_path[0] != '\0') ?
         environment_database_path : default_database_path);

    // Save the final path for later use or inspection.
    snprintf(m_path, sizeof(m_path), "%s", database_path);

    // Ensure the parent directory exists before creating/opening the SQLite file.
    std::filesystem::path file_path(database_path);
    std::error_code error;
    if (file_path.has_parent_path())
    {
        std::filesystem::create_directories(file_path.parent_path(), error);
        if (error)
        {
            printf("%s:%d: Error creating database directory: %s\n", __func__, __LINE__, error.message().c_str());
            return -1;
        }
    }

    // Open the SQLite database handle. If it fails, close any partially opened handle.
    if (sqlite3_open(database_path, &m_con) != SQLITE_OK)
    {
        printf("%s:%d: sqlite3_open() failed: %s\n", __func__, __LINE__, sqlite3_errmsg(m_con));
        sqlite3_close(m_con);
        m_con = NULL;
        return -1;
    }

    // Give SQLite a short busy timeout so transient locking does not fail immediately.
    sqlite3_busy_timeout(m_con, 5000);
    return 0;
}

int db_client_t::init(const char *path)
{
    if (connect(path) != 0)
    {
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
    if (m_con)
    {
        sqlite3_close(m_con);
        m_con = NULL;
    }
}

sqlite3 *db_client_t::native_handle() const
{
    return m_con;
}
