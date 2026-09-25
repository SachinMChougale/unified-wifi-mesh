#ifndef DB_CLIENT_H
#define DB_CLIENT_H

#include <mutex>
#include <sqlite3.h>

#include "query_result.h"

class db_client_t {
    sqlite3 *m_con;
    char m_path[512];
    std::mutex m_mutex;

    int connect(const char *path);

public:
    int init(const char *path);
    QueryResult execute(const char *query);

    int recreate_db();
    int begin_transaction();
    int commit();
    int rollback();

    sqlite3 *native_handle() const;

    db_client_t();
    ~db_client_t();
};

#endif