#include "crow.h"
#include "influxdb.hpp"
#include "sqlite_wrapper.h"

const std::string influxdb_token =
    "****";
const std::string bucket = "sensors";
const std::string org = "home";

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
  influxdb_cpp::server_info server_info("127.0.0.1", 8086, org, influxdb_token, bucket);
  auto sqlite = std::make_shared<sqlite_wrapper>("home_logger.db");

  CROW_ROUTE(app, "/about")
  ([]() { return "About Home Logger."; });

  CROW_ROUTE(app, "/sensors/<string>/add")
      .methods("POST"_method)(
          [=](const crow::request& req, crow::response& res, const std::string& measurement)
          {
            auto body = crow::json::load(req.body);

            const auto& name = body["sensor_name"];
            CROW_LOG_INFO << "Adding sensor data with name: " << name;
            for (const auto& data_point : body["data"])
            {
              auto timestamp = data_point["timestamp"];
              auto value = data_point["value"];
              CROW_LOG_INFO << "Data point: " << value << " at " << timestamp;

              std::tm t{};
              std::istringstream ss((std::string)timestamp);

              ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S");
              if (ss.fail())
              {
                throw std::runtime_error{"failed to parse time string"};
              }

              std::time_t unix_timestamp = mktime(&t);
              std::uint64_t nano_unix_timestamp = unix_timestamp * 1000'000'000;
              double d_value = std::stod((std::string)value);

              auto r = influxdb_cpp::builder()
                           .meas(measurement)
                           .field((std::string)name, d_value)
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
        std::string query(
            R"(from(bucket: \")" + bucket +
            R"(\") |> range(start: -7d) |> filter(fn: (r)=>r[\"_measurement\"] == \")" +
            sensor_name + R"(\"))");

        influxdb_cpp::flux_query(resp, query, server_info);
        return resp;
      });

  CROW_ROUTE(app, "/sensors/<string>/add_mapping")
      .methods("POST"_method)(
          [=](const crow::request& req, crow::response& res, const std::string& measurement)
          {
            std::string from = req.url_params.get("from");
            std::string to = req.url_params.get("to");

            sqlite->exec("insert into field_mapping values (\"" + from + "\", \"" + to + "\");",
                         [](int col_index, char* col_value, char* col_name) {});
          });

  CROW_ROUTE(app, "/sensors/<string>/list_mapping")
  (
      [=](const crow::request& req, crow::response& res, const std::string& measurement)
      {
        std::string from = req.url_params.get("from");
        std::string to = req.url_params.get("to");

        sqlite->exec("select from_name, to_name from field_mapping;",
                     [](int col_index, char* col_value, char* col_name) {});
      });

  app.port(18080).multithreaded().run();
}