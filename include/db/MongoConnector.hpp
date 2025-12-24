#ifndef MONGO_CONNECTOR_HPP
#define MONGO_CONNECTOR_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace mongocxx
{
    inline namespace v_noabi
    {
        class client;
        class collection;
        class instance;
    }
}

struct RawDocument
{
    uint32_t id;
    std::string mongoId;
    std::string url;
    std::string html;
};

class MongoConnector
{
private:
    std::unique_ptr<mongocxx::instance> inst;
    std::unique_ptr<mongocxx::client> client;
    std::string dbName;
    std::string collectionName;

public:
    MongoConnector(const std::string &uri, const std::string &db, const std::string &coll);
    ~MongoConnector();

    void processAllDocuments(std::function<void(const RawDocument &)> callback);
};

#endif