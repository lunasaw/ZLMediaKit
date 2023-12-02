#include "MultiMp4Publish.h"
#include "Util/util.h"
//#include "FileScanner.h"
#include "FileScanner_2.h"

MultiMp4Publish::MultiMp4Publish(/* args */)
{
    
}

MultiMp4Publish::~MultiMp4Publish()
{
}

std::shared_ptr<MultiMp4Publish> MultiMp4Publish::pMp4Publish = nullptr;
std::shared_ptr<MultiMp4Publish> MultiMp4Publish::GetCreate()
{
    if(!pMp4Publish){
        pMp4Publish = std::make_shared<MultiMp4Publish>();
    }
    return pMp4Publish;
}

int MultiMp4Publish::Publish(std::string callId, std::string startTime, std::string endTime, std::string speed, std::string app, std::string stream, std::string url, std::string& errMsg)
{
    std::vector<MultiMediaSourceTuple> filePathVec = {};
    // todo: 通过时间段，找出对应的文件列表，同时应该得到第一个文件的开始时刻和最后一个文件结束时刻（也就是需要seek的位置）;
    std::string path = mINI::Instance()[mediakit::Protocol::kMP4SavePath] + "/"+ mINI::Instance()[mediakit::Record::kAppName] + "/" + app + "/" + stream;

    FileScanner fileScanner;
    filePathVec = fileScanner.getPlayFileList(path, startTime, endTime);
    if(filePathVec.empty()){
        errMsg = "未匹配到任何文件[时段:" + startTime + " _ " + endTime + ", 路径：" + path + "]";
        WarnL << errMsg;
        return -1;
    }

    auto poller = EventPollerPool::Instance().getPoller();
    if(createPusher(callId, poller, RTSP_SCHEMA, DEFAULT_VHOST, app, stream, filePathVec, url, atof(speed.c_str())/*atof(speed.c_str())<1.0f?1.0f:atof(speed.c_str())*/)<0){
        errMsg = "推流失败";
        WarnL << errMsg;
        return -1;
    }

    return 0;
}

int MultiMp4Publish::Stop(std::string callId, std::string& errMsg)
{
    return deletePusher(callId, errMsg);
}

int MultiMp4Publish::createPusher(std::string callId, 
                                    const EventPoller::Ptr &poller,
                                    const string &schema,
                                    const string &vhost,
                                    const string &app,
                                    const string &stream,
                                    const std::vector<MultiMediaSourceTuple> &filePath,
                                    const string &url,
                                    float speed)
{
    _pusherMapMutex.lock();
    auto ps = _mp4PushersMap.find(callId);
    if(ps!=_mp4PushersMap.end()){
        _pusherMapMutex.unlock();
        return -1;
    }
    _pusherMapMutex.unlock();
    
    std::shared_ptr<MultiMp4Publish::Mp4Pusher> pusher = make_shared<MultiMp4Publish::Mp4Pusher>(this, callId, speed);
    if(pusher->Start(poller, schema, vhost, app, stream, filePath, url)<0){
        return -1;
    }
    DebugL << "Multi MP4 Pusher Create success.";
    lock_guard<mutex> lock(_pusherMapMutex);	
    _mp4PushersMap.emplace(callId, pusher);
    return 0;
}

int MultiMp4Publish::deletePusher(std::string callId, std::string& errMsg)
{
    lock_guard<mutex> lock(_pusherMapMutex);	
    auto ps = _mp4PushersMap.find(callId);
    if(ps!=_mp4PushersMap.end()){
        _mp4PushersMap.erase(ps);
        errMsg = "Multi MP4 Pusher Delete success. callId: " + callId;
        DebugL << errMsg;
        return 0;
    }else{
        errMsg = "Multi MP4 Pusher Delete faild: 不存在此 callId:" + callId;
        DebugL << errMsg;
        return -1;
    }
}

int MultiMp4Publish::Mp4Pusher::Start(const EventPoller::Ptr &poller, 
            const string &schema,
            const string &vhost, 
            const string &app, 
            const string &stream, 
            const std::vector<MultiMediaSourceTuple> &filePath, 
            const string &url){
    
    MultiMp4Publish* parentPtr = _parent;
    std::string id = _id;
    _src = MediaSource::createFromMultiMP4(_id, schema, vhost, app, stream, filePath, false, _speed, [this, parentPtr, id](){
        DebugL << "End of streaming, Close Connection";
        std::string msg;
        parentPtr->deletePusher(id, msg);
    });
    if (!_src) {
        ErrorL << "Multi MP4 source Create faild: " << filePath.size();
        return -1;
    }
    DebugL << "Multi MP4 source Create success.";

    //创建推流器并绑定一个MediaSource
    _pusher.reset(new MediaPusher(_src, poller));
    //可以指定rtsp推流方式，支持tcp和udp方式，默认tcp
    //(*g_pusher)[Client::kRtpType] = Rtsp::RTP_UDP;

    //设置推流中断处理逻辑
    _pusher->setOnShutdown([parentPtr, id, poller, schema, vhost, app, stream, filePath, url](const SockException &ex) {
        WarnL << "Server connection is closed:" << ex.getErrCode() << " " << ex.what();
        //重新推流
        // rePushDelay(poller, schema, vhost, app, stream, filePath, url);
        std::string msg;
        parentPtr->deletePusher(id, msg);
    });

    //设置发布结果处理逻辑
    _pusher->setOnPublished([this, poller, schema, vhost, app, stream, filePath, url](const SockException &ex) {
        if (ex) {
            WarnL << "Publish fail:" << ex.getErrCode() << " " << ex.what();
            //如果发布失败，就重试
            rePushDelay(poller, schema, vhost, app, stream, filePath, url);
        } else {
            InfoL << "Publish success,Please play with player:" << url;
        }
    });
    _pusher->publish(url);

    return 0;
}


//推流失败或断开延迟2秒后重试推流
void MultiMp4Publish::Mp4Pusher::rePushDelay(const EventPoller::Ptr &poller,
                                            const string &schema,
                                            const string &vhost,
                                            const string &app,
                                            const string &stream,
                                            const std::vector<MultiMediaSourceTuple> &sourceTuple,
                                            const string &url) {
    MultiMp4Publish* parentPtr = _parent;
    std::string id = _id;
    _timer = std::make_shared<Timer>(2.0f, [this, parentPtr, id, poller, schema, vhost, app, stream, sourceTuple, url]() {
            InfoL << "Re-Publishing...";
            //重新推流
            std::string msg;
            parentPtr->deletePusher(id, msg);
            parentPtr->createPusher(id, poller, schema, vhost, app, stream, sourceTuple, url, _speed);
            //此任务不重复
            return false;
        }, poller);
}