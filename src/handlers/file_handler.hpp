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

#ifndef WILTON_SERVER_HANDLERS_FILE_HANDLER_HPP
#define WILTON_SERVER_HANDLERS_FILE_HANDLER_HPP

#include <cstdint>
#include <memory>
#include <streambuf>
#include <string>

#include "staticlib/config.hpp"
#include "staticlib/json.hpp"
#include "staticlib/io.hpp"
#include "staticlib/pion.hpp"
#include "staticlib/tinydir.hpp"
#include "staticlib/utils.hpp"

#include "wilton/support/exception.hpp"

#include "conf/document_root.hpp"
#include "handlers/handlers_common.hpp"
#include "response_stream_sender.hpp"

namespace wilton {
namespace server {
namespace handlers {

namespace { // anonymous

sl::support::optional<sl::tinydir::file_source> open_file_source(const std::string& file_path) {
    try {
        auto res = sl::support::make_optional(sl::tinydir::file_source(file_path));
        // directory can be opened as a source, but cannot be read from
        auto pa = sl::tinydir::path(file_path);
        if (!pa.is_directory()) {
            return res;
        }
        return sl::support::optional<sl::tinydir::file_source>();
    } catch(const sl::tinydir::tinydir_exception&){
        return sl::support::optional<sl::tinydir::file_source>();
    }
}

} // namespace

class file_handler {
    std::shared_ptr<server::conf::document_root> conf;

public:
    // must be copyable to satisfy std::function
    file_handler(const file_handler& other) :
    conf(other.conf) { }

    file_handler& operator=(const file_handler& other) {
        this->conf = other.conf;
        return *this;
    }

    file_handler(const server::conf::document_root& conf) :
    conf(std::make_shared<server::conf::document_root>(conf.clone())) {
        if (0 == this->conf->dirPath.length()) throw support::exception(TRACEMSG(
                "Invalid empty 'dirPath' specified"));
    }

    // note: it may be better to add some pre-checks to the supplied file path
    // before providing it to FS
    void operator()(sl::pion::http_request_ptr req, sl::pion::response_writer_ptr resp) {
        std::string url_path = std::string{req->get_resource(), conf->resource.length()};
        if (url_path.find("..") != std::string::npos) {
            send400(std::move(resp), url_path);
        } else {
            std::string file_path = std::string(conf->dirPath) + "/" + url_path;
            auto fd_opt = open_file_source(file_path);
            if (fd_opt.has_value()) {
                auto fd_ptr = sl::io::make_source_istream_ptr(std::move(fd_opt.value()));
                set_response_headers(*conf, url_path, resp->get_response());
                auto sender = sl::support::make_unique<response_stream_sender>(std::move(resp), std::move(fd_ptr));
                sender->send(std::move(sender));
            } else {
                send404(std::move(resp), url_path);
            }
        }
    }

};

} // namespace
}
}

#endif /* WILTON_SERVER_HANDLERS_FILE_HANDLER_HPP */

