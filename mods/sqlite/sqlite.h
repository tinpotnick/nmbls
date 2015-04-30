




#ifndef _MOD_SQLITE_H_
#define _MOD_SQLITE_H_

#include <vector>
#include <sqlite3.h>

typedef std::map<std::string, sqlite3 *> dbhandlemap;

class sqlrequest
{
public:

    sqlrequest(){}

     websockcon ref;
     std::string filename;
     std::string host;
     std::string hostroot;
     boost::function<void(std::string, std::string, std::string)> postResponceCallback;
     ValuePair http_headers;
     ValuePair params;
     //boost::function<void(std::string, std::string, std::string)> postResponceCallback;

};

typedef std::vector<sqlrequest> requestcontainer;


#endif /* _MOD_SQLITE_H_ */
