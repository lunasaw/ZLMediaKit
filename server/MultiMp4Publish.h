#ifndef MULTIMP4_PUBLISH_H
#define MULTIMP4_PUBLISH_H

#include <signal.h>
#include <iostream>
#include <map>
#include "Util/logger.h"
#include "Util/NoticeCenter.h"
#include "Poller/EventPoller.h"
#include "Player/PlayerProxy.h"
#include "Rtmp/RtmpPusher.h"
#include "Common/config.h"
#include "Common/Parser.h"
#include "Pusher/MediaPusher.h"
#include "Record/MP4Reader.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

class MultiMp4Publish
{
public:
    MultiMp4Publish(/* args */);
    ~MultiMp4Publish();
    static std::shared_ptr<MultiMp4Publish> GetCreate();

    int Publish(std::string callId, std::string startTime, std::string endTime, std::string speed, std::string app, std::string stream, std::string url, std::string& errMsg);
    int Stop(std::string callId, std::string& errMsg);
    
protected:
    int createPusher(std::string callId, 
                        const EventPoller::Ptr &poller,
                        const string &schema,
                        const string &vhost,
                        const string &app,
                        const string &stream,
                        const std::vector<MultiMediaSourceTuple> &filePath,
                        const string &url,
                        float speed);
    int deletePusher(std::string callId, std::string& errMsg);
    
private:
    class Mp4Pusher
    {
    public:
        Mp4Pusher(MultiMp4Publish* parent, std::string id, float speed):_parent(parent), _id(id), _speed(speed){}
        ~Mp4Pusher(){
            if(_src) _src->close(true);
        }

        int Start(const EventPoller::Ptr &poller, 
                    const string &schema,
                    const string &vhost, 
                    const string &app, 
                    const string &stream, 
                    const std::vector<MultiMediaSourceTuple> &filePath, 
                    const string &url);

        void rePushDelay(const EventPoller::Ptr &poller,
                 const string &schema,
                 const string &vhost,
                 const string &app,
                 const string &stream,
                 const std::vector<MultiMediaSourceTuple> &filePath,
                 const string &url);

    private:
        MediaPusher::Ptr _pusher = nullptr;
        Timer::Ptr _timer = nullptr;
        MediaSource::Ptr _src = nullptr;
        MultiMp4Publish* _parent = nullptr;
        std::string _id;
        float _speed;
    };
    
    friend class Mp4Pusher;
    static std::shared_ptr<MultiMp4Publish> pMp4Publish;
    std::mutex _pusherMapMutex;
    std::map<std::string/* callId */, std::shared_ptr<Mp4Pusher>> _mp4PushersMap;
};


#endif