/*
 * Copyright 2018, alex at staticlibs.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 
 * File:   mustache_cache.hpp
 * Author: alex
 *
 * Created on October 26, 2018, 11:58 PM
 */

#ifndef WILTON_SERVER_MUSTACHE_CACHE_HPP
#define WILTON_SERVER_MUSTACHE_CACHE_HPP

#include <map>
#include <mutex>
#include <string>
#include <utility>

#include "staticlib/io.hpp"
#include "staticlib/tinydir.hpp"

namespace wilton {
namespace server {

class mustache_cache {
    std::mutex mutex;
    std::map<std::string, std::string> cache;

public:
    mustache_cache() { }

    mustache_cache(const mustache_cache&) = delete;

    mustache_cache& operator=(const mustache_cache&) = delete;

    const std::string& get(const std::string& file_path) {
        std::lock_guard<std::mutex> guard{mutex};
        auto it = cache.find(file_path);
        if (it != cache.end()) {
            return it->second;
        } else {
            auto src = sl::tinydir::file_source(file_path);
            auto sink = sl::io::string_sink();
            sl::io::copy_all(src, sink);
            auto pa = cache.insert(std::pair<std::string, std::string>(file_path, std::move(sink.get_string())));
            auto& res = pa.first->second;
            return res;
        }
    }
};

} // namespace
}

#endif /* WILTON_SERVER_MUSTACHE_CACHE_HPP */

