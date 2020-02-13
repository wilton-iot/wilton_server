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
 * File:   request_metadata.hpp
 * Author: alex
 *
 * Created on May 5, 2016, 7:20 PM
 */

#ifndef WILTON_SERVER_CONF_REQUEST_METADATA_HPP
#define WILTON_SERVER_CONF_REQUEST_METADATA_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "staticlib/config.hpp"
#include "staticlib/ranges.hpp"
#include "staticlib/json.hpp"

#include "wilton/support/exception.hpp"

#include "conf/header.hpp"

namespace wilton {
namespace server {
namespace conf {

class request_metadata {
    std::string httpVersion;
    std::string protocol;
    std::string method;
    std::string pathname;
    std::string query;
    std::vector<std::pair<std::string, std::string>> queries;
    std::vector<server::conf::header> headers;

public:
    request_metadata(const request_metadata&) = delete;
    
    request_metadata& operator=(const request_metadata&) = delete;
    
    request_metadata(request_metadata&& other) :
    httpVersion(std::move(other.httpVersion)),
    protocol(std::move(other.protocol)),
    method(std::move(other.method)),
    pathname(std::move(other.pathname)),
    query(std::move(other.query)),
    queries(std::move(other.queries)),
    headers(std::move(other.headers)) { }
    
    request_metadata& operator=(request_metadata&& other) {
        httpVersion = std::move(other.httpVersion);
        protocol = std::move(other.protocol);
        method = std::move(other.method);
        pathname = std::move(other.pathname);
        query = std::move(other.query);
        queries = std::move(other.queries);
        headers = std::move(other.headers);
        return *this;
    }
    
    request_metadata(const std::string& httpVersion, const std::string& protocol, 
            const std::string& method, const std::string& pathname,
            const std::string& query, 
            std::vector<std::pair<std::string, std::string>> queries,
            std::vector<server::conf::header> headers) :
    httpVersion(httpVersion.data(), httpVersion.length()),
    protocol(protocol.data(), protocol.length()),
    method(method.data(), method.length()),
    pathname(pathname.data(), pathname.length()),
    query(query.data(), query.length()),
    queries(std::move(queries)),
    headers(std::move(headers)) { }
        
    sl::json::value to_json() const {
        auto ha = sl::ranges::transform(headers, [](const server::conf::header& el) {
            return el.to_json();
        });
        std::vector<sl::json::field> hfields = sl::ranges::emplace_to_vector(std::move(ha));
        auto qu = sl::ranges::transform(sl::ranges::refwrap(queries), [](const std::pair<std::string, std::string>& pa) {
            return sl::json::field(pa.first, pa.second);
        });
        std::vector<sl::json::field> qfields = sl::ranges::emplace_to_vector(std::move(qu));
        return {
            {"httpVersion", httpVersion},
            {"protocol", protocol},
            {"method", method},
            {"url", reconstructUrl()},
            {"pathname", pathname},
            {"query", query},
            {"queries", std::move(qfields)},
            {"headers", std::move(hfields)}
        };
    }

private:
    std::string reconstructUrl() const {
        if (0 == query.length()) {
            return pathname;
        } else {
            return pathname + "?" + query;
        }
    }
};


} // namespace
}
}

#endif /* WILTON_SERVER_CONF_REQUEST_METADATA_HPP */

