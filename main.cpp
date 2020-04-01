#include <iostream>
#include <fstream>

#include "XmlToJson.h"

namespace x2j = xmltojson;

int main()
{
    x2j::XmlToJson translator;
    translator.ignoreNames("subscriber");
    translator.setTypeByName("subscribers", x2j::ItemType::Array);
    translator.setTypeByName("subscriber", x2j::ItemType::Object);
    translator.setTypeByName("balance", x2j::ItemType::Double);

    translator.translate("input.xml", "output.json");

    return 0;
}
