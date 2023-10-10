#include "crow.h"
#include "influxdb.hpp"
#include "sqlite_orm/sqlite_orm.h"
#include "date/date.h"

const std::string db = "sensors";

struct field_mapping
{
  std::string measurement;
  std::string from_field;
  std::string to_field;
};

std::string get_time_query(const crow::request& req)
{
  std::string daily = " where time >= now() - 24h group by time(1h)";
  std::string monthly = " where time >= now() - 31d group by time(1d)";
  std::string weekly = " where time >= now() - 7d group by time(1d)";

  auto interval = req.url_params.get("interval"); // daily / monthly
  if (interval == nullptr || interval == std::string_view("daily"))
  {
    return daily;
  }

  if (interval == std::string_view("weekly"))
  {
    return weekly;
  }

  return monthly;
}

std::string get_agg_query(const crow::request& req)
{
  auto type = req.url_params.get("type"); // mean / max / min
  if (type == nullptr || type == std::string_view("mean"))
  {
    return "mean";
  }

  return "max";
}


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

  storage.sync_schema();

  CROW_ROUTE(app, "/about")
  ([]() { return "About Home Logger."; });

  CROW_ROUTE(app, "/sensors/<string>/add")
      .methods("POST"_method)(
          [&](const crow::request& req, crow::response& res, const std::string& measurement)
          {
            if (req.body.empty())
            {
              res.end();
              return;
            }

            auto body = crow::json::load(req.body);

            // auto measurement = (std::string)body["measurement"];
            CROW_LOG_INFO << "Adding sensor data with name: " << measurement;
            for (const auto& data_points : body["data"])
            {
              auto name = (std::string)data_points.key();
              for (const auto& series : data_points)
              {
                auto timestamp = (std::string)series["timestamp"];
                auto value = (std::string)series["value"];
                CROW_LOG_INFO << "Data point: name " << name << " at " << value;

                std::istringstream in{std::move(timestamp)};
                std::chrono::sys_seconds tp;
                in >> date::parse("%Y%m%dT%H:%M:%S", tp);

                auto unix_timestamp = tp.time_since_epoch().count();
                std::uint64_t nano_unix_timestamp = unix_timestamp * 1000'000'000ull;
                double d_value = std::stod(value);

                CROW_LOG_INFO << "Adding with timestamp: " << nano_unix_timestamp;

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
            }

            res.end();
          });

  CROW_ROUTE(app, "/sensors/<string>/list/<string>")
  (
      [=](const crow::request& req, const std::string& sensor_name, const std::string& measurement_name)
      {
        auto time_query = get_time_query(req);
        auto agg_query = get_agg_query(req);

        std::string query = "select " + agg_query + "(" + measurement_name + ") from " + sensor_name + time_query;
        CROW_LOG_INFO << "Query " << query;

        // TODO passed by reference, is this thread safe?
        std::string resp;
        influxdb_cpp::query(resp, query, server_info);

        auto body = crow::json::load(resp);
        auto& latest_series = body["results"][0]["series"][0];

        auto& values = latest_series["values"];

        crow::json::wvalue::list results_json;
        for (auto& value: values)
        {
          crow::json::wvalue result_json;
          result_json["timestamp"] = value[0];
          result_json["value"] = value[1];
          results_json.push_back(result_json);
        }

        crow::json::wvalue result = results_json;
        return result.dump();
      });

  CROW_ROUTE(app, "/sensors/<string>/latest")
  (
      [=](const std::string& sensor_name)
      {
        std::string resp;
        std::string query("select * from " + sensor_name + " group by * order by desc limit 1");

        CROW_LOG_INFO << "Query " << query;

        // TODO passed by reference, is this thread safe?
        influxdb_cpp::query(resp, query, server_info);

        auto body = crow::json::load(resp);
        auto& latest_series = body["results"][0]["series"][0];

        auto& columns = latest_series["columns"];
        auto& values = latest_series["values"][0];

        crow::json::wvalue::list results_json;
        for (int i = 0; i < columns.size(); i++)
        {
          crow::json::wvalue result_json;
          result_json["name"] = columns[i];
          result_json["value"] = values[i];
          results_json.push_back(result_json);
        }

        crow::json::wvalue result = results_json;
        return result.dump();
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