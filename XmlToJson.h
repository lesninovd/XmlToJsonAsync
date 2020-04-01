#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <condition_variable>

#include <boost/any.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace pt = boost::property_tree;

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace rj = rapidjson;

namespace xmltojson {

enum class ItemType
{
    Array,
    Object,
    Double,
    Int
};

class XmlToJson
{
    enum class ItemTypePrivate
    {
        StartArray,
        EndArray,
        StartObject,
        EndObject,
        Double,
        Int,
        String,
        End
    };

    struct Item
    {
        Item() = default;
        explicit Item(ItemTypePrivate type_, const std::string &name_ = "",
                      const boost::any value_ = boost::any())
            : type(type_)
            , name(name_)
            , value(value_)
        {}

        ItemTypePrivate type;
        std::string name;
        boost::any value;
    };

    friend void addItemToWriter(const Item &item, rj::Writer<rj::StringBuffer> &writer);

public:
    XmlToJson();

    void translate(const std::string &xmlFileName, const std::string &jsonFileName);

    template<typename... Args>
    void ignoreNodesByName(Args &&... args)
    { _ignoreNodes.emplace(std::forward<Args>(args)...); }

    template<typename... Args>
    void ignoreNames(Args &&... args)
    { _ignoreNames.emplace(std::forward<Args>(args)...); }

    template<typename... Args>
    void setTypeByName(Args &&... args)
    {
        std::pair<std::string, ItemType> pair {std::forward<Args>(args)...};
        _typesByName.emplace(pair.first, itemTypeToPrivate(pair.second));
    }

private:
    void readXml(const std::string &xmlFileName);
    void writeJson(const std::string &jsonFileName);

    void readNode(const pt::ptree::value_type &node);
    ItemTypePrivate addItem(const pt::ptree::value_type &node);
    void addItemEnd(ItemTypePrivate addedItemType);
    Item getItem();
    ItemTypePrivate itemTypeToPrivate(ItemType type);

    std::unordered_map<std::string, ItemTypePrivate> _typesByName;
    std::unordered_set<std::string> _ignoreNodes;
    std::unordered_set<std::string> _ignoreNames;

    std::mutex _queueMutex;
    std::condition_variable _waitItemCondition;
    std::deque<Item> _itemQueue;
};

inline void addItemToWriter(const XmlToJson::Item &item, rj::Writer<rj::StringBuffer> &writer)
{
    if (!item.name.empty()) {
        writer.Key(item.name.c_str());
    }
    switch (item.type) {
    case XmlToJson::ItemTypePrivate::StartArray:
        writer.StartArray();
        break;
    case XmlToJson::ItemTypePrivate::EndArray:
        writer.EndArray();
        break;
    case XmlToJson::ItemTypePrivate::StartObject:
        writer.StartObject();
        break;
    case XmlToJson::ItemTypePrivate::EndObject:
        writer.EndObject();
        break;
    case XmlToJson::ItemTypePrivate::String:
        writer.String(boost::any_cast<std::string>(item.value).c_str());
        break;
    case XmlToJson::ItemTypePrivate::Double:
        writer.Double(boost::any_cast<double>(item.value));
        break;
    case XmlToJson::ItemTypePrivate::Int:
        writer.Int(boost::any_cast<int>(item.value));
        break;
    default:
        throw std::invalid_argument("unknown item type");
        break;
    }
}

}
