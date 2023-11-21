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
#include <stdexcept>
#include "Util/logger.h"
#include "Record/MultiMediaSourceTuple.h"
#include "Common/config.h"
#include "Util/mini.h"

using namespace toolkit;
namespace mediakit {

class FileScanner{
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

    /**
     * 获取播放文件列表
     * dir_path: 检索的路径
     * start_time: 开始时间
     * end_time: 结束时间
    */
    std::vector<MultiMediaSourceTuple> getPlayFileList(const std::string dir_path, const std::string start_time, const std::string end_time){
        DebugL << "检索的文件路径："<< dir_path << ", 开始时间: " << start_time << ", 结束时间: " << end_time;
        uint64_t bOffset = 0;
        uint64_t eOffset = 0;
        std::vector<MultiMediaSourceTuple> vec = {};
        std::vector<std::string> files = getPlayFiles(dir_path, start_time, end_time, bOffset, eOffset);
        if(!files.empty()){
            for(auto item=files.begin();  item!=files.end(); item++){
                MultiMediaSourceTuple mst;
                if(files.size() == 1){
                    mst.startMs = bOffset;
                    mst.endMs = eOffset;
                }else{
                    if(item==files.begin()){
                        mst.startMs = bOffset;
                        mst.endMs = 0;
                    }else if(item==--files.end()){
                        mst.startMs = 0;
                        mst.endMs = eOffset;
                    }else{
                        mst.startMs = 0;
                        mst.endMs = 0;
                    }
                }
                mst.path = *item;
                vec.push_back(mst);
            }
            DebugL << "获取的第一个文件: " <<  files[0] <<", 起始偏移: "<< bOffset;
            DebugL <<" 获取的最后一个文件："<<files[files.size() - 1] << ", 末尾偏移: " << eOffset;
        }
        return vec;
    }

private:

    /**
     * 创建文件映射表
     * 参数 folder_path: 目录路径
    */
    bool createFileMap(std::string folder_path){
        bool ret = false;
        for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
            if (entry.is_directory()) {
                std::vector<std::string> files;
                for (const auto& file : std::filesystem::directory_iterator(entry.path())) {
                    if (file.is_regular_file()) {
                        files.push_back(file.path().filename().string());
                    }
                }
                std::sort(files.begin(), files.end());
                folder_map[entry.path().filename().string()] = files;
                ret = true;
            }
        }

        // for (auto& folder_entry : folder_map)
        // {
        //     std::cout << "Folder: " << folder_entry.first << std::endl;
        //     for (auto& file_entry : folder_entry.second)
        //     {
        //         std::cout << "File: " << file_entry << std::endl;
        //     }
        // }
        return ret;
    }

    /**
     * 找出第一个文件
     * 参数 start_time：开始时间
    */
    std::vector<std::string>  getFirstFile(std::string folder_path, const std::string start_time/* YY-MM-DD hh:mm:ss */, const std::string end_time/* YY-MM-DD hh:mm:ss */, uint64_t& offset) 
    {
        std::vector<std::string> start_time_ans = split(start_time, " +");
        std::vector<std::string> end_time_ans = split(end_time, " +");
        playStartDate = start_time_ans[0];
        std::string hhmmss = start_time_ans[1];
        iPlayStartTime = string2second(hhmmss);
        hhmmss = end_time_ans[1];
        iPlayEndTime = string2second(hhmmss);

        for(auto item=folder_map.begin(); item!=folder_map.end(); item++){
            if(playStartDate==item->first){
                for(auto file_itr = item->second.begin(); file_itr!=item->second.end(); file_itr++){
                    // if((*file_itr)[0] == '.'){
                    //     continue;
                    // }
                    if(isHidden(*file_itr)) continue;
                    std::vector<std::string> infos = split(*file_itr, "[-.]+");
                    std::string fileStartTime = infos[0];
                    std::string fileEndTime = infos[1];

                    int iFileStartTime = string2second2(fileStartTime);
                    int iFileEndTime = string2second2(fileEndTime);
                    if(iFileStartTime < 0 || iFileEndTime < 0)
                        return {};


                    if(iFileEndTime<iFileStartTime){
                        // 说明此文件是跨天的，结束时间为第二天，开始时间为当前
                        iFileEndTime += 24*60*60; // 将0点转换为24点进行计算
                    }

                    if((iPlayStartTime >=iFileStartTime && iPlayStartTime < iFileEndTime )){
                        // 是要播放的第一个文件
                        if(iPlayStartTime > iFileStartTime){
                            offset = iPlayStartTime - iFileStartTime;  // 应该播放到此文件的这个位置
                        }

                        std::string file_path = folder_path+"/"+item->first+"/"+ *file_itr;
                        DebugL << "匹配的文件 1: " << file_path;
                        playFiles.push_back(file_path);
                        return playFiles;
                    } else if(iPlayStartTime < iFileStartTime){
                        if(iPlayStartTime > iFileStartTime){
                            offset = iPlayStartTime - iFileStartTime;  // 应该播放到此文件的这个位置
                        }

                        std::string file_path = folder_path+"/"+item->first+"/"+ *file_itr;
                        DebugL << "匹配的文件 2: " << file_path;
                        playFiles.push_back(file_path);
                        return playFiles;
                    }
                }
            }
        }
        return playFiles;
    }
    /**
     * 获取下一个文件
     * 参数end_time: 结束时间，每次调用，都会判断是否到达结束时间
    */
    std::vector<std::string> fillFile(std::string folder_path, const std::string end_time, uint64_t& offset) 
    {
        std::vector<std::string> end_time_ans = split(end_time, " +");
        std::string playStopDate = end_time_ans[0];

        std::string hhmmss = end_time_ans[1];
        int iPlayStopTime = string2second(hhmmss);
        std::string MP4MaxSecond = mINI::Instance()[mediakit::Protocol::kMP4MaxSecond];
        int iPlayStopTimeEndFile = iPlayStopTime + atoi(MP4MaxSecond.c_str());

        //todo 检查 起始和结束时间的正确性

        for(auto item=folder_map.begin(); item!=folder_map.end(); item++){


            //当天结束的
             if(playStopDate==item->first  && playStartDate == item->first){
                //请求只有当天
                for(auto file_itr = item->second.begin(); file_itr!=item->second.end(); file_itr++){
                    // if((*file_itr)[0] == '.'){
                    //     continue;
                    // }
                    if(isHidden(*file_itr)) continue;
                    std::vector<std::string> infos = split(*file_itr, "[-.]+");
                    std::string fileStartTime = infos[0];
                    std::string fileEndTime = infos[1];

                    int iFileStartTime = string2second2(fileStartTime);
                    int iFileEndTime = string2second2(fileEndTime);
                    if(iFileStartTime < 0 || iFileEndTime < 0)
                        return {};

                    if(iFileEndTime<iFileStartTime){
                        // 说明此文件是跨天的，结束时间为第二天，开始时间为当前
                        iFileEndTime += 24*60*60; // 将0点转换为24点进行计算
                    }

                    //文件开始时间 > 点播开始时间 && 文件结束的时间 < 点播结束的时间
                    if(iPlayStartTime < iFileStartTime &&  iFileEndTime <= iPlayStopTimeEndFile){

                        // 刷选到的文件是 起始
                        std::string file_path = folder_path+"/"+item->first+"/"+ *file_itr;
//                        DebugL << "匹配的文件 3: " << file_path;
                        if(playFiles.size() == 1 && file_path == playFiles[0] ){
                            //去重
                            DebugL << "匹配的文件 3 去重 : " << file_path ;
                            //如果只有一个文件 就设置了 偏移 如果多个文件，偏移还会刷新
                            offset = iPlayStopTime - iFileStartTime;
                            continue;
                        }
                        playFiles.push_back(file_path);
                        if(iPlayStopTime > iFileStartTime && iPlayStopTime <= iFileEndTime){
                            //播放的时间在文件内部
                            offset = iPlayStopTime - iFileStartTime;    // 应该播放到此文件的这个位置
                        } else if(iPlayStopTime > iFileEndTime){
                            //播放的时间在文件外部
                            offset = 0;
                        }
                    } else if(iPlayStopTime > iFileStartTime && iPlayStopTime <= iFileEndTime) {
//                        DebugL << "匹配的文件 3 .1 计算末尾文件时间偏移: "  <<iFileEndTime ;
                        //点播时间在文件里面
                        offset =  (iFileEndTime - iFileStartTime) - (iFileEndTime - iPlayStopTime);
                    }
                }
            } else{
                //跨天 时间都不在的 文件夹
                //如 点播2023-11-12~2023-11-15   则2023-11-12之前的 和 2023-11-15之后的都不处理
                if(item->first < playStartDate || item->first > playStopDate){
                    continue;
                }

                //跨天 点播开始那天
                //如 点播2023-11-12~2023-11-15   此处处理2023-11-12
                if(item->first  == playStartDate ){
                    for(auto file_itr = item->second.begin(); file_itr!=item->second.end(); file_itr++){
                        if(isHidden(*file_itr)) continue;
                        std::vector<std::string> infos = split(*file_itr, "[-.]+");
                        std::string fileStartTime = infos[0];
                        std::string fileEndTime = infos[1];

                        int iFileStartTime = string2second2(fileStartTime);

                        //文件开始时间 > 点播开始时间 && 文件结束的时间 < 点播结束的时间
                        if(iPlayStartTime < iFileStartTime ){
                            // 刷选到的文件是 起始
                            std::string file_path = folder_path+"/"+item->first+"/"+ *file_itr;
//                            DebugL << "匹配的文件 3: " << file_path;
                            if(playFiles.size() == 1 && file_path == playFiles[0] ){
                                //去重
                                DebugL << "匹配的文件 3 去重 : " << file_path ;
                                continue;
                            }
                            playFiles.push_back(file_path);
                        }
                    }
                    continue;
                }
                //跨天 点播结束那天
                //如 点播2023-11-12~2023-11-15   此处处理2023-11-15
                if(item->first  == playStopDate ){

                    for(auto file_itr = item->second.begin(); file_itr!=item->second.end(); file_itr++){

                        if(isHidden(*file_itr)) continue;
                        std::vector<std::string> infos = split(*file_itr, "[-.]+");
                        std::string fileStartTime = infos[0];
                        std::string fileEndTime = infos[1];

                        int iFileStartTime = string2second2(fileStartTime);
                        int iFileEndTime = string2second2(fileEndTime);

                        if(iFileEndTime<iFileStartTime){
                            // 说明此文件是跨天的，结束时间为第二天，开始时间为当前
                            iFileEndTime += 24*60*60; // 将0点转换为24点进行计算
                        }

//                        DebugL << "匹配的文件 4.0: "  <<item->first << "   "<< iFileEndTime <<","<<iPlayStopTimeEndFile ;
                        //文件开始时间 > 点播开始时间 && 文件结束的时间 < 点播结束的时间
                        std::string file_path = folder_path+"/"+item->first+"/"+ *file_itr;
                        if(  iFileStartTime < iPlayStopTimeEndFile && iFileEndTime <iPlayStopTimeEndFile ) {
                            //                            DebugL << "匹配的文件 4: " << file_path;
                            playFiles.push_back(file_path);

                            if (iPlayStopTime > iFileEndTime){
                                    offset = 0;
                            }else{
                                offset = iPlayStopTime - iFileStartTime;
                            }
                        }

                    }
                    break;
                 }

                 //跨天 点播中间的
                 //如 点播2023-11-12~2023-11-15   此处处理2023-11-13  2023-11-14的
                for(auto file_itr = item->second.begin(); file_itr!=item->second.end(); file_itr++){
                    // if((*file_itr)[0] == '.'){
                    //     continue;
                    // }
                    if(isHidden(*file_itr)) continue;
                    std::string file_path = folder_path+"/"+item->first+"/"+ *file_itr;
//                    DebugL << "匹配的文件 2: " << file_path;
                    playFiles.push_back(file_path);
                }

            }
        }
        return playFiles;
    }

    /**
     * 获取时间段指定的文件列表
     * 参数 start_time: 开始时间
     * 参数 end_time: 结束时间
    */
    std::vector<std::string> getPlayFiles(std::string folder_path, const std::string start_time, const std::string end_time, uint64_t& bOffset, uint64_t& eOffset) {
        std::vector<std::string> files;
        std::string dateDirName;
        if(createFileMap(folder_path)){
            files = getFirstFile(folder_path, start_time, end_time, bOffset);
            if(!files.empty()) {
                files = fillFile(folder_path, end_time, eOffset);
            }
        }

        return files;
    }

    int string2second(std::string time_str)
    {
        int hours, minutes, seconds, total_seconds;
        std::stringstream ss(time_str);
        char delimiter;
        ss >> hours >> delimiter >> minutes >> delimiter >> seconds;
        total_seconds = hours * 3600 + minutes * 60 + seconds;
        return total_seconds;
    } 

     int safe_stoi(const std::string& str) {
        // DebugL<<"input time = "<<str;
        try {
            size_t pos;
            int value = std::stoi(str, &pos);
            if (pos < str.size()) {
                throw std::invalid_argument("Invalid argument: " + str);
            }
            return value;
        } catch (const std::out_of_range&) {
            throw std::out_of_range("Out of range: " + str);
        } catch (const std::invalid_argument&) {
            throw std::invalid_argument("Invalid argument: " + str);
        }
    }

    int string2second2(std::string time) 
    {
        int h = 0;
        int m = 0;
        int s = 0;
        std::regex pattern("[0-9]+");
        if (!std::regex_match(time, pattern))
        {
            DebugL<<"input time error. time = "<<time;
            return -1;
        }

        if(!time.empty()){
            h = safe_stoi(time.substr(0,2));
            m = safe_stoi(time.substr(2,2));
            s = safe_stoi(time.substr(4,2));
        }
        return (h) * 3600 + (m)*60 + (s);
    }

    bool isHidden(std::string filename){
        if(filename.at(0) == '.') return true;
        return false;
    }


protected:
    std::vector<std::string> split(const std::string& input, const std::string& regex);
    void initInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info, bool  isStart);
    bool initFileInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info,std::shared_ptr<Info> st,std::shared_ptr<Info> et);
    int calcShift(std::string time, int hour, int minute, int second);
    void genNameVec(std::string full_path, std::shared_ptr<Info>& fn, std::vector<std::shared_ptr<Info>>& myfiles);
    std::vector<std::string> getAllNediaInfo(std::string dir_path, const std::string start_time,const std::string end_time);
    void getInfo(const std::string start_time, const std::string end_time);
    std::vector<std::shared_ptr<FileScanner::Info>> getMediaInfo(std::string dir_path, const std::string start_time, const std::string end_time);
private:
    std::map<std::string/*YYMMDD*/, std::vector<std::string/*hhmmss-hhmmss.mp4*/>> folder_map;
    std::vector<std::string> playFiles;
    std::string playStartDate;
    int iPlayStartTime;
    int iPlayEndTime;
};

}

#endif /* Utils_hpp */
