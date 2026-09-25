#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include "db_client.h"

namespace {

const char *test_database_path = "/tmp/unified-wifi-mesh-client-test.db";

class DbClientTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::remove(test_database_path);
        db.init(test_database_path);
        db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, username TEXT);");
        db.execute("INSERT INTO users VALUES (1, 'alice'), (2, 'bob');");
    }

    void TearDown() override
    {
        std::remove(test_database_path);
    }

    db_client_t db;
};

} // namespace

TEST_F(DbClientTest, QueryResultReadsRows)
{
    QueryResult result = db.execute("SELECT id, username FROM users ORDER BY id;");
    ASSERT_TRUE(result.valid());

    ASSERT_TRUE(result.next());
    EXPECT_EQ(result.get_number(1), 1);
    std::string username;
    ASSERT_TRUE(result.get_string(2, username));
    EXPECT_EQ(username, "alice");

    ASSERT_TRUE(result.next());
    EXPECT_EQ(result.get_number(1), 2);
    ASSERT_TRUE(result.get_string(2, username));
    EXPECT_EQ(username, "bob");
    EXPECT_FALSE(result.next());
}

TEST_F(DbClientTest, QueryResultAutomaticallyReleasesEarlyResult)
{
    {
        QueryResult result = db.execute("SELECT username FROM users;");
        ASSERT_TRUE(result.next());
    }

    QueryResult result = db.execute("SELECT COUNT(*) FROM users;");
    ASSERT_TRUE(result.next());
    EXPECT_EQ(result.get_number(1), 2);
}

TEST_F(DbClientTest, EmptyAndInvalidQueriesReturnInvalidResults)
{
    QueryResult empty_query = db.execute("");
    EXPECT_FALSE(empty_query.valid());
    EXPECT_TRUE(empty_query.has_error());

    QueryResult invalid_query = db.execute("SELECT missing FROM users;");
    EXPECT_FALSE(invalid_query.valid());
    EXPECT_TRUE(invalid_query.has_error());
}

TEST_F(DbClientTest, SelectWithNoRowsIsSuccessfulAndNotAnError)
{
    QueryResult result = db.execute("SELECT username FROM users WHERE id = 999;");
    ASSERT_TRUE(result.valid());
    EXPECT_FALSE(result.next());
    EXPECT_EQ(result.status(), SQLITE_DONE);
    EXPECT_FALSE(result.has_error());
}