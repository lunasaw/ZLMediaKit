//
//  FileScanner.cpp
//  Created by weidian on 2023/10/30.
//

#include "FileScanner.h"
#include <string>
namespace mediakit {

std::vector<std::string> Scanner::split(const std::string& input, const std::string& regex) {
    std::regex re(regex);
    std::sregex_token_iterator first {input.begin(), input.end(), re, -1}, last;
    return {first, last};
}

void Scanner::initInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info, bool  isStart) {
    std::vector<std::string> infos = split(info, seq);
    std::stringstream strstream;
    
    fn->hh = stoi(infos[0]);
    fn->mm = stoi(infos[1]);
    fn->ss = stoi(infos[2]);
    
    for(size_t i = 0; i < infos.size(); i++) {
        strstream << infos[i];
    }
    if(isStart)
        fn->stime = strstream.str();
    else
        fn->etime = strstream.str();
}

bool Scanner::initFileInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info,std::shared_ptr<Info> st,std::shared_ptr<Info> et) {
    std::vector<std::string> infos = split(info, seq);
    if(infos.size() < 2 || infos[0].empty())
        return false;
    fn->file_name = info;
    fn->stime = infos[0];
    fn->etime = infos[1];
    if(fn->stime <= st->stime && fn->etime >= et->etime){
        WarnL << "起始时间和结束时间在一个文件范围内!";
        return true;
    }else{
        return false;
    }
}

bool Scanner::time_compare_st(std::shared_ptr<Info> first, std::shared_ptr<Info> second) {
    if(first->stime.compare(second->stime) < 0)
        return true;
    return false;
}

std::vector<std::shared_ptr<Scanner::Info>> Scanner::getMediaInfo(std::string dir_path, const std::string start_time, const std::string end_time) {
    DIR *dir;
    struct dirent *diread;
    std::vector<std::shared_ptr<Info>> myfiles;
    std::string seq = " +";
    std::vector<std::string> start_time_ans;
    std::vector<std::string> end_time_ans;
    bool isOneFile;
    int start_hh;
    int start_mm;
    int start_ss;
    int end_hh;
    int end_mm;
    int end_ss;

    WarnL << "视频文件目录路径:"<<dir_path<<" 开始时间:"<<start_time<<" 结束时间:"<<end_time;

    start_time_ans = split(start_time, seq);
    if(end_time.empty() || end_time == "\"\"") {
        end_time_ans.push_back(std::string(start_time_ans[0]));
        end_time_ans.push_back(std::string("23:59:59"));
    } else {
        end_time_ans = split(end_time, seq);
    }

    if(start_time_ans[0] != end_time_ans[0])
        return {};
        
    std::shared_ptr<Info> st = std::make_shared<Info>();
    std::shared_ptr<Info> et = std::make_shared<Info>();
    seq = ":+";
    initInfo(st, seq, start_time_ans[1], true);//st为起始时间
    initInfo(et, seq, end_time_ans[1], false);//et为结束时间
        
    std::string full_path = dir_path.append("/" + start_time_ans[0]);
    seq = "[-.]+";
    if ((dir = opendir(full_path.data())) != nullptr) {
        while ((diread = readdir(dir)) != nullptr ) {
            std::shared_ptr<Info> fn = std::make_shared<Info>();
            isOneFile = initFileInfo(fn, seq, diread->d_name, st, et);
            if(!fn->file_name.empty() && et->etime > fn->stime && st->stime < fn->etime) {//
                if(st->stime > fn->stime && st->stime < fn->etime) {
                    start_hh = stoi(fn->stime.substr(0,2));//起始时间
                    start_mm = stoi(fn->stime.substr(2,2));
                    start_ss = stoi(fn->stime.substr(4,2));
                    fn->start_shift = ((st->hh - start_hh) * 3600 + (st->mm - start_mm)*60 + (st->ss - start_ss)) * 1000;
                }
                
                if(et->etime < fn->etime && et->etime > fn->stime) {
                    end_hh = stoi(fn->etime.substr(0,2));//结束时间
                    end_mm = stoi(fn->etime.substr(2,2));
                    end_ss = stoi(fn->etime.substr(4,2));
                    fn->end_shift = ((end_hh - et->hh) * 3600 + (end_mm - et->mm)*60 + (end_ss - et->ss)) * 1000;

                    if(isOneFile){
                        fn->end_shift = ((et->hh - start_hh) * 3600 + (et->mm - start_mm)*60 + (et->ss - start_ss)) * 1000;
                    }
                }
                if(full_path.back() != '/')
                    full_path += '/';
                fn->file_name = full_path + fn->file_name;
                WarnL << "视频文件全路径为:"<<fn->file_name << " start_shift = " <<fn->start_shift << " end_shift = " <<fn->end_shift;
                myfiles.push_back(fn);
            }
        }
        closedir(dir);
    } else {
        WarnL << "无法打开视频文件目录路径,路径为："<<full_path;
        return {};
    }
    sort(myfiles.begin(), myfiles.end(), time_compare_st);
    return myfiles;
}

std::vector<MultiMediaSourceTuple> Scanner::getMST(std::string dir_path, const std::string start_time, const std::string end_time) {
    std::vector<std::shared_ptr<Info>> files = getMediaInfo(dir_path, start_time, end_time);
    for(auto a : files)
        std::cout<<a->file_name<<std::endl;
    std::vector<MultiMediaSourceTuple> vec = {};
    for(auto file : files){
        MultiMediaSourceTuple mst;
        mst.path = file->file_name;
        mst.startMs = file->start_shift;
        mst.endMs = file->end_shift;
        vec.push_back(mst);
    }
    return vec;
}


std::string IPAddress::getIP(const std::string &url) {
    std::regex ipRegex(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
    std::smatch match;

     if (std::regex_search(url, match, ipRegex)) {
        std::string ip = match[1].str();
        return ip;
     }
     return "";
}

bool IPAddress::isIPReachable(const std::string &ip) {
    if (!ip.empty()) {
        std::string command = "ping -c 1 " + ip;
        int result = std::system(command.c_str());
        InfoL << "ping:" << command << ",result:" << !result;
        // 根据返回值判断是否可联通
        if (result == 0) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

}

