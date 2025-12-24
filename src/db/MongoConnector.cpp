#include "db/MongoConnector.hpp"
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

MongoConnector::MongoConnector(const std::string &uri_str, const std::string &db, const std::string &coll)
    : dbName(db), collectionName(coll)
{
    inst = std::make_unique<mongocxx::instance>();
    client = std::make_unique<mongocxx::client>(mongocxx::uri{uri_str});
}

MongoConnector::~MongoConnector() = default;

void MongoConnector::processAllDocuments(std::function<void(const RawDocument &)> callback)
{
    auto collection = (*client)[dbName][collectionName];
    auto cursor = collection.find({});

    uint32_t internalId = 0;
    for (auto &&doc : cursor)
    {

        RawDocument raw;
        raw.id = internalId++;

        if (doc["_id"])
        {
            raw.mongoId = doc["_id"].get_oid().value.to_string();
        }

        if (doc["url"] && doc["url"].type() == bsoncxx::type::k_string)
        {
            // В новых версиях используется get_string()
            auto str_view = doc["url"].get_string().value;
            raw.url = std::string(str_view);
        }

        if (doc["html"] && doc["html"].type() == bsoncxx::type::k_string)
        {
            auto str_view = doc["html"].get_string().value;
            raw.html = std::string(str_view);
        }

        callback(raw);

        if (raw.id % 200 == 0)
        {
            printf("Processed %u documents...\n", raw.id);
        }
    }
}