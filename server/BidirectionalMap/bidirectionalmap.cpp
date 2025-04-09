// bidirectional_map.cpp

#include "bidirectionalmap.h"
#include <iostream>

// Inserts a new entry into the bidirectional map
void BidirectionalMap::insert(const std::string &dbID, int seID)
{
    dbToSe[dbID] = seID;
    seToDb[seID] = dbID;
}

// Checks if the bidirectional map contains a given dbID
bool BidirectionalMap::containsDbID(const std::string &dbID) const
{
    return dbToSe.find(dbID) != dbToSe.end();
}

// Checks if the bidirectional map contains a given seID
bool BidirectionalMap::containsSeID(int seID) const
{
    return seToDb.find(seID) != seToDb.end();
}

// Gets the seID associated with a given dbID
int BidirectionalMap::getSeID(const std::string &dbID) const
{
    if (containsDbID(dbID))
    {
        return dbToSe.at(dbID);
    }
    return -1; // Or throw an exception, or handle as needed
}

// Gets the dbID associated with a given seID
std::string BidirectionalMap::getDbID(int seID) const
{
    if (containsSeID(seID))
    {
        return seToDb.at(seID);
    }
    return ""; // Or throw an exception, or handle as needed
}

// Removes the entry associated with a given dbID
void BidirectionalMap::removeDbID(const std::string &dbID)
{
    if (containsDbID(dbID))
    {
        int seID = dbToSe[dbID];
        dbToSe.erase(dbID);
        seToDb.erase(seID);
    }
}

// Removes the entry associated with a given seID
void BidirectionalMap::removeSeID(int seID)
{
    if (containsSeID(seID))
    {
        std::string dbID = seToDb[seID];
        seToDb.erase(seID);
        dbToSe.erase(dbID);
    }
}

// Retrieves all entries in the bidirectional map as a string
std::string BidirectionalMap::getAllEntries() const
{
    std::string result;
    for (const auto &pair : dbToSe)
    {
        result += "DB ID: " + pair.first + ", SE ID: " + std::to_string(pair.second) + "\n";
    }
    return result;
}
