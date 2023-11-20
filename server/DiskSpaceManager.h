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

#include "Poller/EventPoller.h"
#include "Poller/Timer.h"

// #define DEBUG_RECORD_MANAGER
#define DISK_VIDEO_RECORD_THRESHOLD_PERCENTAGE  0.9f //默认固定开始清理的阈值，当硬盘容量在大于90%时启动清除
class DiskSpaceManager
{
public:
    DiskSpaceManager();
    ~DiskSpaceManager();

    static std::shared_ptr<DiskSpaceManager> GetCreate();
    bool StartService(std::string recordPath, float thresholdMB, float DetectionCycle);
    double GetStorageSpace(std::string recordPath);
    float GetThreshold() { return _thresholdMB; }
    float getSystemDisk(std::string recordPath);//MB
    float getAvailableDiskCap(std::string recordPath);
    int getUsedDisSpace(std::string recordPath);
    void setDeleteVideoThreshold(float  thresholdPercentage);
    float getDeleteVideoThreshold();

private:
    double _getDirSizeInMB(std::string path);
    int _removeEmptyDirectory(const std::string& path);
    void _removeHiddenFiles(const std::string& path);
    void _deleteOldestFile(const std::string& path);

private:
    static std::shared_ptr<DiskSpaceManager> _recordManager;
    toolkit::EventPoller::Ptr _poller;
    toolkit::Timer::Ptr _timer;
    float _thresholdMB = 0.0f;
    double _fileCapacity = 0.0 ; //GB
    double _fileAvailable = 0.0; //GB
    float video_delete_percentage = DISK_VIDEO_RECORD_THRESHOLD_PERCENTAGE;
};


#endif