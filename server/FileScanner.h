//
//  FileScanner.h
//
//  Created by weidian on 2023/10/30.
//

#ifndef ZLMEDIAKIT_UTILS_H
#define ZLMEDIAKIT_UTILS_H

#include <dirent.h>
#include <string>
#include <iostream>
#include <vector>
#include <regex>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include "Util/logger.h"
#include "Record/MultiMediaSourceTuple.h"

namespace fs = std::__fs::filesystem;

namespace mediakit {

class Scanner{
    
public:
    typedef struct Info{
        std::string file_name;
        std::string stime;
        std::string etime;
        uint32_t start_shift;
        uint32_t end_shift;
        int hh;
        int mm;
        int ss;
    }Info;
    

    static std::vector<std::string> split(const std::string& input, const std::string& regex);
    static void initInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info, bool  isStart);
    static bool initFileInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info,std::shared_ptr<Info> st,std::shared_ptr<Info> et);
    static bool time_compare_st(std::shared_ptr<Info> first, std::shared_ptr<Info> second);
    static std::vector<std::shared_ptr<Scanner::Info>> getMediaInfo(std::string dir_path, const std::string start_time, const std::string end_time);
    static std::vector<MultiMediaSourceTuple> getMST(std::string dir_path, const std::string start_time, const std::string end_time);
    static int calcShift(std::string time, int hour, int minute, int second);
    static void genNameVec(std::string full_path, std::shared_ptr<Info>& fn, std::vector<std::shared_ptr<Info>>& myfiles);
    static std::vector<std::string> getAllNediaInfo(std::string dir_path, const std::string start_time,const std::string end_time);
    static std::map<std::string, std::vector<std::string>> createFileForm(std::string folder_path);
    static void getInfo(std::string folder_path,const std::string start_time, const std::string end_time);
};

//todo 先放这里，后续再看看如何处理这一块的业务代码如何分包合适
class IPAddress {
public:
    static std::string getIP(const std::string &url);
    static bool isIPReachable(const std::string &ip);
};
}

#endif /* Utils_hpp */
