#ifndef QUERY_RESULT_H
#define QUERY_RESULT_H

#include <cstddef>
#include <sqlite3.h>
#include <string>

class QueryResult {
public:
    QueryResult();
    QueryResult(sqlite3 *database, sqlite3_stmt *statement, int status);
    ~QueryResult();

    QueryResult(const QueryResult&) = delete;
    QueryResult& operator=(const QueryResult&) = delete;
    QueryResult(QueryResult&& other) noexcept;
    QueryResult& operator=(QueryResult&& other) noexcept;

    bool valid() const;
    int status() const;
    bool has_error() const;
    bool next();
    bool text(unsigned int col, std::string& value);
    int number(unsigned int col) const;
    int get_number(unsigned int col) const;

    bool get_string(unsigned int col, std::string& value);

    template <std::size_t N>
    char *get_string(char (&value)[N], unsigned int col)
    {
        return get_string(value, N, col);
    }

    char *get_string(char *value, std::size_t value_len, unsigned int col);

private:
    sqlite3 *m_database;
    sqlite3_stmt *m_statement;
    bool m_stepped;
    int m_status;
};

#endif