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
 * File:   HttpServer.hpp
 * Author: alex
 *
 * Created on May 5, 2016, 12:30 PM
 */

#ifndef WILTON_SERVER_SERVER_HPP
#define WILTON_SERVER_SERVER_HPP

#include <set>
#include <vector>

#include "staticlib/pimpl.hpp"

#include "conf/server_config.hpp"
#include "http_path.hpp"
#include "request.hpp"

namespace wilton {
namespace server {

class sserver : public sl::pimpl::object {
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
    PIMPL_CONSTRUCTOR(sserver)
            
    sserver(server::conf::server_config conf, std::vector<sl::support::observer_ptr<http_path>> paths);
    
    void stop();
 
    void broadcast_websocket(const std::string& path, sl::io::span<const char> message,
            const std::set<std::string>& dest_ids);

    uint16_t get_tcp_port();
};

} // namespace
}

#endif /* WILTON_SERVER_SERVER_HPP */

