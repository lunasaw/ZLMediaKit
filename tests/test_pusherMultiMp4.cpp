//
// Created by Hope liao on 2023/10/24.
//

#include "Common/Parser.h"
#include "Common/config.h"
#include "Poller/EventPoller.h"
#include "Pusher/MediaPusher.h"
#include "Record/MultiMP4Reader.h"
#include "Record/MultiMediaSourceTuple.h"
#include "Rtmp/RtmpPusher.h"
#include "Util/logger.h"
#include <iostream>
#include <signal.h>

using namespace std;
using namespace toolkit;
using namespace mediakit;

//推流器，保持强引用
MediaPusher::Ptr g_pusher;
Timer::Ptr g_timer;
MediaSource::Ptr g_src;

//声明函数
//推流失败或断开延迟2秒后重试推流
void rePushDelay(const EventPoller::Ptr &poller,
                 const string &schema,
                 const string &vhost,
                 const string &app,
                 const string &stream,
                 const std::vector<MultiMediaSourceTuple> sourceTuple,
                 const string &url);

//创建推流器并开始推流
void createPusher(const EventPoller::Ptr &poller,
                  const string &schema,
                  const string &vhost,
                  const string &app,
                  const string &stream,
                  const std::vector<MultiMediaSourceTuple> sourceTuple,
                  const string &url) {

    InfoL << "schema:" << schema << ",vhost:" << vhost << ",app:" << app << ",stream:" << stream << ",url:" << url;
    if (!g_src) {
        //不限制APP名，并且指定文件绝对路径
        g_src = MediaSource::createFromMultiMP4(schema, vhost, app, stream, sourceTuple, false);
    }
    if (!g_src) {
        //文件不存在
        WarnL << "MP4文件不存在:" << sourceTuple.size();
        return;
    }

    //创建推流器并绑定一个MediaSource
    g_pusher.reset(new MediaPusher(g_src, poller));
    //可以指定rtsp推流方式，支持tcp和udp方式，默认tcp
    //(*g_pusher)[Client::kRtpType] = Rtsp::RTP_UDP;

    //设置推流中断处理逻辑
    g_pusher->setOnShutdown([poller, schema, vhost, app, stream, sourceTuple, url](const SockException &ex) {
        WarnL << "Server connection is closed:" << ex.getErrCode() << " " << ex.what();
        //重新推流
        rePushDelay(poller, schema, vhost, app, stream, sourceTuple, url);
    });

    //设置发布结果处理逻辑
    g_pusher->setOnPublished([poller, schema, vhost, app, stream, sourceTuple, url](const SockException &ex) {
        if (ex) {
            WarnL << "Publish fail:" << ex.getErrCode() << " " << ex.what();
            //如果发布失败，就重试
            rePushDelay(poller, schema, vhost, app, stream, sourceTuple, url);
        } else {
            InfoL << "Publish success,Please play with player:" << url;
        }
    });
    g_pusher->publish(url);
}

//推流失败或断开延迟2秒后重试推流
void rePushDelay(const EventPoller::Ptr &poller,
                 const string &schema,
                 const string &vhost,
                 const string &app,
                 const string &stream,
                 const std::vector<MultiMediaSourceTuple> sourceTuple,
                 const string &url) {
    g_timer = std::make_shared<Timer>(2.0f, [poller, schema, vhost, app, stream, sourceTuple, url]() {
            InfoL << "Re-Publishing...";
            //重新推流
            createPusher(poller, schema, vhost, app, stream, sourceTuple, url);
            //此任务不重复
            return false;
        }, poller);
}

//这里才是真正执行main函数，你可以把函数名(domain)改成main，然后就可以输入自定义url了
int domain(const std::vector<MultiMediaSourceTuple> &sourceTuple, const string &pushUrl) {
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    //循环点播mp4文件
    mINI::Instance()[Record::kFileRepeat] = 1;
    mINI::Instance()[Protocol::kHlsDemand] = 1;
    mINI::Instance()[Protocol::kTSDemand] = 1;
    mINI::Instance()[Protocol::kFMP4Demand] = 1;
    //mINI::Instance()[Protocol::kRtspDemand] = 1;
    //mINI::Instance()[Protocol::kRtmpDemand] = 1;

    auto poller = EventPollerPool::Instance().getPoller();
    //vhost/app/stream可以随便自己填，现在不限制app应用名了
    std::string schema = findSubString(pushUrl.data(), nullptr, "://").substr(0, 4);
    createPusher(poller, schema, DEFAULT_VHOST, "record", "stream", sourceTuple, pushUrl);
    //设置退出信号处理函数
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    g_pusher.reset();
    g_timer.reset();
    return 0;
}

int main(int argc, char *argv[]) {
    //可以使用test_server生成的mp4文件
    //文件使用绝对路径，推流url支持rtsp和rtmp
    std::vector<MultiMediaSourceTuple> filePathVec = {};
    filePathVec.emplace_back("/Users/hopeliao/workspace/WDianZLMediaKit/release/darwin/Debug/www/record/live/v12345-67890/2023-10-16/10-20-55-0.mp4", 0 *10 * 1000, 0 *45 * 1000);
    filePathVec.emplace_back("/Users/hopeliao/workspace/WDianZLMediaKit/release/darwin/Debug/www/record/live/v12345-67890/2023-10-25/13-51-13-0.mp4", 20 * 1000, 0);
    filePathVec.emplace_back("/Users/hopeliao/workspace/WDianZLMediaKit/release/darwin/Debug/www/record/live/v12345-67890/2023-10-25/13-51-13-0.mp4", 0, 0);

    return domain(filePathVec, "rtsp://127.0.0.1/live/rtsp_push");

//    return domain(filePathVec, "rtsp://10.37.4.14:1554/live/v12345-67890?sign=41db35390ddad33f83944f44b8b75ded");

    //    return domain("/Users/hopeliao/workspace/WDianZLMediaKit/release/darwin/Debug/www/record/live/v12345-67890/2023-10-16/10-20-55-0.mp4",
    //                  "rtp://127.0.0.1:10000");
}





