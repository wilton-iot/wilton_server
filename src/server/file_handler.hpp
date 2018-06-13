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
 * File:   file_handler.hpp
 * Author: alex
 *
 * Created on May 11, 2016, 12:50 PM
 */

#ifndef WILTON_SERVER_FILE_HANDLER_HPP
#define WILTON_SERVER_FILE_HANDLER_HPP

#include <cstdint>
#include <memory>
#include <streambuf>
#include <string>

#include "staticlib/config.hpp"
#include "staticlib/io.hpp"
#include "staticlib/pion.hpp"
#include "staticlib/tinydir.hpp"
#include "staticlib/utils.hpp"

#include "wilton/support/exception.hpp"

#include "response_stream_sender.hpp"
#include "serverconf/document_root.hpp"

namespace wilton {
namespace server {

class file_handler {
    std::shared_ptr<serverconf::document_root> conf;
    
public:
    // must be copyable to satisfy std::function
    file_handler(const file_handler& other) :
    conf(other.conf) { }

    file_handler& operator=(const file_handler& other) {
        this->conf = other.conf;
        return *this;
    }

    // todo: path leading slash check
    file_handler(const serverconf::document_root& conf) :
    conf(std::make_shared<serverconf::document_root>(conf.clone())) {
        if (0 == this->conf->dirPath.length()) throw support::exception(TRACEMSG(
                "Invalid empty 'dirPath' specified"));
    }
    
    // todo: error messages format
    // todo: path checks
    void operator()(sl::pion::http_request_ptr req, sl::pion::response_writer_ptr resp) {
        std::string url_path = std::string{req->get_resource(), conf->resource.length()};
        if (url_path.find("..") != std::string::npos) {
            resp->get_response().set_status_code(sl::pion::http_request::RESPONSE_CODE_BAD_REQUEST);
            resp->get_response().set_status_message(sl::pion::http_request::RESPONSE_MESSAGE_BAD_REQUEST);
            resp->write(sl::support::to_string(sl::pion::http_request::RESPONSE_CODE_BAD_REQUEST));
            resp->write_nocopy(" ");
            resp->write_nocopy(sl::pion::http_request::RESPONSE_MESSAGE_BAD_REQUEST);
            resp->write_nocopy(": [");
            resp->write(url_path);
            resp->write_nocopy("]\n");
            resp->send(std::move(resp));
        } else {
            std::string file_path = std::string(conf->dirPath) +"/" + url_path;
            auto fd_opt = open_file_source(file_path);
            if (fd_opt.has_value()) {
                auto fd_ptr = sl::io::make_source_istream_ptr(std::move(fd_opt.value()));
                set_resp_headers(url_path, resp->get_response());
                auto sender = sl::support::make_unique<response_stream_sender>(std::move(resp), std::move(fd_ptr));
                sender->send(std::move(sender));
            } else {
                resp->get_response().set_status_code(sl::pion::http_request::RESPONSE_CODE_NOT_FOUND);
                resp->get_response().set_status_message(sl::pion::http_request::RESPONSE_MESSAGE_NOT_FOUND);
                resp->write(sl::support::to_string(sl::pion::http_request::RESPONSE_CODE_NOT_FOUND));
                resp->write_nocopy(" ");
                resp->write_nocopy(sl::pion::http_request::RESPONSE_MESSAGE_NOT_FOUND);
                resp->write_nocopy(": [");
                resp->write(url_path);
                resp->write_nocopy("]\n");
                resp->send(std::move(resp));
            }
        }
    }
    
private:
    void set_resp_headers(const std::string& url_path, sl::pion::http_response& resp) {
        std::string ct{"application/octet-stream"};
        for(const auto& mi : conf->mimeTypes) {
            if (sl::utils::ends_with(url_path, mi.extension)) {
                ct = mi.mime;
                break;
            }
        }
        resp.change_header("Content-Type", ct);
        // set caching
        resp.change_header("Cache-Control", "max-age=" + sl::support::to_string(conf->cacheMaxAgeSeconds) + ", public");
    }

    // todo: fixme
    static sl::support::optional<sl::tinydir::file_source> open_file_source(const std::string& file_path) {
        try {
            return sl::support::make_optional(sl::tinydir::file_source(file_path));
        } catch(const sl::tinydir::tinydir_exception&){
            return sl::support::optional<sl::tinydir::file_source>();
        }
    }
};

} // namespace
}

#endif /* WILTON_SERVER_FILE_HANDLER_HPP */

