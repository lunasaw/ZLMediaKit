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

#define DEBUG_RECORD_MANAGER

class DiskSpaceManager
{
public:
    DiskSpaceManager();
    ~DiskSpaceManager();

    static std::shared_ptr<DiskSpaceManager> GetCreate();
    bool StartService(std::string recordPath, float thresholdMB, float DetectionCycle);
    double GetStorageSpace(std::string recordPath);
    float GetThreshold() { return _thresholdMB; }

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
};


#endif