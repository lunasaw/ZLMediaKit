//
// Created by Hope liao on 2023/10/17.
//

#ifndef ZLMEDIAKIT_MULTIMP4READER_H
#define ZLMEDIAKIT_MULTIMP4READER_H

#include "MP4Demuxer.h"
#include "Common/MultiMediaSourceMuxer.h"
#include "MultiMediaSourceTuple.h"

#ifdef ENABLE_MP4
namespace mediakit {

class MultiMP4Reader
    :public std::enable_shared_from_this<MultiMP4Reader>, public MediaSourceEvent{
public:
    using Ptr = std::shared_ptr<MultiMP4Reader>;

    MultiMP4Reader(const std::string &vhost,
                   const std::string &app,
                   const std::string &stream_id,
                   const std::vector<MultiMediaSourceTuple> &file_path);
    ~MultiMP4Reader() override;

    /**
     * 开始解复用MP4文件
     * @param sample_ms 每次读取文件数据量，单位毫秒，置0时采用配置文件配置
     * @param ref_self 是否让定时器引用此对象本身，如果无其他对象引用本身，在不循环读文件时，读取文件结束后本对象将自动销毁
     * @param file_repeat 是否循环读取文件，如果配置文件设置为循环读文件，此参数无效
     */
    void startReadMP4(float fSpeed, uint64_t sample_ms = 0, bool ref_self = true,  bool file_repeat = false);

    /**
     * 停止解复用MP4定时器
     */
    void stopReadMP4();

    /**
     * 获取mp4解复用器
     */
    const MP4Demuxer::Ptr& getDemuxer() const;

private:
    bool loadMP4(int index);
    bool isPlayEof();
    void checkNeedSeek();
    bool readSample();
    bool readNextSample();
    void setCurrentStamp(uint32_t stamp);
    uint32_t getCurrentStamp();
    bool seekTo(uint32_t stamp_seek);
    bool seekTo(MediaSource &sender,uint32_t stamp) override;
    bool pause(MediaSource &sender, bool pause) override;
    bool speed(MediaSource &sender, float speed) override;

    bool close(MediaSource &sender) override;
    MediaOriginType getOriginType(MediaSource &sender) const override;
    std::string getOriginUrl(MediaSource &sender) const override;
    toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) override;
private:
    toolkit::EventPoller::Ptr                 _poller;
    std::vector<MultiMediaSourceTuple>        _file_path;
    MultiMediaSourceTuple                     _current_source_tuple;
    MP4Demuxer::Ptr                           _demuxer;
    MultiMediaSourceMuxer::Ptr                _muxer = nullptr;
    MediaTuple                                _tuple;
    toolkit::Ticker                           _seek_ticker;
    int                                       _currentIndex = 0;
    bool                                      _paused = false;
    bool                                      _have_video = false;
    bool                                      _read_mp4_item_done = false;
    float                                     _speed = 1.0;
    uint32_t                                  _last_dts = 0;
    uint32_t                                  _last_pts = 0;
    uint64_t                                  _capture_dts = 0;
    uint64_t                                  _capture_pts = 0;
    uint32_t                                  _seek_to = 0;
    uint32_t                                  _capture_seek_to = 0;
    bool                                      _file_repeat = false;
    toolkit::Timer::Ptr                       _timer;
    std::recursive_mutex                      _mtx;
    bool                                      _first_read = true;
};

}
#endif //ENABLE_MP4
#endif //ZLMEDIAKIT_MULTIMP4READER_H
