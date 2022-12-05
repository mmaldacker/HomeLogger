#include "crow.h"
#include "influxdb.hpp"
#include "sqlite_orm/sqlite_orm.h"

const std::string db = "sensors";

struct field_mapping
{
  std::string measurement;
  std::string from_field;
  std::string to_field;
};

int main()
{
#ifdef _WIN32
  WSADATA wsaData;
  auto r = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (r != 0)
  {
    std::cerr << "WSAStartup failed: " << r << std::endl;
    return 1;
  }
#endif

  crow::SimpleApp app;
  influxdb_cpp::server_info server_info("127.0.0.1", 8086, db);

  auto storage = sqlite_orm::make_storage(
      "home_logger.db",
      sqlite_orm::make_table("field_mapping",
                             sqlite_orm::make_column("measurement", &field_mapping::measurement),
                             sqlite_orm::make_column("from_field", &field_mapping::from_field),
                             sqlite_orm::make_column("to_field", &field_mapping::to_field)));

  CROW_ROUTE(app, "/about")
  ([]() { return "About Home Logger."; });

  CROW_ROUTE(app, "/sensors/<string>/add")
      .methods("POST"_method)(
          [&](const crow::request& req, crow::response& res, const std::string& measurement)
          {
            auto body = crow::json::load(req.body);

            // auto measurement = (std::string)body["measurement"];
            CROW_LOG_INFO << "Adding sensor data with name: " << measurement;
            for (const auto& data_point : body["data"])
            {
              auto name = (std::string)data_point["name"];
              // auto timestamp = data_point["timestamp"];
              auto value = (std::string)data_point["value"];
              CROW_LOG_INFO << "Data point: name " << name << " at " << value;
              /*
              std::tm t{};
              std::istringstream ss((std::string)timestamp);

              ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S");
              if (ss.fail())
              {
                throw std::runtime_error{"failed to parse time string"};
              }

              std::time_t unix_timestamp = mktime(&t);
              */
              std::time_t unix_timestamp = std::time(nullptr);
              std::uint64_t nano_unix_timestamp = unix_timestamp * 1000'000'000;
              double d_value = std::stod(value);

              // TODO passed by reference, is this thread safe?
              using namespace sqlite_orm;
              auto results = storage.get_all<field_mapping>(
                  where(c(&field_mapping::measurement) == measurement &&
                        c(&field_mapping::from_field) == name));

              if (!results.empty())
              {
                name = results[0].to_field;
              }

              // TODO passed by reference, is this thread safe?
              auto r = influxdb_cpp::builder()
                           .meas(measurement)
                           .field(name, d_value)
                           .timestamp(nano_unix_timestamp)
                           .post_http(server_info);
              CROW_LOG_INFO << "InfluxDB result: " << r;
            }

            res.end();
          });

  CROW_ROUTE(app, "/sensors/<string>/list")
  (
      [=](const std::string& sensor_name)
      {
        std::string resp;
        std::string query("select * from " + sensor_name);

        CROW_LOG_INFO << "Query " << query;

        // TODO passed by reference, is this thread safe?
        influxdb_cpp::query(resp, query, server_info);
        return resp;
      });

  CROW_ROUTE(app, "/sensors/<string>/add_mapping")
      .methods("POST"_method)(
          [&](const crow::request& req, crow::response& res, const std::string& measurement)
          {
            auto body = crow::json::load(req.body);
            field_mapping mapping;
            mapping.measurement = measurement;
            mapping.from_field = (std::string)body["from_field"];
            mapping.to_field = (std::string)body["to_field"];

            // TODO passed by reference, is this thread safe?
            storage.insert(mapping);

            res.end();
          });

  CROW_ROUTE(app, "/sensors/<string>/list_mappings")
  (
      [&](const crow::request& req, crow::response& res, const std::string& measurement)
      {
        // TODO passed by reference, is this thread safe?
        using namespace sqlite_orm;
        auto results =
            storage.get_all<field_mapping>(where(c(&field_mapping::measurement) == measurement));

        crow::json::wvalue::list results_json;
        for (auto& result : results)
        {
          crow::json::wvalue result_json;
          result_json["from_field"] = result.from_field;
          result_json["to_field"] = result.to_field;
          results_json.push_back(result_json);
        }

        crow::json::wvalue result = {{"measurement", measurement}, {"mappings", results_json}};
        res.end(result.dump());
      });

  app.port(18080).run();
}