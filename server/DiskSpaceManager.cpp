#include "DiskSpaceManager.h"
#include "Thread/WorkThreadPool.h"
#include <sys/statvfs.h>
#include <iomanip>
#include <Common/config.h>
#include <Util/NoticeCenter.h>

using namespace toolkit;
using namespace mediakit;

DiskSpaceManager::DiskSpaceManager()
{
        video_delete_percentage = DISK_VIDEO_RECORD_THRESHOLD_PERCENTAGE;
}

DiskSpaceManager::~DiskSpaceManager()
{
    _timer = nullptr;
    video_delete_percentage = 0;
}

std::shared_ptr<DiskSpaceManager> DiskSpaceManager::_recordManager = nullptr;
std::shared_ptr<DiskSpaceManager> DiskSpaceManager::GetCreate()
{
    if(!_recordManager){
        _recordManager = std::make_shared<DiskSpaceManager>();
    }
    return _recordManager;
}

bool DiskSpaceManager::StartService(std::string recordPath, float thresholdMB, float DetectionCycle)
{
    _timer = nullptr;
    float timerSec = 1;
    std::string path = recordPath;
    float threshold = _thresholdMB = thresholdMB;
    InfoL << "StartService threshold " << threshold <<std::endl;
    _poller = toolkit::WorkThreadPool::Instance().getPoller();
    _timer = std::make_shared<toolkit::Timer>(timerSec, [this, path, threshold]() {
        if(getUsedDisSpace(path) >= threshold){
        // if(_getDirSizeInMB(path) >= threshold){
            _deleteOldestFile(path);
        }
        return true;
    }, _poller);
    return true;
}

double DiskSpaceManager::GetStorageSpace(std::string recordPath)
{
    return _getDirSizeInMB(recordPath);
}

double DiskSpaceManager::_getDirSizeInMB(std::string path)
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
#ifdef DEBUG_RECORD_MANAGER
    std::cout << "dir[ " << path <<" ] size:" << dirSizeInBytes << " MB" << std::endl;
#endif
    return dirSizeInBytes;
}

int DiskSpaceManager::_removeEmptyDirectory(const std::string& path)
{
    if (std::filesystem::is_empty(path)) {
        if (std::filesystem::remove(path)) {
#ifdef DEBUG_RECORD_MANAGER
            std::cout << "Directory removed: " << path << std::endl;
#endif
            return 0;
        } else {
            // std::cerr << "Failed to remove directory: " << path << std::endl;
            return -1;
        }
    } else {
#ifdef DEBUG_RECORD_MANAGER
        std::cerr << "Directory is not empty: " << path << std::endl;
#endif
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
#ifdef DEBUG_RECORD_MANAGER
                std::cout << "Removed hidden file: " << filePath << std::endl;
#endif
            } else {
                // std::cerr << "Failed to remove hidden file: " << filePath << std::endl;
            }
        }
    }
}

void DiskSpaceManager::_deleteOldestFile(const std::string& path)
{
    std::string minDir;
    std::string minTimestamp;
    std::string minFile;
//    std::regex timestampPattern(R"(([0-9]{6})-([0-9]{6}))");
    std::regex timestampPattern = fileDeleteRegexs[0];

    std::string appDirName;
    std::string streamDirName;
    std::string dateDirName;

    std::string needDeleteDate;
    std::string needDeleteStream;
    for (const auto& appDir : std::filesystem::directory_iterator(path)) {
        //判断 record/ 目录下面的文件夹
        if (appDir.is_directory()) {
            appDirName = appDir.path().filename().string();
            if (appDirName == "." || appDirName == "..") {
                continue;
            }
            if(_removeEmptyDirectory(path +"/"+ appDirName)==0) continue;
            for (const auto& streamDir : std::filesystem::directory_iterator(appDir)) {
                //判断 record/onvif/ 层名字
                if (streamDir.is_directory()) {
                    streamDirName = streamDir.path().filename().string();
                    if (streamDirName == "." || streamDirName == "..") continue;
                    if(_removeEmptyDirectory(path +"/"+ appDirName+ "/" +streamDirName)==0) continue;
                    for (const auto& dateDir : std::filesystem::directory_iterator(streamDir)) {
                        //判断流名字 record/onvif/034a0002004bb2cb5ffd__D01_CH01_Main
                        if (dateDir.is_directory()) {
                            dateDirName = dateDir.path().filename().string();
        //                  _removeHiddenFiles(path+"/"+ dirName+"/"+ subDirName);
                            if(_removeEmptyDirectory(path+"/"+ appDirName+"/"+ streamDirName + "/" + dateDirName)==0) continue;
        //                  printf("subDirName:%s\n", subDirName.c_str());
                            // 判断流名字 record/onvif/034a0002004bb2cb5ffd__D01_CH01_Main/2023-11-01

//                            if (std::regex_match(dateDirName, std::regex("[0-9]{4}-[0-9]{2}-[0-9]{2}"))) {
                            if (std::regex_match(dateDirName, fileDeleteRegexs[1])) {
                                for (const auto& mp4File : std::filesystem::directory_iterator(dateDir)) {
                                    if (mp4File.is_regular_file()) {
                                        std::string fileName = mp4File.path().filename().string();
                                        std::smatch match;
        //                              printf("fileName:%s\n", fileName.c_str());
                                        // 判断流名字 record/onvif/034a0002004bb2cb5ffd__D01_CH01_Main/2023-11-01/235954-000202.mp4
                                        if (std::regex_search(fileName, match, timestampPattern)) {
                                            std::string timestamp = dateDirName + match[0].str();
                                            std::string file = match[0].str();
        //                                  printf("timestamp:%s\n", timestamp.c_str());
                                            if (minTimestamp.empty() || timestamp < minTimestamp) {
                                                minTimestamp = timestamp;
                                                minFile = file;
                                                minDir = dateDir.path().string(); // 记录找到最旧文件的目录路径
                                                needDeleteDate = dateDirName;
                                                needDeleteStream = streamDirName;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 删除日期和时间都最久远的文件
    if (!minTimestamp.empty()) {
        bool result  = std::filesystem::remove(std::filesystem::path(minDir) / (minFile + ".mp4"));
        if(result){
            NOTICE_EMIT(KBroadcastDeleteFileArgs, Broadcast::KBroadcastDeleteFile, appDirName, needDeleteStream, needDeleteDate, minFile + ".mp4");
            InfoL << "已删除日期和时间都最久远的文件 "  << minDir << "/" << minFile << ".mp4"
                  << ",result:"<<result << ", errno:" << strerror(errno);
        } else{
            InfoL << "remove file failed " << strerror(errno) << std::endl;
        }
    }
}

float DiskSpaceManager::getSystemDisk(std::string recordPath) {
    //todo 根据recordPath 确认挂在分区的总大小
    const char * path = recordPath.c_str();
    struct statvfs buf ;
    InfoL << recordPath <<std::endl;
    if(statvfs(path,&buf) == -1){
        //查不到挂在的路径分区大小
        perror("statbuf");
        std::cout << "getSystemDisk error : " <<path<<std::endl;
        InfoL << "getSystemDisk error :" << path <<std::endl;
        return 0;
    }
    _fileCapacity = (double)buf.f_blocks * buf.f_frsize / (1024 * 1024 * 1024) ;
    _fileAvailable = (double)buf.f_bavail * buf.f_frsize / (1024 * 1024 * 1024);
    double usedDiskCap = (double)((buf.f_blocks - buf.f_bfree) * buf.f_frsize /( 1024 * 1024 * 1024) );
    InfoL<< "getSystemDisk _fileCapacity " << _fileCapacity  << "  GB, _fileAvailable " <<_fileAvailable
          << " GB,usedDiskCap " << usedDiskCap <<" GB" << std::endl;
#ifdef OFF

    std::cout << "File system capacity: " <<std::fixed << std::setprecision(2) << _fileCapacity << " GB" << std::endl;
    std::cout << "File system free space: " << std::fixed << std::setprecision(2) << (double)buf.f_bfree * buf.f_frsize / (1024 * 1024 * 1024) << " GB" << std::endl;
    std::cout << "File system available space: " << std::fixed << std::setprecision(2) <<_fileAvailable << " GB" << std::endl;
#endif
    return _fileCapacity;
}
float DiskSpaceManager::getAvailableDiskCap(std::string recordPath) {
    const char * path = recordPath.c_str();
    struct statvfs buf ;
    InfoL << "getAvailableDiskCap" << recordPath <<std::endl;
    if(statvfs(path,&buf) == -1){
        //查不到挂在的路径分区大小
        perror("statbuf");
        InfoL << "getSystemDisk error :" << path <<std::endl;
        return 0;
    }
    _fileAvailable = (double)buf.f_bavail * buf.f_frsize / (1024 * 1024 * 1024);
    InfoL  << " _fileAvailable " <<_fileAvailable << std::endl;
    return _fileAvailable;
}
int DiskSpaceManager::getUsedDisSpace(std::string recordPath) {
    FILE *fp;
    char buffer[1024];
    InfoL  << "usedDiskSpace  " << recordPath << std::endl;
    std::filesystem::path record_path(recordPath);
    if (!std::filesystem::exists(record_path)) {
        WarnL << "Path [ " << recordPath << " ] does not exist!";
        return 0;
    }
    std::string path = recordPath;

    //获取路径上的容量
    std::string cmp_pre = "df -m ";
    std::string cmd = cmp_pre + " " + path;
    InfoL  <<  "cmd: " <<cmd <<std::endl;
    fp = popen(cmd.c_str() , "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
        InfoL  <<  "Failed to run command " <<std::endl;
        return 0;
    }
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        InfoL  <<  "buffer " << buffer << std::endl;
    }

    pclose(fp);

    //从读取出来的数据中读取 容量 字符串
    std::stringstream used(buffer);
    std::vector<std::string> usedVec;
    std::string word;

    while (used >> word) {
        usedVec.push_back(word);
    }
    InfoL  << "used :" << usedVec[2]<< std::endl;
    int currentUsed;
    currentUsed = atoi(usedVec[2].c_str());
    //使用正则表达式获取容量大小
//    std::regex reg("\\d+");
//    std::smatch match;
//    int currentUsed;
//    if (std::regex_search(usedVec[2], match, reg)) {
//        std::string num_str = match[0].str();
//        currentUsed = std::stoi(num_str);
//    }
//    std::cout << currentUsed << std::endl;
    InfoL  << "currentUsed :" << currentUsed   <<  "  MB"<< std::endl;
    return currentUsed;
}

void DiskSpaceManager::setDeleteVideoThreshold(float thresholdPen) {
    InfoL  << thresholdPen << std::endl;
    video_delete_percentage = thresholdPen;
}

float DiskSpaceManager::getDeleteVideoThreshold() {
    InfoL << video_delete_percentage << std::endl;
    return video_delete_percentage;
}
