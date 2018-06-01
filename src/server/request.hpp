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
 * File:   request.hpp
 * Author: alex
 *
 * Created on May 5, 2016, 12:30 PM
 */

#ifndef WILTON_SERVER_REQUEST_HPP
#define WILTON_SERVER_REQUEST_HPP

#include <cstdint>
#include <functional>
#include <map>
#include <unordered_map>
#include <string>
#include <mutex>

#include "staticlib/pimpl.hpp"
#include "staticlib/json.hpp"

#include "wilton/support/exception.hpp"

#include "server/response_writer.hpp"
#include "serverconf/response_metadata.hpp"
#include "serverconf/request_metadata.hpp"

namespace wilton {
namespace server {

class server_mustache_cache {
    std::unordered_map<std::string, std::string> mustache_files_cache;
    std::mutex guardian;
public:
    server_mustache_cache(){}
    size_t count(const std::string& key);
    std::string get(const std::string& key);
    void set(const std::string& key, const std::string& value);
};

class request : public sl::pimpl::object {
protected:
    /**
     * implementation class
     */
    class impl;
public:
    /**
     * PIMPL-specific constructor
     * 
     * @param pimpl impl object
     */
    PIMPL_CONSTRUCTOR(request)
    
    serverconf::request_metadata get_request_metadata();
    
    const std::string& get_request_data();

    const std::map<std::string, std::string>& get_mustache_partials_data();

    server_mustache_cache* get_mustache_cache_ptr();

    sl::json::value get_request_form_data();
    
    const std::string& get_request_data_filename();
    
    void set_response_metadata(serverconf::response_metadata rm);
    
    void send_response(const char* data, uint32_t data_len);
    
    void send_file(std::string file_path, std::function<void(bool)> finalizer);
    
    void send_mustache(std::string mustache_file_path, sl::json::value json);
    
    response_writer send_later();
    
    void finish();
    
    // private api
    
    request(void* /* sl::pion::http_request_ptr&& */ req, 
            void* /* sl::pion::http_response_writer_ptr&& */ resp,
            const std::map<std::string, std::string>& mustache_partials,
            server_mustache_cache* mustache_cache);
};

} // namespace
}

#endif /* WILTON_SERVER_REQUEST_HPP */

