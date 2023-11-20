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
                DebugL << "获取的文件: " <<  mst.path << ", 起始偏移: " << mst.startMs << ", 末尾偏移: " << mst.endMs;
            }
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
    std::vector<std::string>  getFirstFile(std::string folder_path, const std::string start_time/* YY-MM-DD hh:mm:ss */, uint64_t& offset) 
    {
        std::vector<std::string> start_time_ans = split(start_time, " +");
        playStartDate = start_time_ans[0];
        std::string hhmmss = start_time_ans[1];
        iPlayStartTime = string2second(hhmmss);

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


                    if(iFileEndTime<iFileStartTime){
                        // 说明此文件是跨天的，结束时间为第二天，开始时间为当前
                        iFileEndTime += 24*60*60; // 将0点转换为24点进行计算
                    }

                    if((iPlayStartTime >=iFileStartTime && iPlayStartTime < iFileEndTime ) || (iPlayStartTime <= iFileStartTime)){
                        // 是要播放的第一个文件
                        offset = iPlayStartTime - iFileStartTime;    // 应该播放到此文件的这个位置
                        std::string file_path = folder_path+"/"+item->first+"/"+ *file_itr;
                        DebugL << "匹配的文件 1: " << file_path;
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

        for(auto item=folder_map.begin(); item!=folder_map.end(); item++){
            if(playStopDate > item->first && playStartDate < item->first ){
                for(auto file_itr = item->second.begin(); file_itr!=item->second.end(); file_itr++){
                    // if((*file_itr)[0] == '.'){
                    //     continue;
                    // }
                    if(isHidden(*file_itr)) continue;
                    std::string file_path = folder_path+"/"+item->first+"/"+ *file_itr;
                    DebugL << "匹配的文件 2: " << file_path;
                    playFiles.push_back(file_path);
                }
            }else if(playStopDate==item->first || playStartDate == item->first){
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

                    if(iFileEndTime<iFileStartTime){
                        // 说明此文件是跨天的，结束时间为第二天，开始时间为当前
                        iFileEndTime += 24*60*60; // 将0点转换为24点进行计算
                    }

                    if(iPlayStartTime < iFileStartTime && iPlayStopTime <= iFileEndTime){
                        // 是要播放文件
                        std::string file_path = folder_path+"/"+item->first+"/"+ *file_itr;
                        DebugL << "匹配的文件 3: " << file_path;
                        playFiles.push_back(file_path);
                        if(iPlayStopTime > iFileStartTime && iPlayStopTime <= iFileEndTime){
                            offset = iPlayStopTime - iFileStartTime;    // 应该播放到此文件的这个位置
                        }
                    }
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
            files = getFirstFile(folder_path, start_time, bOffset);
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

    int string2second2(std::string time) 
    {
        int h = 0;
        int m = 0;
        int s = 0;
        if(!time.empty()){
            h = stoi(time.substr(0,2));
            m = stoi(time.substr(2,2));
            s = stoi(time.substr(4,2));
        }
        return (h) * 3600 + (m)*60 + (s);
    }

    bool isHidden(std::string filename){
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        std::streampos size = file.tellg();
        std::filesystem::path filePath(filename);
        bool isHidden = ((filePath.filename().string()[0] == '.') || (filePath.filename().string()[0] == '\\'));

        return isHidden;
    }


protected:
    std::vector<std::string> split(const std::string& input, const std::string& regex);
    void initInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info, bool  isStart);
    bool initFileInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info,std::shared_ptr<Info> st,std::shared_ptr<Info> et);
    int calcShift(std::string time, int hour, int minute, int second);
    void genNameVec(std::string full_path, std::shared_ptr<Info>& fn, std::vector<std::shared_ptr<Info>>& myfiles);
    std::vector<std::string> getAllNediaInfo(std::string dir_path, const std::string start_time,const std::string end_time);
    void getInfo(const std::string start_time, const std::string end_time);
    std::vector<std::shared_ptr<Scanner::Info>> getMediaInfo(std::string dir_path, const std::string start_time, const std::string end_time);
private:
    std::map<std::string/*YYMMDD*/, std::vector<std::string/*hhmmss-hhmmss.mp4*/>> folder_map;
    std::vector<std::string> playFiles;
    std::string playStartDate;
    int iPlayStartTime;
};

}

#endif /* Utils_hpp */
