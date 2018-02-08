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
 * File:   response_writer.cpp
 * Author: alex
 * 
 * Created on June 19, 2016, 9:49 PM
 */

#include "server/response_writer.hpp"

#include "staticlib/pion.hpp"
#include "staticlib/pimpl/forward_macros.hpp"

namespace wilton {
namespace server {

class response_writer::impl : public staticlib::pimpl::object::impl {
    sl::pion::http_response_writer_ptr writer;

public:
    impl(void* /* sl::pion::http_response_writer_ptr* */ writer) :
    writer(std::move(*static_cast<sl::pion::http_response_writer_ptr*> (writer))) { }    
    
    void send(response_writer&, const char* data, uint32_t data_len) {
        writer->write(data, data_len);
        writer->send();
    }
    
};
PIMPL_FORWARD_CONSTRUCTOR(response_writer, (void*), (), support::exception)
PIMPL_FORWARD_METHOD(response_writer, void, send, (const char*)(uint32_t), (), support::exception)

} // namespace
}
