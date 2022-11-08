//
// Created by Max on 07/11/2022.
//

#ifndef HOMELOGGER_SQLITE_WRAPPER_H
#define HOMELOGGER_SQLITE_WRAPPER_H

#include "sqlite3.h"
#include <string>
#include <functional>

using sqlite_callback = std::function<void(int col_index, char* col_value, char* col_name)>;

class sqlite_wrapper
{
public:
  explicit sqlite_wrapper(const std::string& name);
  sqlite_wrapper(const sqlite_wrapper&) = delete;
  ~sqlite_wrapper();

  void exec(const std::string& sql, const sqlite_callback& callback) const;

private:
  sqlite3* db{};
};

#endif  // HOMELOGGER_SQLITE_WRAPPER_H
