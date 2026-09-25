#include "database_session.h"
#include "query_result.h"
#include "statement.h"
#include "transaction_guard.h"

DatabaseSession::DatabaseSession(const char *path)
    : m_status(m_client.init(path))
{
}

bool DatabaseSession::valid() const
{
    return m_status == 0 && m_client.native_handle() != nullptr;
}

int DatabaseSession::status() const
{
    return m_status;
}

db_client_t& DatabaseSession::legacy_client()
{
    return m_client;
}

const db_client_t& DatabaseSession::legacy_client() const
{
    return m_client;
}

sqlite3* DatabaseSession::native_handle() const
{
    return m_client.native_handle();
}

QueryResult DatabaseSession::execute(const char *query)
{
    return valid() ? m_client.execute(query) : QueryResult();
}

TransactionGuard::TransactionGuard(DatabaseSession& session)
    : m_session(&session), m_active(false), m_status(-1)
{
    if (session.valid())
    {
        m_status = session.legacy_client().begin_transaction();
        m_active = (m_status == 0);
    }
}

TransactionGuard::~TransactionGuard()
{
    if (m_active)
    {
        m_session->legacy_client().rollback();
    }
}

bool TransactionGuard::active() const
{
    return m_active;
}

int TransactionGuard::status() const
{
    return m_status;
}

int TransactionGuard::commit()
{
    if (!m_active)
    {
        return m_status;
    }

    m_status = m_session->legacy_client().commit();
    if (m_status == 0)
    {
        m_active = false;
    }
    return m_status;
}

Statement::Statement(DatabaseSession& session, const char *query)
    : m_statement(nullptr), m_status(SQLITE_MISUSE)
{
    if (session.native_handle() != nullptr && query != nullptr)
    {
        m_status = sqlite3_prepare_v2(session.native_handle(), query, -1, &m_statement, nullptr);
    }
}

Statement::~Statement()
{
    if (m_statement != nullptr)
    {
        sqlite3_finalize(m_statement);
    }
}

Statement::Statement(Statement&& other) noexcept
    : m_statement(other.m_statement), m_status(other.m_status)
{
    other.m_statement = nullptr;
    other.m_status = SQLITE_MISUSE;
}

Statement& Statement::operator=(Statement&& other) noexcept
{
    if (this != &other)
    {
        if (m_statement != nullptr)
        {
            sqlite3_finalize(m_statement);
        }
        m_statement = other.m_statement;
        m_status = other.m_status;
        other.m_statement = nullptr;
        other.m_status = SQLITE_MISUSE;
    }
    return *this;
}

bool Statement::valid() const
{
    return m_statement != nullptr && m_status == SQLITE_OK;
}

int Statement::status() const
{
    return m_status;
}

int Statement::step()
{
    return valid() ? sqlite3_step(m_statement) : SQLITE_MISUSE;
}

sqlite3_stmt* Statement::native_handle() const
{
    return m_statement;
}