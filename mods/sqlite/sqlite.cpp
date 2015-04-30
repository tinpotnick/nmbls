/*
 * sqlite.cpp
 *
 *  Created on: Nov 21, 2014
 *      Author: Nick Knight
 *
 * See LICENCE for more details.
 */

#include <iostream>


#include "../../nmbls.h"

// We handle the sqlite requests in a seperate thread
// create a pseudo async call.
#include <boost/thread/thread.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/lockfree/stack.hpp>
#include <iostream>
#include <sstream>

#include <boost/atomic.hpp>

#include <boost/bind.hpp>
#include <boost/function.hpp>

// For JSON support
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>

#include <boost/filesystem.hpp>

#include "sqlite.h"

#define WORKERBACKLOGSIZE 1024

//  We have a ring buffer of forms to pass off requests to our worker thread(s)
// doing it this way means we can get away without actually locking anything.
// The only locking mechanism we need is one semaphore to wake threads
// from idle.

// Worker generator thread, sets up the request object in the array of requests.
// once it has completed writing this information it will not touch it again.
// The worker generator then posts an index to this element to the workqueue
// which the workers can pick off and safley read the array element.
// For this to work, the array has to be large enough to contain all working items
// and signal when it is full.
static requestcontainer requests(WORKERBACKLOGSIZE);
static boost::atomic<int> nextavailable(0);
static boost::atomic<int> backlog(0);
static boost::interprocess::interprocess_semaphore sign(0);
static boost::lockfree::stack<int> workqueue(WORKERBACKLOGSIZE);

// Signal we are to shut down
static boost::atomic<bool> done (false);

// Finally our worker thread(s)
static boost::thread_group worker_thread;

static std::string docroot;

static bool is_running_as_debug = false;


void genHttpDoc(std::string body, std::ostringstream &doc)
{
    doc << "Content-type: application/json\r\n";
    doc << "Content-length: " << body.size() << "\r\n";
    doc << "Connection: Keep-Alive\r\n\r\n";

    doc << body;
}

std::string getParamValueFromName(std::string name,  requestcontainer::iterator workingon)
{
    ValuePair queryparams = workingon->params;
    ValuePair::iterator it;
    for ( it = queryparams.begin(); it != queryparams.end(); it++ )
    {
            std::cout << "query param:'" << it->first << "':'" << it->second << "'" << std::endl;
    }

    if ( 0 == name.find("get"))
    {
        std::string getparam = name.substr(4);
        std::string getvalue = workingon->params[getparam];
        std::cout << "get param is:'" << getparam << "'" << std::endl;
        std::cout << "get value is:" << getvalue;
        return getvalue;
    }

    return "1";
}

std::string escapeJsonString(const std::string& input) {
    std::ostringstream ss;
    for (auto iter = input.cbegin(); iter != input.cend(); iter++) {
    //C++98/03:
    //for (std::string::const_iterator iter = input.begin(); iter != input.end(); iter++) {
        switch (*iter) {
            case '\\': ss << "\\\\"; break;
            case '"': ss << "\\\""; break;
            case '/': ss << "\\/"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default: ss << *iter; break;
        }
    }
    return ss.str();
}


std::string getAndBindParamsFromSql(std::string &sqlstr, sqlite3_stmt * stmt, requestcontainer::iterator workingon)
{
    std::size_t lastfoundpos = 0;

    // More than 100 params! Seriously?
    for ( int i = 0; i < 100; i++)
    {
        std::size_t parampos = sqlstr.find(":", lastfoundpos);
        if ( std::string::npos == parampos )
        {
            break;
        }
        else
        {
            std::cout << "Found one" << std::endl;
            std::size_t endparam = sqlstr.find(" ", parampos+1);
            if ( std::string::npos == endparam )
            {
                return "";
            }
            std::string paramname = sqlstr.substr(parampos+1, endparam - parampos - 1 );
            std::cout << "Params is: " << paramname << std::endl;
            // Need to sort out the index value which will be wrong on statements when
            // a get param (or other) would be used more than once.
            std::string paramvalue = getParamValueFromName(paramname, workingon);
            sqlite3_bind_text( stmt, 1, paramvalue.c_str(), paramvalue.size(), SQLITE_TRANSIENT);
        }

        lastfoundpos = parampos + 1;
    }

    return "";
}

void initGDataTable(std::ostringstream &output, sqlite3_stmt *stmt)
{
    /*
    Example document we generate
    ref:
    https://developers.google.com/chart/interactive/docs/dev/implementing_data_source#jsondatatable
    Note: using this format there are a few JSON modifications fomr straight JSON.
    {
            "cols":[{"id":"Col1","label":"","type":"date"}],
            "rows":[
              {"c":[{"v":"a"},{"v":"Date(2010,10,6)"}]},
              {"c":[{"v":"b"},{"v":"Date(2010,10,7)"}]}
            ]
        }

        "rows":[
        {"c":[{"v":"1"},{"v":"Nick"},{"v":"077665544332"}]},{"c":[{"v":"2"},{"v":"Bob Builder"},{"v":"0123456789"}]}]}
    */

    output << "{\"cols\":[";

    int col_count = sqlite3_column_count(stmt);
    for ( int i = 0; i != col_count; i++ )
    {
        if ( 0 != i )
        {
                output << ",";
        }

        output << "{\"id\":\"";
        output << i;
        output << "\",\"label\":\"";
        output << escapeJsonString(sqlite3_column_name(stmt, i));
        output << "\",\"type\":";

        int col_type = sqlite3_column_type(stmt, i);

        switch(col_type)
        {
            case SQLITE_INTEGER:
            {
                output << "\"number\"}";
                break;
            }
            case SQLITE_FLOAT:
            {
                output << "\"number\"}";
            }
            case SQLITE_BLOB:
            {
                output << "\"string\"}";
            }
            case SQLITE_NULL:
                continue;
            // case SQLITE_TEXT:
            case SQLITE3_TEXT:
            {
                output << "\"string\"}";
            }
        }
        if ( i == (col_count  - 1) )
        {
            output << "]";
        }
    }

    // Now start the rows
    output << ",\"rows\":[";

}

void finiGDataTable(std::ostringstream &output, bool haverows)
{
    if ( haverows )
    {
        output << "]}";
    }
    else
    {
        // Current default - maybe we can send column definitionin the furture?
        output << "{}";
    }
}

void addGDataTableRow(std::ostringstream &output, int row, sqlite3_stmt *stmt)
{
    if ( row == 0 )
    {
        initGDataTable(output, stmt);
    }
    else
    {
        output << ",";
    }

    output << "{\"c\":[";
    int col_count = sqlite3_column_count(stmt);

    for ( int i = 0; i != col_count; i++ )
    {
        // {"c":[{"v":"a"},{"v":"Date(2010,10,6)"}]},

        // TODO, need to handle blob better. We should package up as base64 when supported.
        if ( i != 0 ) output << ",";

        output << "{\"v\":\"";
        output << escapeJsonString(std::string((const char *)sqlite3_column_text(stmt, i)));
        output << "\"}";

    }

    output << "]}";
}

void addJsonTableRow(std::ostringstream &output, int row, sqlite3_stmt *stmt)
{
    /*
    Example document we generate:
        [
         {
          "age": 13,
          "id": "motorola-defy-with-motoblur",
          "name": "Motorola DEFY\u2122 with MOTOBLUR\u2122",
          "snippet": "Are you ready for everything life throws your way?"
          ...
         },
        ...
        ]
     */
     if ( 0 == row )
     {
         output << "[{";
     }
     else
     {
         output << ",{";
     }

     int col_count = sqlite3_column_count(stmt);
     for ( int i = 0; i != col_count; i++ )
     {
         if ( 0 != i )
         {
             output << ",";
         }

         int col_type = sqlite3_column_type(stmt, i);

         output << "\"" << escapeJsonString(sqlite3_column_name(stmt, i)) << "\":";

         if ( col_type != SQLITE_INTEGER && col_type != SQLITE_FLOAT )
         {
             output << "\"";
         }

         output << escapeJsonString(std::string((const char *)sqlite3_column_text(stmt, i)));

         if ( col_type != SQLITE_INTEGER && col_type != SQLITE_FLOAT )
         {
             output << "\"";
         }
     }

     output << "}";
}

void finiJsonTableRow(std::ostringstream &output, bool haverows)
{
    if ( haverows)
    {
        output << "]";
    }
    else
    {
        output << "[]";
    }
}

static void readFile(std::string filename, std::string &obuf)
{
    int fd = open(filename.c_str(), O_RDONLY);
    if(fd == -1)
    {
        return;
    }

    /* Advise the kernel of our access pattern.  */
    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED);  // FDADVICE_SEQUENTIAL

    long sz = lseek(fd, 0L, SEEK_END);
    lseek(fd, 0L, SEEK_SET);

    char buf[sz + 1];

    while(size_t bytes_read = read(fd, buf, sz))
    {
        if(bytes_read == (size_t)-1)
            break;
        if (!bytes_read)
            break;

        buf[bytes_read] = 0;
        obuf += buf;
    }

    close(fd);
}

dbhandlemap dbhandles;
void sqliteworker(void)
{
    while(!done)
    {
        sign.wait();
        int nexttoworkon;
        while (workqueue.pop(nexttoworkon))
        {
            auto workingon = requests.begin() + nexttoworkon;
            std::string sqlfile = workingon->hostroot +  "data.sqlite";
            std::string sqlinitfile = workingon->hostroot +  "data.init";

            void (*addJsonRowFunction)(std::ostringstream &output, int row, sqlite3_stmt *stmt) = addJsonTableRow;
            void (*finiJsonFunction)(std::ostringstream &output, bool haverows) = finiJsonTableRow;

            try
            {
                workingon->params.at("formatdatatable");
                addJsonRowFunction = addGDataTableRow;
                finiJsonFunction = finiGDataTable;
            }
            catch (const std::out_of_range &x)
            {

            }

            boost::posix_time::ptime starttime;
            measureuSTime(starttime, is_running_as_debug);

            sqlite3 *dbhandle = NULL;
            try
            {
                dbhandle = dbhandles.at(workingon->host);
            }
            catch(const std::out_of_range &x)
            {
                bool needscreating = false;
                if ( !boost::filesystem::is_regular_file(sqlfile) )
                {
                    needscreating = true;
                }

                std::cout << "Need to open db" << std::endl;
                if ( SQLITE_OK == sqlite3_open_v2(sqlfile.c_str(),  &dbhandle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL))
                {
                    dbhandles[workingon->host] = dbhandle;
                    dbhandle = dbhandle;

                    if (needscreating)
                    {
                        char *errmsg = 0;
                        std::ifstream insql(sqlinitfile);
                        if ( !insql.fail())
                        {
                            std::string createsql(static_cast<std::stringstream const&>(std::stringstream() << insql.rdbuf()).str());
                            int rc = sqlite3_exec(dbhandle, createsql.c_str(), NULL, 0, &errmsg);
                            if( rc != SQLITE_OK )
                            {
                                std::cerr << "Error running SQL: " <<  errmsg <<std::endl;

                                std::ostringstream doc;
                                genHttpDoc("", doc);

                                workingon->postResponceCallback("500", errmsg, doc.str());
                                sqlite3_free(errmsg);

                                sqlite3_close_v2(dbhandle);
                                backlog--;
                                continue;
                            }

                            measureuSTime(starttime, is_running_as_debug, "Time taken (sqlite3_exec - init)");
                        }
                    }
                }
            }

            if ( NULL != dbhandle )
            {
                measureuSTime(starttime, is_running_as_debug, "Time taken (sqlite3_open_v2)");

                try
                {
                    std::string insql;
                    readFile(workingon->filename, insql);

                    measureuSTime(starttime, is_running_as_debug, "Time taken (read sql)");

                    sqlite3_stmt *stmt;
                    const char *sqlltail;
                    int rc = sqlite3_prepare_v2(dbhandle, insql.c_str(), insql.size(), &stmt, &sqlltail);

                    measureuSTime(starttime, is_running_as_debug, "Time taken (sqlite3_prepare_v2)");

                    if( rc != SQLITE_OK )
                    {
                        std::string errormsg = "{error: \"";
                        errormsg += "Error preparing SQL statement";

                        std::cerr << "Error preparing: " << insql << std::endl;

                        errormsg +=  "\"}";

                        std::ostringstream doc;
                        genHttpDoc("", doc);

                        workingon->postResponceCallback("200", "OK", doc.str());
                    }
                    else
                    {
                        getAndBindParamsFromSql(insql, stmt, workingon);

                        measureuSTime(starttime, is_running_as_debug, "Time taken (bind)");

                        std::ostringstream jsondoc;
                        bool haverows = false;

                        // Have some sencible limit
                        for ( int row = 0; row < 10000; row++)
                        {
                            int step_result = sqlite3_step(stmt);
                            if ( SQLITE_ROW != step_result )
                            {
                                // Probably SQLITE_DONE
                                break;
                            }

                            haverows = true;
                            addJsonRowFunction(jsondoc, row, stmt);
                            //addGDataTableRow(jsondoc, row, stmt);
                        }
                        sqlite3_finalize(stmt);

                        //finiGDataTable(jsondoc, haverows);
                        finiJsonFunction(jsondoc, haverows);

                        std::ostringstream doc;
                        genHttpDoc(jsondoc.str(), doc);

                        workingon->postResponceCallback("200", "OK", doc.str());
                    }
                }
                catch (std::ifstream::failure e)
                {
                    std::cerr << "Exception opening/reading/closing file: " << workingon->filename << std::endl;
                }
            }
            else
            {
                workingon->postResponceCallback("404", "Not found", "Something bad just happened...");
            }

            //sqlite3_close_v2(dbhandle);
            backlog--;
        }
    }
}

NMBLSEXPORT void handleLoadLibrary(boost::asio::io_service &, std::string root, bool debug)
{
    docroot = root;

    worker_thread.create_thread(sqliteworker);

    std::cout << "Hi form handle load lib" << std::endl;

    is_running_as_debug = debug;
    sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);
}

NMBLSEXPORT void handleUnLoadLibrary(void)
{
    done = true;
    sign.post();

    // Currently throwing: what():  boost: mutex lock failed in pthread_mutex_lock: Invalid argument
    // But given we are using an atomic done flag - is this needed?
    // worker_thread.join_all();

    for ( auto it = dbhandles.begin(); it != dbhandles.end(); it++)
    {
        sqlite3_close_v2(it->second);
    }

    std::cout << "Oppertunity to clean up..." << std::endl;
}

NMBLSEXPORT bool handleNewWSConnection(websockcon ref, std::string file, ValuePair &httpheaders, ValuePair &queryparams, boost::function<void(std::string)> postTextCallback)
{
    return false;
}

NMBLSEXPORT void handleDestroyConnection(websockcon ref)
{

}

NMBLSEXPORT bool handleTextPacket(websockcon ref, std::string str)
{
    return false;
}

NMBLSEXPORT bool handleWebRequest(std::string method, websockcon ref, std::string webfilename, ValuePair &queryparams, ValuePair &httpheaders, boost::function<void(std::string, std::string, std::string)> postResponceCallback)
{
    // Only supported method (at the moment?)
    if ( "GET" != method )
    {
        return false;
    }
    // First check if we have a statement to process for this request.
    std::string filename;
    // This should be long enough
    filename.reserve(512);

    filename += docroot;
    filename += boost::filesystem::path::preferred_separator;

    // To get here we must have got the host header
    std::string host = httpheaders["host"];
    filename += host;
    filename += boost::filesystem::path::preferred_separator;

    filename += "sqlite";
    filename += boost::filesystem::path::preferred_separator;
    std::string hostroot = filename;

    filename += "files";
    filename += boost::filesystem::path::preferred_separator;
    filename += webfilename;

    if (is_running_as_debug) std::cout << "Checking for sqlite file " << filename << std::endl;

    try{
        if ( !boost::filesystem::is_regular_file(filename) )
        {
            if ( !boost::filesystem::is_directory(filename) )
            {
                return false;
            }
            else
            {
                filename += boost::filesystem::path::preferred_separator;
                filename += "index";

                if ( !boost::filesystem::is_regular_file(filename) )
                {
                    // nothing to see here!
                    return false;
                }
            }
        }
    }
    catch (boost::filesystem::filesystem_error)
    {
        return false;
    }

    // Second check we have not created too much of a backlog
    if ( backlog >= WORKERBACKLOGSIZE )
    {
        // Post a 500 error of some form...
        return true;
    }

    // Now post it to the workers.
    int currentindex = nextavailable % WORKERBACKLOGSIZE ;
    nextavailable++;
    backlog++;

    auto it = requests.begin() + currentindex;
    it->ref = ref;
    it->filename = filename;
    it->host = host;
    it->hostroot = hostroot;
    it->postResponceCallback = postResponceCallback;
    it->http_headers = httpheaders;
    it->params = queryparams;

    workqueue.push(currentindex);
    sign.post();

    return true;
}
