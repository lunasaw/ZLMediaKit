#include "MultiMp4Publish.h"
#include "util.h"
#include "FileScanner.h"

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

int MultiMp4Publish::Publish(std::string callId, std::string startTime, std::string endTime, std::string speed, std::string app, std::string stream, std::string url)
{
    std::vector<MultiMediaSourceTuple> filePathVec = {};
    // todo: 通过时间段，找出对应的文件列表，同时应该得到第一个文件的开始时刻和最后一个文件结束时刻（也就是需要seek的位置）;
    std::string path = mINI::Instance()[mediakit::Protocol::kMP4SavePath] + "/"+ mINI::Instance()[mediakit::Record::kAppName] + "/" + app + "/" + stream;
    filePathVec = Scanner::getMST(path, startTime, endTime);

    auto poller = EventPollerPool::Instance().getPoller();
    //vhost/app/stream可以随便自己填，现在不限制app应用名了
    createPusher(callId, poller, findSubString(url.data(), nullptr, "://").substr(0, 4), DEFAULT_VHOST, app, stream, filePathVec, url);

    return 0;
}

int MultiMp4Publish::Stop(std::string callId)
{
    deletePusher(callId);
    return 0;
}

int MultiMp4Publish::createPusher(std::string callId, 
                                    const EventPoller::Ptr &poller,
                                    const string &schema,
                                    const string &vhost,
                                    const string &app,
                                    const string &stream,
                                    const std::vector<MultiMediaSourceTuple> &filePath,
                                    const string &url)
{
        auto ps = _mp4PushersMap.find(callId);
        if(ps!=_mp4PushersMap.end()){
            return -1;
        }
    

    std::shared_ptr<MultiMp4Publish::Mp4Pusher> pusher = make_shared<MultiMp4Publish::Mp4Pusher>(this, callId);
    
    pusher->Start(poller, schema, vhost, app, stream, filePath, url);
    lock_guard<mutex> lock(_pusherMapMutex);	
    _mp4PushersMap.emplace(callId, pusher);
    return 0;
}

int MultiMp4Publish::deletePusher(std::string callId)
{
    lock_guard<mutex> lock(_pusherMapMutex);	
    auto ps = _mp4PushersMap.find(callId);
    if(ps!=_mp4PushersMap.end()){
        _mp4PushersMap.erase(ps);
    }
    return 0;
}

void MultiMp4Publish::Mp4Pusher::Start(const EventPoller::Ptr &poller, 
            const string &schema,
            const string &vhost, 
            const string &app, 
            const string &stream, 
            const std::vector<MultiMediaSourceTuple> &filePath, 
            const string &url){
    if (!_src) {
        //不限制APP名，并且指定文件绝对路径
        _src = MediaSource::createFromMultiMP4(schema, vhost, app, stream, filePath, false);
    }
    if (!_src) {
        //文件不存在
        WarnL << "MP4文件不存在:" << filePath.size();
        return;
    }

    //创建推流器并绑定一个MediaSource
    _pusher.reset(new MediaPusher(_src, poller));
    //可以指定rtsp推流方式，支持tcp和udp方式，默认tcp
    //(*g_pusher)[Client::kRtpType] = Rtsp::RTP_UDP;

    //设置推流中断处理逻辑
    MultiMp4Publish* parentPtr = _parent;
    std::string id = _id;
    _pusher->setOnShutdown([this, parentPtr, id, poller, schema, vhost, app, stream, filePath, url](const SockException &ex) {
        WarnL << "Server connection is closed:" << ex.getErrCode() << " " << ex.what();
        //重新推流
        // rePushDelay(poller, schema, vhost, app, stream, filePath, url);
        parentPtr->deletePusher(id);
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
    _timer = std::make_shared<Timer>(2.0f, [parentPtr, id, poller, schema, vhost, app, stream, sourceTuple, url]() {
            InfoL << "Re-Publishing...";
            //重新推流
            parentPtr->createPusher(id, poller, schema, vhost, app, stream, sourceTuple, url);
            //此任务不重复
            return false;
        }, poller);
}