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
 * File:   response_metadata.hpp
 * Author: alex
 *
 * Created on May 5, 2016, 7:21 PM
 */

#ifndef WILTON_SERVER_CONF_RESPONSE_METADATA_HPP
#define WILTON_SERVER_CONF_RESPONSE_METADATA_HPP

#include <cstdint>
#include <string>

#include "staticlib/config.hpp"
#include "staticlib/ranges.hpp"
#include "staticlib/json.hpp"

#include "wilton/support/exception.hpp"

#include "conf/header.hpp"

namespace wilton {
namespace serverconf {

class response_metadata {
public:
    uint16_t statusCode = 200;
    std::string statusMessage = "OK";
    std::vector<serverconf::header> headers;

    response_metadata(const response_metadata&) = delete;

    response_metadata& operator=(const response_metadata&) = delete;

    response_metadata(response_metadata&& other) :
    statusCode(other.statusCode),
    statusMessage(std::move(other.statusMessage)),
    headers(std::move(other.headers)) { }

    response_metadata& operator=(response_metadata&& other) {
        statusCode = other.statusCode;
        statusMessage = std::move(other.statusMessage);
        headers = std::move(other.headers);
        return *this;
    }
    
    response_metadata(const sl::json::value& json) {
        for (const sl::json::field& fi : json.as_object()) {
            auto& name = fi.name();
            if ("statusCode" == name) {
                this->statusCode = fi.as_uint16_or_throw(name);
            } else if ("statusMessage" == name) {
                this->statusMessage = fi.as_string_nonempty_or_throw(name);
            } else if ("headers" == name) {
                for (const sl::json::field& hf : fi.as_object_or_throw(name)) {
                    std::string val = hf.as_string_nonempty_or_throw(hf.name());
                    this->headers.emplace_back(hf.name(), std::move(val));
                }
            } else {
                throw support::exception(TRACEMSG("Unknown field: [" + name + "]"));
            }
        }
    }

    sl::json::value to_json() const {
        auto ha = sl::ranges::transform(headers, [](const serverconf::header & el) {
            return el.to_json();
        });
        std::vector<sl::json::field> hfields = sl::ranges::emplace_to_vector(std::move(ha));
        return {
            {"statusCode", statusCode},
            {"statusMessage", statusMessage},
            {"headers", std::move(hfields)}
        };
    }
    
};

} // namespace
}

#endif /* WILTON_SERVER_CONF_RESPONSE_METADATA_HPP */

