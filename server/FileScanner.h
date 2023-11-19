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
    
    std::vector<std::string> split(const std::string& input, const std::string& regex);
    void initInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info, bool  isStart);
    bool initFileInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info,std::shared_ptr<Info> st,std::shared_ptr<Info> et);
    static std::vector<MultiMediaSourceTuple> getMST(const std::string start_time, const std::string end_time);
    int calcShift(std::string time, int hour, int minute, int second);
    void genNameVec(std::string full_path, std::shared_ptr<Info>& fn, std::vector<std::shared_ptr<Info>>& myfiles);
    std::vector<std::string> getAllNediaInfo(std::string dir_path, const std::string start_time,const std::string end_time);
    
    /**
     * 创建文件映射表
     * 参数 folder_path: 目录路径
    */
    bool createFileForm(std::string folder_path);

    /**
     * 找出第一个文件
     * 参数 start_time：开始时间
    */
    std::string getFirstFile(const std::string start_time/* YY-MM-DD hh:mm:ss */, uint64_t& offset) {

        std::vector<std::string> start_time_ans = split(start_time, " +");
        std::string playStartDate = start_time_ans[0];

        std::string hhmmss = start_time_ans[1];
        std::vector<std::string> timeInfos = split(hhmmss, std::string(":"));
        std::string playStartTime = timeInfos[0]+timeInfos[1]+timeInfos[2];
        int iPlayStartTime = stoi(playStartTime);

        for(auto item=folder_map.begin(); item!=folder_map.end(); item++){
            if(playStartDate==item->first){
                for(auto file_itr = item->second.begin(); file_itr!=item->second.end(); file_itr++){
                    
                    std::vector<std::string> infos = split(*file_itr, "[-.]+");
                    std::string fileStartTime = infos[0];
                    std::string fileEndTime = infos[1];
                    
                    int iFileStartTime = stoi(fileStartTime);
                    int iFileEndTime = stoi(fileEndTime);

                    int interval = 0;
                    if(iFileEndTime<iFileStartTime){
                        // 说明此文件是跨天的，结束时间为第二天，开始时间为当前
                        iFileEndTime += 240000; // 将0点转换为24点进行计算
                    }
                    interval = iFileEndTime - iFileStartTime;

                    if(iPlayStartTime >=iFileStartTime && iPlayStartTime < iFileEndTime){
                        // 是要播放的第一个文件
                        offset = iPlayStartTime - iFileStartTime;    // 应该播放到此文件的这个位置
                        return *file_itr;
                    }
                }
            }
        }
    }

    /**
     * 获取下一个文件
     * 参数end_time: 结束时间，每次调用，都会判断是否到达结束时间
    */
    std::string getNextFile(const std::string end_time, uint64_t& offset) {
        std::vector<std::string> end_time_ans = split(end_time, " +");
        std::string playStopDate = end_time_ans[0];

        std::string hhmmss = end_time_ans[1];
        std::vector<std::string> timeInfos = split(hhmmss, std::string(":"));
        std::string playStopTime = timeInfos[0]+timeInfos[1]+timeInfos[2];
        int iPlayStopTime = stoi(playStopTime);

        for(auto item=folder_map.begin(); item!=folder_map.end(); item++){
            if(playStopDate==item->first){
                for(auto file_itr = item->second.begin(); file_itr!=item->second.end(); file_itr++){
                    
                    std::vector<std::string> infos = split(*file_itr, "[-.]+");
                    std::string fileStartTime = infos[0];
                    std::string fileEndTime = infos[1];
                    
                    int iFileStartTime = stoi(fileStartTime);
                    int iFileEndTime = stoi(fileEndTime);

                    int interval = 0;
                    if(iFileEndTime<iFileStartTime){
                        // 说明此文件是跨天的，结束时间为第二天，开始时间为当前
                        iFileEndTime += 240000; // 将0点转换为24点进行计算
                    }
                    interval = iFileEndTime - iFileStartTime;

                    if(iPlayStopTime > iFileEndTime){
                        // 是要播放文件
                        return *file_itr;
                    }else if(iPlayStopTime > iFileStartTime && iPlayStopTime <= iFileEndTime){
                        offset = iPlayStopTime - iFileStartTime;    // 应该播放到此文件的这个位置
                        return *file_itr;
                    }
                }
            }
        }
    }

    /**
     * 获取时间段指定的文件列表
     * 参数 start_time: 开始时间
     * 参数 end_time: 结束时间
    */
    std::vector<std::string> getPlayFiles(std::string folder_path, const std::string start_time, const std::string end_time, uint64_t& bOffset, uint64_t& eOffset) {
        std::vector<std::string> files;
        
        if(createFileForm(folder_path)){
            std::string first = getFirstFile(start_time, bOffset);
            if(!first.empty()) {
                files.push_back(first);
                uint64_t end_offst = 0;
                while(end_offst !=0 ) {
                    std::string file = getNextFile(end_time, eOffset);
                    if(file.empty()) break;
                    files.push_back(file);
                }
            }
        }
        return files;
    }



    void getInfo(const std::string start_time, const std::string end_time);
    std::vector<std::shared_ptr<Scanner::Info>> getMediaInfo(std::string dir_path, const std::string start_time, const std::string end_time);
private:
    std::map<std::string, std::vector<std::string>> folder_map;
};

//todo 先放这里，后续再看看如何处理这一块的业务代码如何分包合适
class IPAddress {
public:
    static std::string getIP(const std::string &url);
    static bool isIPReachable(const std::string &ip);
};
}

#endif /* Utils_hpp */
