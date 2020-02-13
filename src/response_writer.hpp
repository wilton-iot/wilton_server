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
 * File:   response_writer.hpp
 * Author: alex
 *
 * Created on June 19, 2016, 9:43 PM
 */

#ifndef WILTON_SERVER_RESPONSE_WRITER_HPP
#define WILTON_SERVER_RESPONSE_WRITER_HPP

#include <cstdint>

#include "staticlib/io.hpp"
#include "staticlib/pimpl.hpp"

#include "wilton/support/exception.hpp"

#include "conf/response_metadata.hpp"

namespace wilton {
namespace server {

class response_writer : public sl::pimpl::object {
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
    PIMPL_CONSTRUCTOR(response_writer)

    void set_metadata(server::conf::response_metadata rm);
    
    void send(sl::io::span<const char> data);
  
    // private api
    response_writer(void* /* sl::pion::http_response_writer_ptr&& */ writer);
};

} // namespace
}

#endif /* WILTON_SERVER_RESPONSE_WRITER_HPP */

