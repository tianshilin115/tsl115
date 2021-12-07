//
// Created by root on 2020/7/2.
//

#ifndef CAPTURE_HANDLEOFFLINE_H
#define CAPTURE_HANDLEOFFLINE_H

#include <curl/curl.h>
#include <list>
#include <string>
#include "cjson/cJSON.h"
#include "xdpi_conf.h"
#include "rte_log.h"

#define WAIT_FOR_ANALYSIS_PACPFILE_INDEX "wait_for_analysis_pcapfile"
#define SERVER_MEDIUM_MSG_INDEX "server_medium_msg"
#define NET_EQUIPMENT_INDEX "net_equipment_msg"
#define NOT_ANALYSIS 0
#define STAY_ANALYSIS 1
#define ANALYSISING 2
#define ANALYSISED 3

using namespace std;
struct CurlRequest{
    char *data;
    CURL *curl;
    size_t datalength;
    size_t used;
    CurlRequest(CURL *ptr):data(nullptr),curl(ptr),datalength(0),used(0){}
};
struct OfflineTask{
    int handledFilesNum;
    int allFileNum;
    string taskId;
    list<string> files;
    int isListening;
};

class HandleOffline {
public:
    HandleOffline();
    ~HandleOffline();
    CURL *curlSearch = nullptr;
    char buf[200]{};
    char RequestURL[100]{};
    
    list<OfflineTask> getOfflineTask(const string& taskIndex,char *condition);
    void updateHandleStatus(const string& taskIndex, int handledFiles, int totalFiles, const string& id);
private:
    void handlePcapDirs(cJSON *pcapDirs, list<string> *pcapFiles);
    void initElasticsearch();
    void setSearchCurl(CurlRequest *curlRequest);
    void handlePcapDir(string filePath, list<string> *pcapFiles);
    void handlePcapFiles(cJSON *files, list<string> *pcapFiles);

};


#endif //CAPTURE_HANDLEOFFLINE_H
