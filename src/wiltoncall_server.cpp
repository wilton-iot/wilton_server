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
 * File:   wiltoncall_http.cpp
 * Author: alex
 *
 * Created on October 18, 2017, 9:16 PM
 */


#include <cstdint>
#include <string>

#include "staticlib/config.hpp"
#include "staticlib/support.hpp"

#include "wilton/support/registrar.hpp"

namespace wilton {

// Server
namespace server {

support::buffer server_create(sl::io::span<const char> data);

support::buffer server_stop(sl::io::span<const char> data);

support::buffer server_broadcast_websocket(sl::io::span<const char> data);

support::buffer request_get_metadata(sl::io::span<const char> data);

support::buffer request_get_data(sl::io::span<const char> data);

support::buffer request_get_form_data(sl::io::span<const char> data);

support::buffer request_get_data_filename(sl::io::span<const char> data);

support::buffer request_set_response_metadata(sl::io::span<const char> data);

support::buffer request_send_response(sl::io::span<const char> data);

support::buffer request_send_temp_file(sl::io::span<const char> data);

support::buffer request_send_mustache(sl::io::span<const char> data);

support::buffer request_send_later(sl::io::span<const char> data);

support::buffer request_close_websocket(sl::io::span<const char> data);

support::buffer request_set_metadata_with_response_writer(sl::io::span<const char> data);

support::buffer request_send_with_response_writer(sl::io::span<const char> data);

void initialize();

} // namespace
}

extern "C" char* wilton_module_init() {
    try {
        // server
        wilton::server::initialize();
        wilton::support::register_wiltoncall("server_create", wilton::server::server_create);
        wilton::support::register_wiltoncall("server_stop", wilton::server::server_stop);
        wilton::support::register_wiltoncall("server_broadcast_websocket", wilton::server::server_broadcast_websocket);
        wilton::support::register_wiltoncall("request_get_metadata", wilton::server::request_get_metadata);
        wilton::support::register_wiltoncall("request_get_data", wilton::server::request_get_data);
        wilton::support::register_wiltoncall("request_get_form_data", wilton::server::request_get_form_data);
        wilton::support::register_wiltoncall("request_get_data_filename", wilton::server::request_get_data_filename);
        wilton::support::register_wiltoncall("request_set_response_metadata", wilton::server::request_set_response_metadata);
        wilton::support::register_wiltoncall("request_send_response", wilton::server::request_send_response);
        wilton::support::register_wiltoncall("request_send_temp_file", wilton::server::request_send_temp_file);
        wilton::support::register_wiltoncall("request_send_mustache", wilton::server::request_send_mustache);
        wilton::support::register_wiltoncall("request_send_later", wilton::server::request_send_later);
        wilton::support::register_wiltoncall("request_close_websocket", wilton::server::request_close_websocket);
        wilton::support::register_wiltoncall("request_set_metadata_with_response_writer", wilton::server::request_set_metadata_with_response_writer);
        wilton::support::register_wiltoncall("request_send_with_response_writer", wilton::server::request_send_with_response_writer);
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}
