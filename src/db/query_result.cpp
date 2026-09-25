#include "query_result.h"

QueryResult::QueryResult()
    : m_database(nullptr), m_statement(nullptr), m_stepped(false), m_status(SQLITE_MISUSE)
{
}

QueryResult::QueryResult(sqlite3 *database, sqlite3_stmt *statement, int status)
    : m_database(database), m_statement(statement), m_stepped(false), m_status(status)
{
}

QueryResult::~QueryResult()
{
    if (m_statement != nullptr)
    {
        sqlite3_finalize(m_statement);
    }
}

QueryResult::QueryResult(QueryResult&& other) noexcept
    : m_database(other.m_database), m_statement(other.m_statement), m_stepped(other.m_stepped), m_status(other.m_status)
{
    other.m_database = nullptr;
    other.m_statement = nullptr;
    other.m_stepped = false;
    other.m_status = SQLITE_MISUSE;
}

QueryResult& QueryResult::operator=(QueryResult&& other) noexcept
{
    if (this != &other)
    {
        if (m_statement != nullptr)
        {
            sqlite3_finalize(m_statement);
        }
        m_database = other.m_database;
        m_statement = other.m_statement;
        m_stepped = other.m_stepped;
        m_status = other.m_status;
        other.m_database = nullptr;
        other.m_statement = nullptr;
        other.m_stepped = false;
        other.m_status = SQLITE_MISUSE;
    }
    return *this;
}

bool QueryResult::valid() const
{
    return m_statement != nullptr;
}

int QueryResult::status() const
{
    return m_status;
}

bool QueryResult::has_error() const
{
    return m_status != SQLITE_OK && m_status != SQLITE_ROW && m_status != SQLITE_DONE;
}

bool QueryResult::next()
{
    if (!valid())
    {
        return false;
    }

    int result = m_stepped ? sqlite3_step(m_statement) : m_status;
    m_stepped = true;
    m_status = result;
    if (result == SQLITE_ROW)
    {
        return true;
    }

    sqlite3_finalize(m_statement);
    m_statement = nullptr;
    return false;
}

bool QueryResult::get_string(unsigned int col, std::string& value)
{
    if (!valid() || col == 0)
    {
        return false;
    }

    const unsigned char *text_value = sqlite3_column_text(m_statement, static_cast<int>(col - 1));
    if (text_value == nullptr)
    {
        value.clear();
        return false;
    }

    value.assign(reinterpret_cast<const char *>(text_value));
    return true;
}

bool QueryResult::text(unsigned int col, std::string& value)
{
    return get_string(col, value);
}

char *QueryResult::get_string(char *value, std::size_t value_len, unsigned int col)
{
    if (!valid() || value == nullptr || value_len == 0 || col == 0)
    {
        return nullptr;
    }

    const unsigned char *text_value = sqlite3_column_text(m_statement, static_cast<int>(col - 1));
    if (text_value == nullptr)
    {
        value[0] = '\0';
        return nullptr;
    }

    snprintf(value, value_len, "%s", text_value);
    return value;
}

int QueryResult::number(unsigned int col) const
{
    if (!valid() || col == 0 || sqlite3_column_type(m_statement, static_cast<int>(col - 1)) == SQLITE_NULL)
    {
        return 0;
    }
    return sqlite3_column_int(m_statement, static_cast<int>(col - 1));
}

int QueryResult::get_number(unsigned int col) const
{
    return number(col);
}