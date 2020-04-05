#include "XmlToJson.h"

#include <thread>

namespace xmltojson {

XmlToJson::XmlToJson()
{
    _ignoreNodes.emplace("<xmlattr>");
}

void XmlToJson::translate(const std::string &xmlFileName, const std::string &jsonFileName)
{
    std::thread readingThread(&XmlToJson::readXml, this, xmlFileName);
    std::thread writingThread(&XmlToJson::writeJson, this, jsonFileName);

    readingThread.join();
    writingThread.join();
}

void XmlToJson::readXml(const std::string &xmlFileName)
{
    pt::ptree tree;
    pt::read_xml(xmlFileName, tree);

    for(auto &node : tree) {
        readNode(node);
    }
    _itemQueue.emplace_back(ItemTypePrivate::End);
}

void XmlToJson::writeJson(const std::string &jsonFileName)
{
    json root;
    auto item = getItem();
    while (item.type != ItemTypePrivate::End) {
        addItemToJson(item);
        item = getItem();
    }

    auto jsonByName = _jsonByNameHierarchy.top();
    _jsonByNameHierarchy.pop();
    root.emplace(jsonByName.first, jsonByName.second);
    std::ofstream out {jsonFileName, std::ios::trunc};
    if (!out.is_open()) {
        throw std::invalid_argument("Failed to open file " + jsonFileName);
    }
    out << root;
}

void XmlToJson::addItemToJson(const XmlToJson::Item &item)
{
    switch (item.type) {
    case ItemTypePrivate::StartObject:
        _jsonByNameHierarchy.emplace(item.name, json::object());
        break;
    case ItemTypePrivate::StartArray:
        _jsonByNameHierarchy.emplace(item.name, json::array());
        break;
    case ItemTypePrivate::EndObject:
    case ItemTypePrivate::EndArray:
        collapseHierarchy();
        break;
    case ItemTypePrivate::String:
        addValue<std::string>(item);
        break;
    case ItemTypePrivate::Double:
        addValue<double>(item);
        break;
    case ItemTypePrivate::Int:
        addValue<int>(item);
        break;
    default:
        throw std::invalid_argument("unknown item type");
//        break;
    }
}

void XmlToJson::collapseHierarchy()
{
    if (_jsonByNameHierarchy.size() > 1) {
        auto jsonByName = _jsonByNameHierarchy.top();
        _jsonByNameHierarchy.pop();
        auto parent = &_jsonByNameHierarchy.top().second;
        if (parent->is_array()) {
            parent->emplace_back(jsonByName.second);
        } else {
            parent->emplace(jsonByName.first, jsonByName.second);
        }
    }
}

void XmlToJson::readNode(const pt::ptree::value_type &node)
{
    ItemTypePrivate itemType = ItemTypePrivate::String;
    if (_ignoreNodes.count(node.first) == 0) {
        itemType = addItem(node);
    }

    for(const auto &child : node.second.get_child("")) {
        readNode(child);
    }

    addItemEnd(itemType);
}

void XmlToJson::addItemEnd(ItemTypePrivate addedItemType)
{
    switch (addedItemType) {
    case ItemTypePrivate::StartArray:
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _itemQueue.emplace_back(ItemTypePrivate::EndArray);
    }
        break;
    case ItemTypePrivate::StartObject:
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _itemQueue.emplace_back(ItemTypePrivate::EndObject);
    }
        break;
    default:
        break;
    }
}

XmlToJson::Item XmlToJson::getItem()
{
    std::unique_lock<std::mutex> lock(_queueMutex);
    _waitItemCondition.wait(lock, [this] () { return !_itemQueue.empty(); });

    auto item = _itemQueue.front();
    _itemQueue.pop_front();
    return item;
}

XmlToJson::ItemTypePrivate XmlToJson::addItem(const pt::ptree::value_type &node)
{
    std::string nodeName = node.first;
    auto type = ItemTypePrivate::String;
    auto keyTypePairIt = _typesByName.find(node.first);
    if (keyTypePairIt != _typesByName.end()) {
        type = keyTypePairIt->second;
    }
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        switch (type) {
        case ItemTypePrivate::String:
            _itemQueue.emplace_back(type, nodeName, node.second.get<std::string>(""));
            break;
        case ItemTypePrivate::Double:
            _itemQueue.emplace_back(type, nodeName, node.second.get<double>(""));
            break;
        case ItemTypePrivate::Int:
            _itemQueue.emplace_back(type, nodeName, node.second.get<int>(""));
            break;
        default:
            _itemQueue.emplace_back(type, nodeName);
            break;
        }
        _waitItemCondition.notify_one();
    }
    return type;
}

XmlToJson::ItemTypePrivate XmlToJson::itemTypeToPrivate(ItemType type)
{
    XmlToJson::ItemTypePrivate typePrivate;
    switch (type) {
    case ItemType::Array:
        typePrivate = ItemTypePrivate::StartArray;
        break;
    case ItemType::Object:
        typePrivate = ItemTypePrivate::StartObject;
        break;
    case ItemType::Double:
        typePrivate = ItemTypePrivate::Double;
        break;
    case ItemType::Int:
        typePrivate = ItemTypePrivate::Int;
        break;
    }
    return typePrivate;
}

}
