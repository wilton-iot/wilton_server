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
 * File:   handlers_common.hpp
 * Author: alex
 *
 * Created on February 12, 2020, 10:43 AM
 */

#ifndef WILTON_SERVER_HANDLERS_HANDLERS_COMMON_HPP
#define WILTON_SERVER_HANDLERS_HANDLERS_COMMON_HPP

#include <string>

#include "staticlib/config.hpp"
#include "staticlib/json.hpp"
#include "staticlib/pion.hpp"
#include "staticlib/support.hpp"
#include "staticlib/utils.hpp"

#include "wilton/support/exception.hpp"

#include "conf/mime_type.hpp"
#include "conf/document_root.hpp"

namespace wilton {
namespace server {
namespace handlers {

inline void set_response_headers(const server::conf::document_root& conf, 
        const std::string& url_path, sl::pion::http_response& resp) {
    auto ct = std::string("application/octet-stream");
    for (const auto& mi : conf.mimeTypes) {
        if (sl::utils::ends_with(url_path, mi.extension)) {
            ct = mi.mime;
            break;
        }
    }
    resp.change_header("Content-Type", ct);
    // set caching
    resp.change_header("Cache-Control", "max-age=" + sl::support::to_string(conf.cacheMaxAgeSeconds) + ", public");
}

void send400(sl::pion::response_writer_ptr resp, const std::string& url_path) {
    auto msg = sl::json::dumps({
        {"error", {
            { "code", sl::pion::http_request::RESPONSE_CODE_BAD_REQUEST },
            { "message", sl::pion::http_request::RESPONSE_MESSAGE_BAD_REQUEST },
            { "path", url_path }}}
    });
    resp->get_response().set_status_code(sl::pion::http_request::RESPONSE_CODE_NOT_FOUND);
    resp->get_response().set_status_message(sl::pion::http_request::RESPONSE_MESSAGE_NOT_FOUND);
    resp->write(msg);
    resp->send(std::move(resp));
}

void send404(sl::pion::response_writer_ptr resp, const std::string& url_path) {
    auto msg = sl::json::dumps({
        {"error", {
            { "code", sl::pion::http_request::RESPONSE_CODE_NOT_FOUND },
            { "message", sl::pion::http_request::RESPONSE_MESSAGE_NOT_FOUND },
            { "path", url_path }}}
    });
    resp->get_response().set_status_code(sl::pion::http_request::RESPONSE_CODE_NOT_FOUND);
    resp->get_response().set_status_message(sl::pion::http_request::RESPONSE_MESSAGE_NOT_FOUND);
    resp->write(msg);
    resp->send(std::move(resp));
}

} // namespace
}
}

#endif /* WILTON_SERVER_HANDLERS_HANDLERS_COMMON_HPP */

