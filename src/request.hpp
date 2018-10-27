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
#include <string>

#include "staticlib/pimpl.hpp"
#include "staticlib/io.hpp"
#include "staticlib/json.hpp"

#include "wilton/support/buffer.hpp"
#include "wilton/support/exception.hpp"

#include "conf/response_metadata.hpp"
#include "conf/request_metadata.hpp"
#include "mustache_cache.hpp"
#include "response_writer.hpp"

namespace wilton {
namespace server {

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

    support::buffer get_request_data_buffer();

    sl::json::value get_request_form_data();
    
    const std::string& get_request_data_filename();
    
    void set_response_metadata(serverconf::response_metadata rm);
    
    void send_response(sl::io::span<const char> data);
    
    void send_file(std::string file_path, std::function<void(bool)> finalizer);
    
    void send_mustache(std::string mustache_file_path, sl::json::value json);
    
    response_writer send_later();
    
    void finish();

    bool is_websocket();

    void close_websocket();
    
    // private api
    
    request(void* /* sl::pion::http_request_ptr&& */ req, 
            void* /* sl::pion::http_response_writer_ptr&& */ resp,
            mustache_cache& mustache_templates,
            const std::map<std::string, std::string>& mustache_partials);

    request(void* /* sl::pion::websocket_ptr&& */ ws, bool response_allowed = true);
};

} // namespace
}

#endif /* WILTON_SERVER_REQUEST_HPP */

