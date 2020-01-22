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
 * File:   websocket.cpp
 * Author: alex
 * 
 * Created on January 20, 2020, 12:31 PM
 */
#include "websocket.hpp"

#include "staticlib/pion.hpp"
#include "staticlib/pimpl/forward_macros.hpp"

namespace wilton {
namespace server {

class websocket::impl : public staticlib::pimpl::object::impl {
    sl::pion::websocket_ptr ws;

public:
    impl(void* /* sl::pion::websocket_ptr* */ ws) :
    ws(std::move(*static_cast<sl::pion::websocket_ptr*> (ws))) { }

    void send(websocket&, sl::io::span<const char> data) {
        ws->write(data);
        ws->send(std::move(ws));
    }

    void close(websocket&) {
        ws->close(std::move(ws));
    }

};
PIMPL_FORWARD_CONSTRUCTOR(websocket, (void*), (), support::exception)
PIMPL_FORWARD_METHOD(websocket, void, send, (sl::io::span<const char>), (), support::exception)
PIMPL_FORWARD_METHOD(websocket, void, close, (), (), support::exception)

} // namespace
}
