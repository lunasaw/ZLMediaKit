//
//  FileScanner.cpp
//  Created by weidian on 2023/10/30.
//

#include "FileScanner.h"

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

void Scanner::initFileInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info) {
    std::vector<std::string> infos = split(info, seq);
    if(infos.size() < 2 || infos[0].empty())
        return;
    fn->file_name = info;
    fn->stime = infos[0];
    fn->etime = infos[1];
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
        
    std::vector<std::string> start_time_ans = split(start_time, seq);
    std::vector<std::string> end_time_ans = split(end_time, seq);
    if(start_time_ans[0] != end_time_ans[0])
        return {};
        
    std::shared_ptr<Info> st = std::make_shared<Info>();
    std::shared_ptr<Info> et = std::make_shared<Info>();
    seq = ":+";
    initInfo(st, seq, start_time_ans[1], true);
    initInfo(et, seq, end_time_ans[1], false);
        
    std::string full_path = dir_path.append("/" + start_time_ans[0]);
    seq = "[-.]+";
    if ((dir = opendir(full_path.data())) != nullptr) {
        while ((diread = readdir(dir)) != nullptr ) {
            std::shared_ptr<Info> fn = std::make_shared<Info>();
            initFileInfo(fn, seq, diread->d_name);
            if(!fn->file_name.empty() && et->etime > fn->stime && st->stime < fn->etime) {
                if(st->stime > fn->stime && st->stime < fn->etime) {
                    int hh = stoi(fn->stime.substr(0,2));
                    int mm = stoi(fn->stime.substr(2,2));
                    int ss = stoi(fn->stime.substr(4,2));
                    fn->start_shift = ((st->hh - hh) * 3600 + (st->mm - mm)*60 + (st->ss - ss)) * 1000;
                }
                
                if(et->etime < fn->etime && et->etime > fn->stime) {
                    int hh = stoi(fn->etime.substr(0,2));
                    int mm = stoi(fn->etime.substr(2,2));
                    int ss = stoi(fn->etime.substr(4,2));
                    fn->end_shift = ((hh - et->hh) * 3600 + (mm - et->mm)*60 + (ss - et->ss)) * 1000;
                }
                if(full_path.back() != '/')
                    full_path += '/';
                fn->file_name = full_path + fn->file_name;
                myfiles.push_back(fn);
            }
        }
        closedir(dir);
    } else {
        return {};
    }
    sort(myfiles.begin(), myfiles.end(), time_compare_st);
    return myfiles;
}

std::vector<MultiMediaSourceTuple> Scanner::getMST(std::string dir_path, const std::string start_time, const std::string end_time) {
    std::vector<std::shared_ptr<Info>> files = getMediaInfo(dir_path, start_time, end_time);
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


}

