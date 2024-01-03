#include "DiskSpaceManager.h"
#include "Thread/WorkThreadPool.h"
#include <iomanip>
#include <Common/config.h>
#include <Util/NoticeCenter.h>
#include <ctime>

using namespace toolkit;
using namespace mediakit;

DiskSpaceManager::DiskSpaceManager()
{
}

DiskSpaceManager::~DiskSpaceManager()
{
    _timer = nullptr;
}

std::shared_ptr<DiskSpaceManager> DiskSpaceManager::_recordManager = nullptr;
std::shared_ptr<DiskSpaceManager> DiskSpaceManager::GetCreate()
{
    if(!_recordManager){
        _recordManager = std::make_shared<DiskSpaceManager>();
    }
    return _recordManager;
}

bool DiskSpaceManager::StartService(std::string recordPath, CONTROL_MODE_E ctrl_mode)
{
    float timerSec = 30; // 30 秒定时监测录制的文件
    _timer = nullptr;
    _poller = toolkit::WorkThreadPool::Instance().getPoller();
    _timer = std::make_shared<toolkit::Timer>(timerSec, [this, recordPath, ctrl_mode]() {
        DiskInfo diskInfo(recordPath);
        InfoL << "管理模式: " <<  ctrl_mode << ", 硬盘实际使用率: " << diskInfo._UsageRate*100 << "%";
        if(ctrl_mode == THRESHOLD_CTRL){
            if(diskInfo._UsageRate >= DISK_VIDEO_RECORD_THRESHOLD_PERCENTAGE){
                _nDays = -1;
                _deleteOldestFile(recordPath);
            }
        }else if(ctrl_mode == DAYS_CTRL){
            if(diskInfo._UsageRate >= DISK_VIDEO_RECORD_THRESHOLD_PERCENTAGE){
                // 如果磁盘使用超过阈值，则保存的录制文件天数减 1, 但不能少于 5+1 天
                if(_nDays>5) _nDays--;
                InfoL << "磁盘使用量超过的阈值, 录制文件的天数为:" << _nDays;
            }else{
                _nDays = 8; // 如果磁盘空间足够，则保存 7+1 天的文件
                InfoL << "磁盘空间足够, 录制文件的天数为:" << _nDays;
            }
            _deleteOldestFile(recordPath);
        }
        return true;
    }, _poller);

    return true;
}

double DiskSpaceManager::GetStorageSpace(std::string path)
{
    double dirSizeInBytes = 0.0;
    char buffer[1024];
    FILE* pipe = popen(("du -sm " + path).c_str(), "r");
    
    if (!pipe) {
        std::cout << "Error executing du command" << std::endl;
        return dirSizeInBytes;
    }

    // 读取du命令的输出
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        // 提取输出中的占用空间大小
        char* sizeStr = strtok(buffer, "\t");
        if (sizeStr) {
            dirSizeInBytes = atof(sizeStr);
        }
    }

    pclose(pipe);
    return dirSizeInBytes;
}

int DiskSpaceManager::_removeEmptyDirectory(const std::string& path)
{
    if (std::filesystem::is_empty(path)) {
        if (std::filesystem::remove(path)) {
            std::cout << "Directory removed: " << path << std::endl;
            return 0;
        } else {
            std::cerr << "Failed to remove directory: " << path << std::endl;
            return -1;
        }
    } else {
        return -1;
    }
}

void DiskSpaceManager::_removeHiddenFiles(const std::string& path) 
{
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        const std::filesystem::path filePath = entry.path();
        const std::string fileName = filePath.filename().string();
        if (fileName[0] == '.') {
            if (std::filesystem::remove(filePath)) {
                std::cout << "Removed hidden file: " << filePath << std::endl;
            } else {
                std::cerr << "Failed to remove hidden file: " << filePath << std::endl;
            }
        }
    }
}
int DiskSpaceManager::_getFileNumInDirectory(std::string path)
{
    std::filesystem::directory_iterator list(path);
    int counter=0;
	for (auto& it:list) {
        if(it.is_directory() && std::regex_match(it.path().filename().string(), fileDeleteRegexs[1])) 
            counter++;
    }
    // InfoL << "流[" << path << "]已经录制了 " << counter << " 天的数据.";
    return counter;
}

void DiskSpaceManager::_deleteOldestFile(const std::string& path)
{
    std::string maxTimestamp;
    std::string maxFile;
//    std::regex timestampPattern(R"(([0-9]{6})-([0-9]{6}))");
    std::regex timestampPattern = fileDeleteRegexs[0];

    std::string appDirName;

    std::string needDeleteDate;
    std::string needDeleteStream;
    std::string needDeleteAppDirName;

    //判断 record/ 目录下面的文件夹
    for (const auto& appDir : std::filesystem::directory_iterator(path)) {
        
        //应用名 onvif
        if (appDir.is_directory()) {
            appDirName = appDir.path().filename().string();
            if (appDirName == "." || appDirName == "..") {
                continue;
            }
            if(_removeEmptyDirectory(path +"/"+ appDirName)==0) continue;

            for (const auto& streamDir : std::filesystem::directory_iterator(appDir)) {
                // 流 034a0002004bb2cb5ffd__D01_CH01_Main
                if (streamDir.is_directory()) {
                    std::string streamDirName = streamDir.path().filename().string();
                    if (streamDirName == "." || streamDirName == "..") continue;
                    if(_removeEmptyDirectory(path +"/"+ appDirName+ "/" +streamDirName)==0) continue;

                    // 如果目录数量（已经存储了几天的数据）<= 8, 则不需要删除
                    //（这种情况是为了间隔多天开机运行的场景；当按阈值管理时，至少保存 7+1 天）
                    // if(_getFileNumInDirectory(streamDir.path())<=_nDays) continue;
                    // if(_getFileNumInDirectory(streamDir.path())<=8) continue;
                    
                    // 找出最久远的目录
                    
                    for (const auto& dateDir : std::filesystem::directory_iterator(streamDir)) {
                        // 目录(日期) 2023-11-01
                        if (dateDir.is_directory()) {
                            std::string dateDirName = dateDir.path().filename().string();
                            if(needDeleteDate.empty() || dateDirName < needDeleteDate){
                                needDeleteDate = dateDirName;
                                needDeleteStream = streamDirName;
                                needDeleteAppDirName = appDirName;
                            }
                        }
                    }
                }
            }
        }
    }

    // 如果日期距离今天 ≥ _nDays 天，则这个目录的文件都删掉（如果_nDays==-1，则由直接由阈值控制，删除最久远的日期目录）
    bool need_directly_delete = _OverNdays(needDeleteDate, _nDays);
    if(need_directly_delete){
        // 如果这是一个空目录，那么直接删除（先把隐藏文件删掉，否则目录为非空）
        std::string dateDir_path = path+"/"+ needDeleteAppDirName+"/"+ needDeleteStream + "/" + needDeleteDate;
        _removeHiddenFiles(dateDir_path);
        if(_removeEmptyDirectory(dateDir_path)==0) return;

        if (std::regex_match(needDeleteDate, fileDeleteRegexs[1])) {
            // 找出当天录制的最后一个文件
            for (const auto& mp4File : std::filesystem::directory_iterator(dateDir_path)) {
                // 文件名(时间段) 235954-000202.mp4
                if (mp4File.is_regular_file()) {
                    std::string fileName = mp4File.path().filename().string();
                    std::smatch match;
                    // printf("fileName:%s\n", fileName.c_str());
                    
                    if (std::regex_search(fileName, match, timestampPattern)) {
                        std::string timestamp = needDeleteDate + match[0].str();
                        std::string file = match[0].str();
                        // printf("timestamp:%s, maxTimestamp:%s\n", timestamp.c_str(), maxTimestamp.c_str());
                        if (maxTimestamp.empty() || timestamp > maxTimestamp) {
                            maxTimestamp = timestamp;
                            maxFile = file;
                        }
                    }
                }
            }

            // hook， 这个文件之前录制的都将要删除了
            InfoL << "EMIT HOOK NOTICE(delete file): " << needDeleteAppDirName+"/"+needDeleteStream+"/"+needDeleteDate+"/" << maxFile + ".mp4";
            NOTICE_EMIT(KBroadcastDeleteFileArgs, Broadcast::KBroadcastDeleteFile, needDeleteAppDirName, needDeleteStream, needDeleteDate, maxFile + ".mp4");

            // 删除这个 needDeleteDate 目录
            std::string rm_path = path+"/"+ needDeleteAppDirName+"/"+ needDeleteStream + "/" + needDeleteDate;
            std::uintmax_t n = std::filesystem::remove_all(rm_path.c_str());
            if(n==0){
                InfoL << "路径不存在: " << rm_path;
            }else{
                InfoL << "已删除目录:[ "  << needDeleteAppDirName+"/"+needDeleteStream+"/"+needDeleteDate << " ]";
            }
        }
    }
}

bool DiskSpaceManager::_OverNdays(const std::string dir_name, int nDays)
{
    if(nDays == -1) return true; // 如果 nDays==-1，不由天数控制，固定返回 true

    // 获取当前时间戳
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    int now_year = t->tm_year + 1900;
    int now_month = t->tm_mon + 1;
    int now_day = t->tm_mday;

    // 将目录名称转换为struct tm结构体
    struct tm dir_tm;
    memset(&dir_tm, 0, sizeof(dir_tm));
    sscanf(dir_name.c_str(), "%d-%d-%d", &dir_tm.tm_year, &dir_tm.tm_mon, &dir_tm.tm_mday);
    dir_tm.tm_year -= 1900;
    dir_tm.tm_mon -= 1;

    // 将两个时间转换为时间戳
    time_t now_sec = mktime(t);
    time_t dir_sec = mktime(&dir_tm);

    // 计算两个时间戳的差值
    int diff_sec = now_sec - dir_sec;
    int diff_day = diff_sec / 86400;

    // 判断是否相差7天
    int diff = abs(diff_day);
    if (diff>= nDays){
        InfoL << "目录:" << dir_name << ", 距离今天相差:" << diff << " ≥ " << nDays << " (天)" ;
        return true;
    }
    // InfoL << "目录距离今天相差:" << diff << "天";
    return false;
}