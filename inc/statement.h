#ifndef STATEMENT_H
#define STATEMENT_H

#include <sqlite3.h>

class DatabaseSession;

class Statement {
public:
    Statement(DatabaseSession& session, const char *query);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    bool valid() const;
    int status() const;
    int step();
    sqlite3_stmt* native_handle() const;

private:
    sqlite3_stmt *m_statement;
    int m_status;
};

#endif