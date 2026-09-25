#ifndef DATABASE_SESSION_H
#define DATABASE_SESSION_H

#include "db_client.h"

class QueryResult;
class Statement;
class TransactionGuard;

class DatabaseSession {
public:
    explicit DatabaseSession(const char *path = nullptr);

    DatabaseSession(const DatabaseSession&) = delete;
    DatabaseSession& operator=(const DatabaseSession&) = delete;
    DatabaseSession(DatabaseSession&&) = delete;
    DatabaseSession& operator=(DatabaseSession&&) = delete;

    bool valid() const;
    int status() const;
    db_client_t& legacy_client();
    const db_client_t& legacy_client() const;
    sqlite3* native_handle() const;

    QueryResult execute(const char *query);

private:
    db_client_t m_client;
    int m_status;

    friend class Statement;
    friend class TransactionGuard;
};

#endif