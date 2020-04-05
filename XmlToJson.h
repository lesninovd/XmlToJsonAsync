#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <condition_variable>

#include <boost/any.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace pt = boost::property_tree;

#include <nlohmann/json.hpp>
#include "fifo_map.hpp"

template<class K, class V, class dummy_compare, class A>
using fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;
using json = nlohmann::basic_json<fifo_map>;

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

public:
    XmlToJson();

    void translate(const std::string &xmlFileName, const std::string &jsonFileName);

    template<typename... Args>
    void ignoreNodesByName(Args &&... args)
    { _ignoreNodes.emplace(std::forward<Args>(args)...); }

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
    void addItemToJson(const Item &item);
    void collapseHierarchy();

    template<typename T>
    void addValue(const Item &item);

    std::unordered_map<std::string, ItemTypePrivate> _typesByName;
    std::unordered_set<std::string> _ignoreNodes;

    std::stack<std::pair<std::string, json>> _jsonByNameHierarchy;

    std::mutex _queueMutex;
    std::condition_variable _waitItemCondition;
    std::deque<Item> _itemQueue;
};

template<typename T>
void XmlToJson::addValue(const Item &item)
{
    auto value = boost::any_cast<T>(item.value);
    auto parent = &_jsonByNameHierarchy.top().second;
    if (parent->is_array()) {
        parent->emplace_back(value);
    } else {
        parent->emplace(item.name, value);
    }
}

}
