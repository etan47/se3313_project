#ifndef USER_H
#define USER_H

#include <string>

using namespace std;

class User
{
public:
    string email, password;

    User(string email, string password);
};

#endif
