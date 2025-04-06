// bidirectional_map.cpp

#include "bidirectionalmap.h"
#include <iostream>

void BidirectionalMap::insert(const std::string &dbID, int seID)
{
    dbToSe[dbID] = seID;
    seToDb[seID] = dbID;
}

bool BidirectionalMap::containsDbID(const std::string &dbID) const
{
    return dbToSe.find(dbID) != dbToSe.end();
}

bool BidirectionalMap::containsSeID(int seID) const
{
    return seToDb.find(seID) != seToDb.end();
}

int BidirectionalMap::getSeID(const std::string &dbID) const
{
    if (containsDbID(dbID))
    {
        return dbToSe.at(dbID);
    }
    return -1; // Or throw an exception, or handle as needed
}

std::string BidirectionalMap::getDbID(int seID) const
{
    if (containsSeID(seID))
    {
        return seToDb.at(seID);
    }
    return ""; // Or throw an exception, or handle as needed
}

void BidirectionalMap::removeDbID(const std::string &dbID)
{
    if (containsDbID(dbID))
    {
        int seID = dbToSe[dbID];
        dbToSe.erase(dbID);
        seToDb.erase(seID);
    }
}

void BidirectionalMap::removeSeID(int seID)
{
    if (containsSeID(seID))
    {
        std::string dbID = seToDb[seID];
        seToDb.erase(seID);
        dbToSe.erase(dbID);
    }
}