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
    MultiMediaSourceTuple(std::string &filePath, uint32_t start, uint32_t end) {
        path = filePath;
        startMs = start;
        endMs = end;
    }
    MultiMediaSourceTuple(const char *filePath, uint32_t start, uint32_t end) {
        path = filePath;
        startMs = start;
        endMs = end;
    }
    ~MultiMediaSourceTuple() = default;
    std::string path;
    uint32_t startMs;
    uint32_t endMs;
};
}

#endif // ZLMEDIAKIT_MULTIMEDIASOURCETUPLE_H
