#ifndef RECORD_DIR_MANAGER_H
#define RECORD_DIR_MANAGER_H

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <regex>
#include <sys/statvfs.h>


#include "Poller/EventPoller.h"
#include "Poller/Timer.h"

#define DISK_VIDEO_RECORD_THRESHOLD_PERCENTAGE  0.89f //默认固定开始清理的阈值，当硬盘容量在大于85%时启动清除
class DiskSpaceManager
{
public:
    DiskSpaceManager();
    ~DiskSpaceManager();

    typedef enum {
        DAYS_CTRL = 1,
        THRESHOLD_CTRL = 2,
    }CONTROL_MODE_E;

    class DiskInfo
    {
    public:
        DiskInfo(std::string path){
            //获取路径上的容量
#if 0
            struct statvfs vfs ;
            if(statvfs(path.c_str(), &vfs) == -1){
                //查不到挂在的路径分区大小
                perror("statvfs");
                InfoL << "statvfs error :" << path <<std::endl;
                return;
            }
            _Capacity = (double)vfs.f_blocks * vfs.f_frsize / (1024 * 1024 * 1024) ;
            _Available = (double)vfs.f_bavail * vfs.f_frsize / (1024 * 1024 * 1024);
            _UsedDiskCap = (double)((vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize /( 1024 * 1024 * 1024));
            _UsageRate = _UsedDiskCap/_Capacity;
            InfoL<< "磁盘容量:" << _Capacity  << "  GB, 可用空间:" <<_Available << " GB, 已用空间:" << _UsedDiskCap <<" GB, 实际使用率" << _UsageRate*100 <<"%";
#else
            std::filesystem::path record_path(path);
            if (!std::filesystem::exists(record_path)) {
                WarnL << "Path [ " << path << " ] does not exist!";
                return;
            }

            //获取路径上的容量
            std::string cmd = "df -m " + path;
            char buffer[1024];
            FILE *fp = popen(cmd.c_str() , "r");
            if (fp == NULL) {
                ErrorL  <<  "Failed to run command " <<std::endl;
                return;
            }
            while (fgets(buffer, sizeof(buffer), fp) != NULL);
            pclose(fp);

            //从读取出来的数据中读取 容量 字符串
            std::stringstream used(buffer);
            std::vector<std::string> usedVec;
            std::string word;
            while (used >> word)usedVec.push_back(word);
            InfoL << usedVec[0]<< " " << usedVec[1]<< " " << usedVec[2]<< " "<<usedVec[3]<< " "<< usedVec[4]<< " "<<  std::endl;
            _Capacity = stod(usedVec[1])/1024;
            _UsedDiskCap = stod(usedVec[2])/1024;
            _Available = stod(usedVec[3])/1024;

            usedVec[4].erase(usedVec[4].find_last_not_of("%") + 1);  // 去掉百分号
            _UsageRate = stod(usedVec[4]) / 100.0;  // 转换成 double，除以100

            InfoL<< "磁盘容量:" << _Capacity  << "  GB, 可用空间:" <<_Available << " GB, 已用空间:" << _UsedDiskCap <<" GB, *使用率" << _UsageRate*100 <<"%";
#endif
        }
        ~DiskInfo(){};

        double _Capacity = 0.0 ;    // 总容量 GB  
        double _Available = 0.0;    // 可用容量 GB
        double _UsedDiskCap = 0.0;  // 已用容量 GB
        double _UsageRate = 0.0;    // 使用率
    };
    
    static std::shared_ptr<DiskSpaceManager> GetCreate();
    bool StartService(std::string recordPath, CONTROL_MODE_E ctrl_mode = THRESHOLD_CTRL);
    float GetThreshold() { return _thresholdMB; }
    float GetDeleteVideoThreshold() { return DISK_VIDEO_RECORD_THRESHOLD_PERCENTAGE; }
    double GetStorageSpace(std::string path);

private:
    int _removeEmptyDirectory(const std::string& path);
    void _removeHiddenFiles(const std::string& path);
    void _deleteOldestFile(const std::string& path);
    int _getFileNumInDirectory(std::string path);
    bool _OverNdays(const std::string dir_name, int nDays = 8);

private:
    static std::shared_ptr<DiskSpaceManager> _recordManager;
    toolkit::EventPoller::Ptr _poller;
    toolkit::Timer::Ptr _timer;
    float _thresholdMB = 0.0f;
    int _nDays = 8;
    std::vector<std::regex> fileDeleteRegexs {std::regex(R"(([0-9]{6})-([0-9]{6}))"),
                                               std::regex("[0-9]{4}-[0-9]{2}-[0-9]{2}")
    };
};


#endif