#include "DiskSpaceManager.h"
#include "Thread/WorkThreadPool.h"
#include <sys/statvfs.h>
#include <iomanip>

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

bool DiskSpaceManager::StartService(std::string recordPath, float thresholdMB, float DetectionCycle)
{
    _timer = nullptr;
    float timerSec = 3;
    std::string path = recordPath;
    float threshold = _thresholdMB = thresholdMB;
    _poller = toolkit::WorkThreadPool::Instance().getPoller();
    _timer = std::make_shared<toolkit::Timer>(timerSec, [this, path, threshold]() {
        if(_getDirSizeInMB(path) >= threshold){
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
    std::regex timestampPattern(R"(([0-9]{6})-([0-9]{6}))");
    
    for (const auto& appDir : std::filesystem::directory_iterator(path)) {
        if (appDir.is_directory()) {
            std::string appDirName = appDir.path().filename().string();
            if (appDirName == "." || appDirName == "..") {
                continue;
            }
            if(_removeEmptyDirectory(path +"/"+ appDirName)==0) continue;
            for (const auto& streamDir : std::filesystem::directory_iterator(appDir)) {
                if (streamDir.is_directory()) {
                    std::string streamDirName = streamDir.path().filename().string();
                    if (streamDirName == "." || streamDirName == "..") continue;
                    if(_removeEmptyDirectory(path +"/"+ appDirName+ "/" +streamDirName)==0) continue;
                    for (const auto& dateDir : std::filesystem::directory_iterator(streamDir)) {
                        if (dateDir.is_directory()) {
                            std::string dateDirName = dateDir.path().filename().string();
        //                  _removeHiddenFiles(path+"/"+ dirName+"/"+ subDirName);
                            if(_removeEmptyDirectory(path+"/"+ appDirName+"/"+ streamDirName + "/" + dateDirName)==0) continue;
        //                  printf("subDirName:%s\n", subDirName.c_str());
                            if (std::regex_match(dateDirName, std::regex("[0-9]{4}-[0-9]{2}-[0-9]{2}"))) {
                                for (const auto& mp4File : std::filesystem::directory_iterator(dateDir)) {
                                    if (mp4File.is_regular_file()) {
                                        std::string fileName = mp4File.path().filename().string();
                                        std::smatch match;
        //                              printf("fileName:%s\n", fileName.c_str());
                                        if (std::regex_search(fileName, match, timestampPattern)) {
                                            std::string timestamp = dateDirName + match[0].str();
                                            std::string file = match[0].str();
        //                                  printf("timestamp:%s\n", timestamp.c_str());
                                            if (minTimestamp.empty() || timestamp < minTimestamp) {
                                                minTimestamp = timestamp;
                                                minFile = file;
                                                minDir = dateDir.path().string(); // 记录找到最旧文件的目录路径
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
        std::filesystem::remove(std::filesystem::path(minDir) / (minFile + ".MP4"));
#ifdef DEBUG_RECORD_MANAGER
        std::cout << "已删除日期和时间都最久远的文件：" << minDir << "/" << minFile << ".MP4" << std::endl;
#endif
    }
}

float DiskSpaceManager::getSystemDisk(std::string recordPath) {
    //todo 根据recordPath 确认挂在分区的总大小
    const char * path = recordPath.c_str();
    struct statvfs buf ;
    std::cout << "getSystemDisk recordPath :  " <<recordPath<<std::endl;
    if(statvfs(path,&buf) == -1){
        //查不到挂在的路径分区大小
        perror("statbuf");
        std::cout << "getSystemDisk error :%s " <<path<<std::endl;
        return -1;
    }
    _fileCapacity = (double)buf.f_blocks * buf.f_frsize / (1024 * 1024 * 1024) ;
    _fileAvailable = (double)buf.f_bavail * buf.f_frsize / (1024 * 1024 * 1024);

#ifdef DEBUG_RECORD_MANAGER

    std::cout << "File system capacity: " <<std::fixed << std::setprecision(2) << _fileCapacity << " GB" << std::endl;
    std::cout << "File system free space: " << std::fixed << std::setprecision(2) << (double)buf.f_bfree * buf.f_frsize / (1024 * 1024 * 1024) << " GB" << std::endl;
    std::cout << "File system available space: " << std::fixed << std::setprecision(2) <<_fileAvailable << " GB" << std::endl;
#endif
    return _fileCapacity;
}
