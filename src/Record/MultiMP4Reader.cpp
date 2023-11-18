//
// Created by Hope liao on 2023/10/17.
//

#include "MultiMP4Reader.h"
#include "Common/config.h"
#include "Thread/WorkThreadPool.h"
#include "Util/File.h"

#define MAX(x,y) (x>y?x:y)

using namespace std;
using namespace toolkit;
namespace mediakit {

MultiMP4Reader::MultiMP4Reader(const std::string &vhost,
                               const std::string &app,
                               const std::string &stream_id,
                               const std::vector<MultiMediaSourceTuple> &file_path,
                               float speed, std::function<void()> endCB) {
    _speed = speed;
    _tuple =  MediaTuple{vhost, app, stream_id};
    _poller = WorkThreadPool::Instance().getPoller();
    _file_path = file_path;
    _end_CB = endCB;
    if (_tuple.stream.empty()) {
        return;
    }
    loadMP4(_currentIndex);
}

MultiMP4Reader::~MultiMP4Reader() {
    DebugL << "MultiMP4Reader release done";
}

bool MultiMP4Reader::loadMP4(int index) {
    if (_file_path.empty())
        return false;

    MultiMediaSourceTuple tuple = _file_path.at(index);
    _current_source_tuple = tuple;
    if(File::fileExist(tuple.path.c_str())) {
        DebugL << "found path:" << tuple.path;
    } else {
        DebugL << "not found path:" << tuple.path;
        return false;
    }
    _demuxer = std::make_shared<MP4Demuxer>();
    _demuxer->openMP4(tuple.path);

    auto tracks = _demuxer->getTracks(false);
    if (tracks.empty()) {
        throw std::runtime_error(StrPrinter << "该mp4文件没有有效的track:" << tuple.path);
    }
    for (auto &track : tracks) {
        if (track->getTrackType() == TrackVideo) {
            _have_video = true;
        }
    }
    if(!_muxer) {
        ProtocolOption option;
        //读取mp4文件并流化时，不重复生成mp4/hls文件
        option.enable_mp4 = false;
        option.enable_hls = false;
        _muxer = std::make_shared<MultiMediaSourceMuxer>(_tuple, 0, option);

        for (auto &track : tracks) {
            _muxer->addTrack(track);
        }
        //添加完毕所有track，防止单track情况下最大等待3秒
        _muxer->addTrackCompleted();
    }

    _currentIndex++;
    _read_mp4_item_done = true;
    DebugL << "load mp4 done:" << tuple.path << ",duration:" << _demuxer->getDurationMS();
    return true;
}

bool MultiMP4Reader::isPlayEof() {
    if(_currentIndex >= (int)_file_path.size()) {
        return true;
    }
    return false;
}

void MultiMP4Reader::checkNeedSeek() {
    MultiMediaSourceTuple currTuple = _current_source_tuple;
    if(currTuple.startMs != 0) {
        DebugL << "seek to:" << currTuple.startMs;
        seekTo(currTuple.startMs);
    }
    _seek_ticker.resetTime(); //强制还原
}

void MultiMP4Reader::startReadMP4(uint64_t sample_ms, bool ref_self, bool file_repeat) {
    if(_file_path.empty()) {
        ErrorL << " file path empty";
        return;
    }
    GET_CONFIG(uint32_t, sampleMS, Record::kSampleMS);
    auto strong_self = shared_from_this();
    if (_muxer) {
        //一直读到所有track就绪为止
        while (!_muxer->isAllTrackReady() && readNextSample());
        //注册后再切换OwnerPoller
        _muxer->setMediaListener(strong_self);
        checkNeedSeek();
    }

    auto timer_sec = (sample_ms ? sample_ms : sampleMS) / 1000.0f;
    //启动定时器
    if (ref_self) {
        //callback 返回false 不在重复
        _timer = std::make_shared<Timer>(timer_sec, [strong_self]() {
                lock_guard<recursive_mutex> lck(strong_self->_mtx);
                bool flag = strong_self->readSample();

                if(!flag) {
                    DebugL << "read sample:0";
                    strong_self->_end_CB();
                }
                return flag;
            }, _poller);
    } else {
        weak_ptr<MultiMP4Reader> weak_self = strong_self;
        _timer = std::make_shared<Timer>(timer_sec, [weak_self]() {
                auto strong_self = weak_self.lock();
                if (!strong_self) {
                    return false;
                }
                lock_guard<recursive_mutex> lck(strong_self->_mtx);
                bool flag = strong_self->readSample();
                if(!flag) {
                    DebugL << "read sample:0";
                    strong_self->_end_CB();
                }
                return flag;
            }, _poller);
    }

    _file_repeat = file_repeat;
}

void MultiMP4Reader::stopReadMP4() {
    _timer = nullptr;
}

bool MultiMP4Reader::readSample() {
    if (_paused) {
        //确保暂停时，时间轴不走动
        _seek_ticker.resetTime();
        return true;
    }

    bool keyFrame = false;
    bool eof = false;
    uint64_t oldDts = 0;
    if((_last_dts - _capture_dts) >= getCurrentStamp()) {
        DebugL << "视频DTS大于系统时间,跳出循环." 
               << ",_last_dts:" << std::to_string(_last_dts) 
               << ",_capture_dts:" << std::to_string(_capture_dts)
               << ",offset:" << (_last_dts - _capture_dts)
               << ",current:" << getCurrentStamp();
    }
    
    while (!eof && (_last_dts - _capture_dts) < getCurrentStamp()) {
        _read_mp4_item_done = false;

        auto frame = _demuxer->readFrame(keyFrame, eof);
        if (!frame) {
            continue;
        }
        auto frameFromPtr = std::dynamic_pointer_cast<FrameFromPtr>(frame);

        if(!frameFromPtr) {
            continue;
        }
        if(_read_sample_last_dts >= frame->dts()) {
            frameFromPtr->setDTS(_read_sample_last_dts + 1);
            frameFromPtr->setPTS(_read_sample_last_dts + 1);
        }
        _read_sample_last_dts = frame->dts();

        oldDts = frame->dts();
        if(_first_read) {
            _first_read = false;
            DebugL << "first read frame dts:" << oldDts << ",isKeyFrame:" << frame->keyFrame();
            if(frameFromPtr->dts() != 0) {
                _first_read_dts = frameFromPtr->dts();
                // frameFromPtr->setDTS(0);
            }
        }

        _last_dts = MAX(frameFromPtr->dts() - _first_read_dts, 0) + _capture_dts;
        _last_pts = MAX(frameFromPtr->dts() - _first_read_dts, 0) + _capture_pts;

        if (_muxer) {
            if(frameFromPtr) {
                // frameFromPtr->setPTS(_last_pts/_speed);
                // frameFromPtr->setDTS(_last_dts/_speed);
                frameFromPtr->setPTS(_last_pts);
                frameFromPtr->setDTS(_last_dts);
            }
           DebugL << "oldDts:" << oldDts
                  << ",inputDts:" << frame->dts()
                  << ",capture:" << _capture_dts
                  << ",_last_dts:" << _last_dts
                  << ",_capture_dts:" << _capture_dts
                  << ",isKeyFrame:" << frame->keyFrame()
                  << ",now:" << getCurrentStamp()
                  << ",codecId:" << frame->getCodecName();
            _muxer->inputFrame(frame);
        }
    }

    MultiMediaSourceTuple tuple = _current_source_tuple;
    if(tuple.endMs != 0) {
        if(oldDts >= tuple.endMs) {
            eof = true;
            DebugL << "readFrame 超过结束时间限制，强制 eof 结束";
        }
    }

    if(eof) {
        if(isPlayEof()) {
            DebugL << "play eof.";
            return false;
        }
        _capture_dts = _last_dts;
        _capture_pts = _last_pts;
        DebugL << "MultiMP4Reader readSample EOF."
               << ",_file_repeat:" << _file_repeat
               << ", isPlayEof:" << isPlayEof();

        if(loadMP4(_currentIndex)) {
            _first_read = true;
            _first_read_dts = 0;
            _read_sample_last_dts = 0;
            checkNeedSeek();
            eof = false;
        } else {
            eof = true;
        }
    }
    if (eof && _file_repeat) {
        //需要从头开始看
        seekTo(0);
        return true;
    }
    return !eof;
}

void MultiMP4Reader::setCurrentStamp(uint32_t new_stamp) {
    auto old_stamp = getCurrentStamp();
    _seek_ticker.resetTime();
    TraceL << "MultiMP4Reader::setCurrentStamp:" << new_stamp;
}

bool MultiMP4Reader::seekTo(uint32_t stamp_seek) {
    lock_guard<recursive_mutex> lck(_mtx);
    if (stamp_seek > _demuxer->getDurationMS()) {
        //超过文件长度
        return false;
    }
    auto stamp = _demuxer->seekTo(stamp_seek);
    if (stamp == -1) {
        //seek失败
        return false;
    }

    if (!_have_video) {
        //没有视频，不需要搜索关键帧；设置当前时间戳
        setCurrentStamp((uint32_t) stamp);
        return true;
    }
    InfoL << "seek:" << stamp_seek;
    //搜索到下一帧关键帧
    bool keyFrame = false;
    bool eof = false;
    while (!eof) {
        auto frame = _demuxer->readFrame(keyFrame, eof);
        if (!frame) {
            //文件读完了都未找到下一帧关键帧
            continue;
        }
        if (keyFrame || frame->keyFrame() || frame->configFrame()) {
            auto frameFromPtr = std::dynamic_pointer_cast<FrameFromPtr>(frame);
            uint64_t currentDTS = frameFromPtr->dts();
            if(frameFromPtr) {
                frameFromPtr->setPTS(0);
                frameFromPtr->setDTS(0);
            }
            //定位到key帧
            if (_muxer) {
                _muxer->inputFrame(frame);
            }
            //设置当前时间戳
            setCurrentStamp(currentDTS);
            return true;
        }
    }
    return false;
}

bool MultiMP4Reader::pause(mediakit::MediaSource &sender, bool pause) {
    if (_paused == pause) {
        return true;
    }
    //_seek_ticker重新计时，不管是暂停还是seek都不影响总的播放进度
    setCurrentStamp(getCurrentStamp());
    _paused = pause;
    DebugL << getOriginUrl(sender) << ",pause:" << pause;
    return true;
}

bool MultiMP4Reader::speed(MediaSource &sender, float speed) {
    if (speed < 0.1 || speed > 20) {
        WarnL << "播放速度取值范围非法:" << speed;
        return false;
    }
    // 设置播放速度后应该恢复播放
    if (_speed == speed) {
        return true;
    }
       _paused = false;
     //_seek_ticker重置，赋值_seek_to
    setCurrentStamp(getCurrentStamp());

    _speed = speed;
    DebugL << getOriginUrl(sender) << ",speed:" << speed;
    return true;
}

bool MultiMP4Reader::seekTo(mediakit::MediaSource &sender, uint32_t stamp) {
    //拖动进度条后应该恢复播放
    pause(sender, false);
    DebugL << getOriginUrl(sender) << ",stamp:" << stamp;
    return seekTo(stamp);
}

bool MultiMP4Reader::readNextSample() {
    bool keyFrame = false;
    bool eof = false;
    auto frame = _demuxer->readFrame(keyFrame, eof);
    if (!frame) {
        return false;
    }
    if (_muxer) {
        _muxer->inputFrame(frame);
    }
    setCurrentStamp(frame->dts());
    DebugL << "readNextSample:" << frame->dts();
    return true;
}

bool MultiMP4Reader::close(mediakit::MediaSource &sender) {
    _timer = nullptr;
    WarnL << "close media: " << sender.getUrl();
    return true;
}

MediaOriginType MultiMP4Reader::getOriginType(MediaSource &sender) const {
    return MediaOriginType::mp4_vod;
}

uint32_t MultiMP4Reader::getCurrentStamp() {
    return (uint32_t) (!_paused * _speed * _seek_ticker.elapsedTime());
}

std::string MultiMP4Reader::getOriginUrl(mediakit::MediaSource &sender) const {
    return _file_path.at(_currentIndex).path;
}

toolkit::EventPoller::Ptr MultiMP4Reader::getOwnerPoller(mediakit::MediaSource &sender) {
    return _poller;
}

const MP4Demuxer::Ptr &MultiMP4Reader::getDemuxer() const {
    return _demuxer;
}

}

