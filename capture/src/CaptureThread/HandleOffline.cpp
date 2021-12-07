//
// Created by root on 2020/7/2.
//

#include <cstring>
#include <iostream>
#include <dirent.h>
#include "HandleOffline.h"

int getCJSONInt(cJSON *intCJSON){
    return intCJSON ? intCJSON->valueint:0;
}
/**
 * 获取返回结果
 * @param contents
 * @param size
 * @param nmemb
 * @param requestP
 * @return
 */
size_t getList(void *contents, size_t size, size_t nmemb, void *requestP) {
    auto *ptr = static_cast<CurlRequest *>(requestP);
    size_t currentDataSize = size * nmemb;

    if (!ptr->data) {
        double dataSize;
        curl_easy_getinfo(ptr->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &dataSize);
        ptr->used = currentDataSize;
        ptr->datalength = ((currentDataSize > dataSize) ? currentDataSize : dataSize);
        ptr->data = (char *) malloc(ptr->datalength + 1);
        memcpy(ptr->data, contents, dataSize);
        return currentDataSize;
    }
    if (ptr->used + currentDataSize >= ptr->datalength) {
        ptr->datalength += ptr->used + currentDataSize;
        ptr->data = (char *) realloc(ptr->data, ptr->datalength + 1);
    }
    memcpy(ptr->data + ptr->used, contents, currentDataSize);
    ptr->used += currentDataSize;
    return currentDataSize;
}

HandleOffline::HandleOffline() {
    initElasticsearch();
}

/**
 * 初始化elasticsearch
 * @param host
 */
void HandleOffline::initElasticsearch() {
    curlSearch = curl_easy_init();
    struct curl_slist *headerList = nullptr;
    headerList = curl_slist_append(headerList, "Content-Type: application/json");
    curl_easy_setopt(curlSearch, CURLOPT_HTTPHEADER, headerList);
}

/**
 *  色和之查询的CURL
 * @param curl
 * @param size
 * @param from
 */
void HandleOffline::setSearchCurl(CurlRequest *curlRequest) {
    curl_easy_setopt(curlSearch, CURLOPT_WRITEFUNCTION, getList);
    curl_easy_setopt(curlSearch, CURLOPT_URL, RequestURL);
    curl_easy_setopt(curlSearch, CURLOPT_POST, 1);
    curl_easy_setopt(curlSearch, CURLOPT_POSTFIELDSIZE, strlen(buf));
    curl_easy_setopt(curlSearch, CURLOPT_POSTFIELDS, buf);
    curl_easy_setopt(curlSearch, CURLOPT_WRITEDATA, (void *) curlRequest);
}

/**
 * 获取离线任务
 * @param taskIndex
 * @param condition
 * @return
 */
list<OfflineTask> HandleOffline::getOfflineTask(const string& taskIndex, char *condition) {
    sprintf(RequestURL, "%s/%s/%s/%s",GlobalConfig.dbCfg.esPath.c_str(), taskIndex.c_str(), "_doc", "_search");
    int searchSize = 100;
    int totalFilesTaskNum = 0;
    int handledFileTask = 0;
    list<OfflineTask> tasks;
    do {
        CurlRequest curlRequest(curlSearch);
        sprintf(buf, R"({"query":{"bool":%s},"size":%d,"from":%d})", condition,searchSize, handledFileTask);
        setSearchCurl(&curlRequest);
        if (CURLE_OK != curl_easy_perform(curlSearch)) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "查询文件任务错误");
            return tasks;
        }
        cJSON *root = cJSON_Parse(curlRequest.data);
        if (cJSON_GetObjectItem(root, "error")) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "查询错误 - %s",curlRequest.data);
            return tasks;
        }
        totalFilesTaskNum = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(root, "hits"), "total"),
                                                "value")->valueint;
        cJSON *hitsArry = cJSON_GetObjectItem(cJSON_GetObjectItem(root, "hits"), "hits");

        for (int idx = 0; idx < cJSON_GetArraySize(hitsArry); idx++) {
            //upload file
            handledFileTask++;
            OfflineTask offlinePcapFile{};
            offlinePcapFile.handledFilesNum = 0;
            offlinePcapFile.allFileNum = 0;
            if (taskIndex == WAIT_FOR_ANALYSIS_PACPFILE_INDEX) {
                offlinePcapFile.taskId = cJSON_GetObjectItem(cJSON_GetArrayItem(hitsArry, idx), "_id")->valuestring;
                handlePcapFiles(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetArrayItem(hitsArry, idx), "_source"),
                                                    "analysisFile"),&offlinePcapFile.files);
                //equioment  network
            } else if (taskIndex == SERVER_MEDIUM_MSG_INDEX || taskIndex == NET_EQUIPMENT_INDEX) {
                offlinePcapFile.taskId = cJSON_GetObjectItem(cJSON_GetArrayItem(hitsArry, idx), "_id")->valuestring;
                cJSON * paths =cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetArrayItem(hitsArry, idx), "_source"),"analysisPath");
                if(paths == nullptr)
                    continue;
                handlePcapDirs(paths,&offlinePcapFile.files);
                offlinePcapFile.isListening = getCJSONInt(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetArrayItem(hitsArry, idx), "_source"), "listenState"));
                offlinePcapFile.handledFilesNum = getCJSONInt(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetArrayItem(hitsArry, idx), "_source"), "handledFilesNum"));
            }

            tasks.push_back(offlinePcapFile);
        }
    } while (handledFileTask < totalFilesTaskNum);
    return tasks;
}

/**
 * 处理文件加数组
 * @param pcapDirs
 * @param pcapFiles
 */
void HandleOffline::handlePcapDirs(cJSON *pcapDirs, list<string> *pcapFiles){
    for (int idx = 0;idx <  cJSON_GetArraySize(pcapDirs); ++idx) {
        cJSON * path =cJSON_GetArrayItem(pcapDirs, idx);
        if(!path || path->valuestring == nullptr)
            continue;
        handlePcapDir(path->valuestring,pcapFiles);
    }
}

/**
 * 处理文件夹 从文件夹中遍历该文件夹中所有文件并没有判断文件是否为pcap文件
 * @param filePath
 * @param pcapFiles
 */
void HandleOffline::handlePcapDir(string filePath, list<string> *pcapFiles) {
    DIR *fileDir;
    struct dirent *ptr;
    if (!(fileDir = opendir(filePath.c_str())))
        return;
    while ((ptr = readdir(fileDir)) != nullptr) {
        if (!ptr->d_type) {
            continue;
        } else if (strcmp(".", ptr->d_name) == 0 || strcmp("..", ptr->d_name) == 0) {
            continue;
        } else if (ptr->d_type == DT_DIR) {
            handlePcapDir(filePath + "/" + ptr->d_name, pcapFiles);
        } else {
            pcapFiles->push_back(filePath + "/" + ptr->d_name);
        }
    }
    closedir(fileDir);
}

/**
 * 处理数据包文件 即从json文件中，读取文件到相应的list中
 * @param files
 * @param pcapFiles
 */
void HandleOffline::handlePcapFiles(cJSON *files, list<string> *pcapFiles) {
    for (int idx = 0; idx < cJSON_GetArraySize(files); idx++) {
        pcapFiles->emplace_back(cJSON_GetArrayItem(files, idx)->valuestring);
    }
}

/**
 * 更新文件处理状态   读取了系统dbcfg中es路径地址，从此处可以看出：处理文件数量、总的文件数量以及分析状态（正在分析中或者分析完成）
 * @param taskIndex
 * @param handledFiles
 * @param totalFiles
 * @param id
 */
void HandleOffline::updateHandleStatus(const string& taskIndex, int handledFiles, int totalFiles, const string& id) {
    sprintf(RequestURL, "%s/%s/%s/%s", GlobalConfig.dbCfg.esPath.c_str(), taskIndex.c_str(), "_update", id.c_str());
    sprintf(buf,R"({"doc":{"handledFilesNum":%d,"totalFilesNum":%d, "status":%d}})", handledFiles,totalFiles,handledFiles==totalFiles?ANALYSISED:ANALYSISING);
    CurlRequest curlRequest(curlSearch);
    setSearchCurl(&curlRequest);
    if (CURLE_OK != curl_easy_perform(curlSearch))
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "更新文件处理状态失败");
}