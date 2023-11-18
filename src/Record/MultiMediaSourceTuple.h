//
// Created by Hope liao on 2023/10/26.
//

#ifndef ZLMEDIAKIT_MULTIMEDIASOURCETUPLE_H
#define ZLMEDIAKIT_MULTIMEDIASOURCETUPLE_H

#include <string>

namespace mediakit {

struct MultiMediaSourceTuple {
    MultiMediaSourceTuple() {
        path = "";
        startMs = 0;
        endMs = 0;
    }
    MultiMediaSourceTuple(std::string &filePath, uint64_t start, uint64_t end) {
        path = filePath;
        startMs = start;
        endMs = end;
    }
    MultiMediaSourceTuple(const char *filePath, uint64_t start, uint64_t end) {
        path = filePath;
        startMs = start;
        endMs = end;
    }
    ~MultiMediaSourceTuple() = default;
    std::string path;
    uint64_t startMs;
    uint64_t endMs;
};
}

#endif // ZLMEDIAKIT_MULTIMEDIASOURCETUPLE_H
