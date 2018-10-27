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
 * File:   zip_handler.hpp
 * Author: alex
 *
 * Created on May 11, 2016, 12:50 PM
 */

#ifndef WILTON_SERVER_ZIP_HANDLER_HPP
#define WILTON_SERVER_ZIP_HANDLER_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "staticlib/config.hpp"
#include "staticlib/io.hpp"
#include "staticlib/unzip.hpp"

#include "wilton/support/exception.hpp"

#include "conf/document_root.hpp"
#include "response_stream_sender.hpp"

namespace wilton {
namespace server {

namespace { // anonymous

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

class zip_handler {
    std::shared_ptr<serverconf::document_root> conf;
    std::shared_ptr<sl::unzip::file_index> idx;

public:
    // must be copyable to satisfy std::function
    zip_handler(const zip_handler& other) :
    conf(other.conf),
    idx(other.idx) { }

    zip_handler& operator=(const zip_handler& other) {
        this->conf = other.conf;
        this->idx = other.idx;
        return *this;
    }

    zip_handler(const serverconf::document_root& conf) :
    conf(std::make_shared<serverconf::document_root>(conf.clone())),
    idx(std::make_shared<sl::unzip::file_index>(conf.zipPath)) { }

    void operator()(sl::pion::http_request_ptr req, sl::pion::response_writer_ptr resp) {
        if (req->get_resource().length() < conf->resource.length()) {
            send404(std::move(resp), req->get_resource());
            return;
        }
        auto path = std::string(req->get_resource(), conf->resource.length());
        if (0 == path.length()) {
            send404(std::move(resp), req->get_resource());
            return;
        }
        if ('/' == path.front()) {
            path = path.substr(1);
        }
        std::string url_path = conf->zipInnerPrefix + path;
        sl::unzip::file_entry en = idx->find_zip_entry(url_path);
        if (en.is_empty()) {
            send404(std::move(resp), url_path);
            return;
        }
        auto stream_ptr = sl::unzip::open_zip_entry(*idx, url_path);
        set_resp_headers(url_path, resp->get_response());
        auto sender = sl::support::make_unique<response_stream_sender>(std::move(resp), std::move(stream_ptr));
        sender->send(std::move(sender));
    }

private:
    void set_resp_headers(const std::string& url_path, sl::pion::http_response& resp) {
        auto ct = std::string("application/octet-stream");
        for (const auto& mi : conf->mimeTypes) {
            if (sl::utils::ends_with(url_path, mi.extension)) {
                ct = mi.mime;
                break;
            }
        }
        resp.change_header("Content-Type", ct);
        // set caching
        resp.change_header("Cache-Control", "max-age=" + sl::support::to_string(conf->cacheMaxAgeSeconds) + ", public");
    }

};

} // namespace
}

#endif /* WILTON_SERVER_ZIP_HANDLER_HPP */
