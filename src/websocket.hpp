/*
 * Copyright 2020, alex at staticlibs.net
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
 * File:   websocket.hpp
 * Author: alex
 *
 * Created on January 20, 2020, 12:31 PM
 */
#ifndef WILTON_SERVER_WEBSOCKET_HPP
#define WILTON_SERVER_WEBSOCKET_HPP

#include <cstdint>

#include "staticlib/io.hpp"
#include "staticlib/pimpl.hpp"

#include "wilton/support/exception.hpp"

#include "conf/response_metadata.hpp"

namespace wilton {
namespace server {

class websocket : public sl::pimpl::object {
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
    PIMPL_CONSTRUCTOR(websocket)

    void send(sl::io::span<const char> data);

    void close();

    // private api
    websocket(void* /* sl::pion::websocket_ptr&& */ ws);
};

} // namespace
}

#endif /* WILTON_SERVER_WEBSOCKET_HPP */
