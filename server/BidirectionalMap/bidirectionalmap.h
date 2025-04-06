// bidirectional_map.h

#ifndef BIDIRECTIONAL_MAP_H
#define BIDIRECTIONAL_MAP_H

#include <string>
#include <unordered_map>
#include <vector>

struct BidirectionalMap
{
    std::unordered_map<std::string, int> dbToSe;
    std::unordered_map<int, std::string> seToDb;

    void insert(const std::string &dbID, int seID);
    bool containsDbID(const std::string &dbID) const;
    bool containsSeID(int seID) const;
    int getSeID(const std::string &dbID) const;
    std::string getDbID(int seID) const;
    void removeDbID(const std::string &dbID);
    void removeSeID(int seID);
};

#endif // BIDIRECTIONAL_MAP_H