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
    rj::StringBuffer s;
    rj::Writer<rj::StringBuffer> writer(s);
    writer.StartObject();

    auto item = getItem();
    while (item.type != ItemTypePrivate::End) {
        addItemToWriter(item, writer);
        item = getItem();
    }

    writer.EndObject();
    std::ofstream out {jsonFileName, std::ios::trunc};
    out << s.GetString();
}

void XmlToJson::readNode(const pt::ptree::value_type &node)
{
    ItemTypePrivate itemType;
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
    std::string nodeName;
    if (_ignoreNames.count(node.first) == 0) {
        nodeName = node.first;
    }
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
        typePrivate = XmlToJson::ItemTypePrivate::StartArray;
        break;
    case ItemType::Object:
        typePrivate = XmlToJson::ItemTypePrivate::StartObject;
        break;
    case ItemType::Double:
        typePrivate = XmlToJson::ItemTypePrivate::Double;
        break;
    case ItemType::Int:
        typePrivate = XmlToJson::ItemTypePrivate::Int;
        break;
    default:
        throw std::invalid_argument("unknown item type");
        break;
    }
    return typePrivate;
}

}
