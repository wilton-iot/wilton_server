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
 * File:   loader_handler.hpp
 * Author: alex
 *
 * Created on February 12, 2020, 10:42 AM
 */

#ifndef WILTON_SERVER_HANDLERS_LOADER_HANDLER_HPP
#define WILTON_SERVER_HANDLERS_LOADER_HANDLER_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "staticlib/support.hpp"

#include "wilton/wilton.h"
#include "wilton/wilton_loader.h"
#include "wilton/support/exception.hpp"

#include "conf/document_root.hpp"
#include "handlers/handlers_common.hpp"

namespace wilton {
namespace server {
namespace handlers {

class loader_handler {
    std::shared_ptr<server::conf::document_root> conf;

public:
    // must be copyable to satisfy std::function
    loader_handler(const loader_handler& other) :
    conf(other.conf) { }

    loader_handler& operator=(const loader_handler& other) {
        this->conf = other.conf;
        return *this;
    }

    loader_handler(const server::conf::document_root& conf) :
    conf(std::make_shared<server::conf::document_root>(conf.clone())) { }

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
        auto url_path = conf->resourceLoaderPrefix + path;

        char* loaded = nullptr;
        int loaded_len = 0;
        auto err = wilton_load_resource(url_path.c_str(), static_cast<int>(url_path.length()),
                std::addressof(loaded), std::addressof(loaded_len));
        if (nullptr != err) {
            wilton_free(err);
            send404(std::move(resp), url_path);
            return;
        }
        auto deferred = sl::support::defer([loaded] () STATICLIB_NOEXCEPT {
            wilton_free(loaded);
        });

        resp->write({loaded, loaded_len});
        resp->send(std::move(resp));
    }

};

} // namespace
}
}


#endif /* WILTON_SERVER_HANDLERS_LOADER_HANDLER_HPP */

