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
 * File:   server_configConfig.hpp
 * Author: alex
 *
 * Created on May 5, 2016, 7:20 PM
 */

#ifndef WILTON_SERVER_CONF_SERVER_CONFIG_HPP
#define WILTON_SERVER_CONF_SERVER_CONFIG_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "staticlib/ranges.hpp"
#include "staticlib/json.hpp"

#include "wilton/support/exception.hpp"

#include "conf/document_root.hpp"
#include "conf/mustache_config.hpp"
#include "conf/request_payload_config.hpp"
#include "conf/ssl_config.hpp"

namespace wilton {
namespace serverconf {

class server_config {
public:
    uint32_t numberOfThreads = 2;
    uint16_t tcpPort = 8080;
    std::string ipAddress = "0.0.0.0";
    uint32_t readTimeoutMillis = 10000;
    ssl_config ssl;
    std::vector<document_root> documentRoots;
    request_payload_config requestPayload;
    mustache_config mustache;
    std::string root_redirect_location;

    server_config(const server_config&) = delete;

    server_config& operator=(const server_config&) = delete;

    server_config(server_config&& other) :
    numberOfThreads(other.numberOfThreads),
    tcpPort(other.tcpPort),
    ipAddress(std::move(other.ipAddress)),
    readTimeoutMillis(other.readTimeoutMillis),
    ssl(std::move(other.ssl)),
    documentRoots(std::move(other.documentRoots)),
    requestPayload(std::move(other.requestPayload)),
    mustache(std::move(other.mustache)),
    root_redirect_location(std::move(other.root_redirect_location)) { }

    server_config& operator=(server_config&& other) {
        this->numberOfThreads = other.numberOfThreads;
        this->tcpPort = other.tcpPort;
        this->ipAddress = std::move(other.ipAddress);
        this->readTimeoutMillis = other.readTimeoutMillis;
        this->ssl = std::move(other.ssl);
        this->documentRoots = std::move(other.documentRoots);
        this->requestPayload = std::move(other.requestPayload);
        this->mustache = std::move(other.mustache);
        this->root_redirect_location = std::move(other.root_redirect_location);
        return *this;
    }

    server_config(const sl::json::value& json) {
        for (const sl::json::field& fi : json.as_object()) {
            auto& name = fi.name();
            if ("numberOfThreads" == name) {
                this->numberOfThreads = fi.as_uint16_positive_or_throw(name);
            } else if ("tcpPort" == name) {
                this->tcpPort = fi.as_uint16_or_throw(name);
            } else if ("ipAddress" == name) {
                this->ipAddress = fi.as_string_nonempty_or_throw(name);
            } else if ("readTimeoutMillis" == name) {
                this->readTimeoutMillis = fi.as_uint32_positive_or_throw(name);
            } else if ("ssl" == name) {
                this->ssl = ssl_config(fi.val());
            } else if ("documentRoots" == name) {
                for (const sl::json::value& lo : fi.as_array_or_throw(name)) {
                    auto jd = serverconf::document_root(lo);
                    this->documentRoots.emplace_back(std::move(jd));
                }
            } else if ("requestPayload" == name) {
                this->requestPayload = serverconf::request_payload_config(fi.val());
            } else if ("mustache" == name) {
                this->mustache = mustache_config(fi.val());
            } else if ("rootRedirectLocation" == name) {
                this->root_redirect_location = fi.as_string_nonempty_or_throw(name);
            } else {
                throw support::exception(TRACEMSG("Unknown field: [" + name + "]"));
            }
        }
    }

    sl::json::value to_json() const {
        return {
            {"numberOfThreads", numberOfThreads},
            {"tcpPort", tcpPort},
            {"ipAddress", ipAddress},
            {"readTimeoutMillis", readTimeoutMillis},
            {"ssl", ssl.to_json()},
            {"documentRoots", [this]() {
                auto drs = sl::ranges::transform(documentRoots, [](const serverconf::document_root& el) {
                    return el.to_json();
                });
                return drs.to_vector();
            }()},
            {"requestPayload", requestPayload.to_json()},
            {"mustache", mustache.to_json()},
            {"rootRedirectLocation", root_redirect_location},
        };
    }
};

} // namespace
}

#endif /* WILTON_SERVER_CONF_SERVER_CONFIG_HPP */

