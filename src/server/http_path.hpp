/*
 * Copyright 2016, alex at staticlibs.net
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
 * File:   http_path.hpp
 * Author: alex
 *
 * Created on December 10, 2016, 9:55 PM
 */

#ifndef WILTON_SERVER_HTTP_PATH_HPP
#define WILTON_SERVER_HTTP_PATH_HPP

#include <cstdint>
#include <string>
#include <functional>

#include "wilton/wilton.h"

#include "staticlib/config.hpp"
#include "staticlib/utils.hpp"

#include "wilton/support/exception.hpp"

#include "server/request.hpp"



namespace wilton {
namespace server {

namespace { // anonymous

namespace sc = staticlib::config;
namespace su = staticlib::utils;

} // namespace

class http_path {
public:
    std::string method;
    std::string path;
    std::function<void(request&)> handler;

    http_path(std::string method, std::string path, std::function<void(request&)> handler) :
    method(std::move(method)),
    path(std::move(path)),
    handler(handler) { }

    http_path(const http_path&) = delete;

    http_path& operator=(const http_path&) = delete;

    http_path(http_path&& other) :
    method(std::move(other.method)),
    path(std::move(other.path)),
    handler(other.handler) { }
 
    http_path& operator=(http_path&&) = delete;
};

} // namespace
}

#endif /* WILTON_SERVER_HTTP_PATH_HPP */

