//
//  FileScanner.cpp
//  Created by weidian on 2023/10/30.
//

#include "FileScanner.h"
#include <string>
namespace mediakit {

struct Date
{
    int year;
    int month;
    int day;
    std::string start_shift;
    std::string end_shift;

    friend Date operator++(Date &x);
    friend bool operator<=(Date &x, Date &y);
    friend bool operator==(Date &x, Date &y);
    friend bool operator<(Date &x, Date &y);
};

bool isleap_year(int year)
{
    if ((year % 400 == 0) || ((year % 100 != 0) && (year % 4 == 0)))
        return true;
    return false;
}


int days_month(int year, int m)
{
    int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (isleap_year(year))
        days[1] = 29;
    return days[m - 1];
}

Date operator++(Date &x)
{
    int days;
    days = days_month(x.year, x.month);
    if (x.day < days)
        x.day++;
    else
    {
        if (x.month == 12)
        {
            x.day = 1;
            x.month = 1;
            x.year++;
        }
        else
        {
            x.day = 1;
            x.month++;
        }
    }
    x.start_shift =  "00:00:00";
    x.end_shift = "24:02:00";
    return x;
}

bool operator<=(Date &x, Date &y)
{
    if (x.year < y.year)
        return true;
    if ((x.year == y.year) && (x.month < y.month))
        return true;
    if ((x.year == y.year) && (x.month == y.month)&&(x.day <= y.day))
        return true;
    return false;
}

bool operator<(Date &x, Date &y)
{
    if (x.year < y.year)
        return true;
    if ((x.year == y.year) && (x.month < y.month))
        return true;
    if ((x.year == y.year) && (x.month == y.month)&&(x.day < y.day))
        return true;
    return false;
}

bool operator ==(Date &x, Date &y)
{
    if ((x.year == y.year) && (x.month == y.month)&&(x.day == y.day))
        return true;
    return false;
}

std::vector<std::string> Scanner::split(const std::string& input, const std::string& regex) 
{
    std::regex re(regex);
    std::sregex_token_iterator first {input.begin(), input.end(), re, -1}, last;
    return {first, last};
}

void Scanner::initInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info, bool  isStart) 
{
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

bool Scanner::initFileInfo(std::shared_ptr<Info>& fn, const std::string seq, const std::string info,std::shared_ptr<Info> st,std::shared_ptr<Info> et) 
{
    std::vector<std::string> infos = split(info, seq);
    if(infos.size() < 2 || infos[0].empty())
        return false;
    fn->file_name = info;
    fn->stime = infos[0];
    fn->etime = infos[1];

    if(fn->stime > fn->etime ){
        WarnL << "起始时间和结束时间不在一天范围内!";
        return false;
    }else{
        return true;
    }
}

static bool time_compare_st(std::shared_ptr<Scanner::Info> first, std::shared_ptr<Scanner::Info> second) 
{
    if(first->stime.compare(second->stime) < 0)
        return true;
    return false;
}

int Scanner::calcShift(std::string time, int hour, int minute, int second) 
{
    int h = 0;
    int m = 0;
    int s = 0;
    if(!time.empty()){
        h = stoi(time.substr(0,2));
        m = stoi(time.substr(2,2));
        s = stoi(time.substr(4,2));
    }
    return ((hour - h) * 3600 + (minute - m)*60 + (second - s)) * 1000;
}

void Scanner::genNameVec(std::string full_path, std::shared_ptr<Info>& fn, std::vector<std::shared_ptr<Info>>& myfiles) 
{
    if(full_path.back() != '/')
        full_path += '/';
        fn->file_name = full_path + fn->file_name;
        WarnL << "视频文件全路径为:"<<fn->file_name << " start_shift = " <<fn->start_shift << " end_shift = " <<fn->end_shift;
        myfiles.push_back(fn);  
}

std::vector<std::string> Scanner::getAllNediaInfo(std::string dir_path, const std::string start_time,const std::string end_time) 
{
    std::string seq = " +";
    std::vector<std::string> start_time_ans;
    std::vector<std::string> end_time_ans;
    std::vector<std::string> date;
    std::stringstream strstream;
    Date start_date;
    Date end_date;

    start_time_ans = split(start_time, seq);
    end_time_ans = split(end_time, seq);
    seq = "-+";
    date = split(start_time_ans[0], seq);
    start_date.year = stoi(date[0]);
    start_date.month = stoi(date[1]);
    start_date.day = stoi(date[2]);
    start_date.start_shift = start_time_ans[1];

    date = split(end_time_ans[0], seq);
    end_date.year = stoi(date[0]);
    end_date.month = stoi(date[1]);
    end_date.day = stoi(date[2]);
    end_date.end_shift = end_time_ans[1];

    if(start_date.year == end_date.year &&
       start_date.month == end_date.month &&
       (start_date.day == end_date.day - 1 || 
        start_date.day == end_date.day)) {
        getMediaInfo(dir_path, start_time, end_time);
    } else {
        while(start_date <= end_date) {        
            WarnL<<strstream.str();
            if(start_date == end_date) {
                strstream<<end_time_ans[0]<<" "<<start_date.start_shift;
                std::string last_day_start_time = strstream.str();
                getMediaInfo(dir_path,last_day_start_time, end_time);
            } else {
                strstream<<start_date.year<<"-"<<start_date.month<<"-"<<start_date.day<<" "<<start_date.start_shift;
                std::string day_start_time = strstream.str();
                strstream<<start_date.year<<"-"<<start_date.month<<"-"<<start_date.day<<" "<<start_date.end_shift;
                std::string day_end_time = strstream.str();
                getMediaInfo(dir_path, day_start_time, day_end_time);
            } 
            strstream.str("");
            ++start_date;
        }
    }
    return {};
}

void Scanner::getInfo(const std::string start_time, const std::string end_time) 
{
    std::string seq = " +";
    std::vector<std::string> start_time_ans;
    std::vector<std::string> end_time_ans;
    std::vector<std::string> date;
    std::stringstream strstream;
    Date start_date;
    Date end_date;

    start_time_ans = split(start_time, seq);
    if(end_time != "\"\"") {
        end_time_ans = split(end_time, seq);
    } else {
        end_time_ans[0] = start_time_ans[0];
    }
    
    seq = "-+";
    date = split(start_time_ans[0], seq);
    start_date.year = stoi(date[0]);
    start_date.month = stoi(date[1]);
    start_date.day = stoi(date[2]);
    start_date.start_shift = start_time_ans[1];

    date = split(end_time_ans[0], seq);
    end_date.year = stoi(date[0]);
    end_date.month = stoi(date[1]);
    end_date.day = stoi(date[2]);
    end_date.end_shift = end_time_ans[1];
    std::vector<Date> date_vec;
    while(start_date <= end_date) {
        date_vec.push_back(start_date);
        ++start_date;
    }

    for(auto a:date_vec) {
        WarnL<<a.year<<"-"<<a.month<<"-"<<a.day<<" "<<a.start_shift;
        strstream<<a.year<<"-"<<a.month<<"-"<<a.day;
        std::string folder_name = strstream.str();
        strstream.str("");
        std::vector<std::string> files = folder_map[folder_name];
        for(auto a : files) {
            WarnL<<a;
        }
    }

}

std::vector<std::shared_ptr<Scanner::Info>> Scanner::getMediaInfo(std::string dir_path, const std::string start_time, const std::string end_time) 
{
    DIR *dir;
    struct dirent *diread;
    std::vector<std::shared_ptr<Info>> myfiles;
    std::string seq = " +";
    std::vector<std::string> start_time_ans;
    std::vector<std::string> end_time_ans;
    std::vector<std::string> date;
    bool isInOneDay;
    int start_hh;
    int start_mm;
    int start_ss;
    int end_hh;
    int end_mm;
    int end_ss;
    
    WarnL << "视频文件目录路径:"<<dir_path<<" 开始时间:"<<start_time<<" 结束时间:"<<end_time;
    // getAllNediaInfo(dir_path, start_time, end_time);

    
    start_time_ans = split(start_time, seq);
    if(end_time.empty() || end_time == "\"\"") {
        end_time_ans.push_back(std::string(start_time_ans[0]));
        end_time_ans.push_back(std::string("24:02:00"));
        
    } else {
        end_time_ans = split(end_time, seq);
    }

    std::shared_ptr<Info> st = std::make_shared<Info>();
    std::shared_ptr<Info> et = std::make_shared<Info>();
    seq = ":+";
    initInfo(st, seq, start_time_ans[1], true);//st为起始时间
    initInfo(et, seq, end_time_ans[1], false);//et为结束时间
        
    std::string full_path = dir_path.append("/" + start_time_ans[0]);
    seq = "[-.]+";
    if ((dir = opendir(full_path.data())) != nullptr) {
        while ((diread = readdir(dir)) != nullptr ) {
            if(!strcmp(diread->d_name , ".") || !strcmp(diread->d_name , ".."))
                continue;
            std::shared_ptr<Info> fn = std::make_shared<Info>();
            isInOneDay = initFileInfo(fn, seq, diread->d_name, st, et);
            if(isInOneDay){
                if(!fn->file_name.empty() && et->etime > fn->stime && st->stime < fn->etime) {//
                    if(st->stime > fn->stime && st->stime < fn->etime) {//起始时间
                        fn->start_shift = calcShift(fn->stime, st->hh, st->mm, st->ss);
                    }
                    
                    if(et->etime < fn->etime && et->etime > fn->stime) {//结束时间
                        fn->start_shift = calcShift(fn->stime, et->hh, et->mm, et->ss);
                    }

                    genNameVec(full_path, fn, myfiles);
                }

            } else {    
                fn->etime.replace(0, 2, "24");
                bool isFind = false;
                if(!fn->stime.empty() && st->stime > fn->stime ){
                    fn->start_shift = calcShift(fn->stime, st->hh, st->mm, st->ss);
                    isFind = true;
                } else if(st->stime == "000000"){
                    isFind = true;
                }
                //TODO
               
                et->etime.replace(0, 2, "24");
                if(et->etime < fn->etime) { 
                    int h = stoi(fn->stime.substr(0,2));
                    int m = stoi(fn->stime.substr(2,2));
                    int s = stoi(fn->stime.substr(4,2));
                    fn->end_shift = calcShift("", (et->hh + 23 - h), (et->mm + 59 - m), (et->ss +59 - s));
                    isFind = true;
                }
                et->etime.replace(0, 2, "00");
                fn->etime.replace(0, 2, "00");
                if(isFind)
                    genNameVec(full_path, fn, myfiles);
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

