//
// Created by Max on 07/11/2022.
//

#include "sqlite_wrapper.h"
#include <stdexcept>

int static_callback(void *object, int argc, char **argv, char **azColName)
{
  sqlite_callback& callback = *static_cast<sqlite_callback*>(object);
  for (int i = 0; i < argc; i++)
  {
    callback(i, argv[i], azColName[i]);
  }
  return 0;
}

sqlite_wrapper::sqlite_wrapper(const std::string& name)
{
  auto exit = sqlite3_open_v2(name.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
  if (exit)
  {
    throw std::runtime_error("Cannot open DB: " + name);
  }
}

sqlite_wrapper::~sqlite_wrapper()
{
  sqlite3_close(db);
}

void sqlite_wrapper::exec(const std::string& sql, const sqlite_callback& callback) const
{
  char *zErrMsg = {};
  auto rc = sqlite3_exec(db, sql.c_str(), static_callback, &callback, &zErrMsg);
  if (rc)
  {
    throw std::runtime_error("Error exec sql");
  }
}