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
 * File:   mustache_config.hpp
 * Author: alex
 *
 * Created on September 7, 2016, 11:22 AM
 */

#ifndef WILTON_SERVERCONF_MUSTACHE_CONFIG_HPP
#define WILTON_SERVERCONF_MUSTACHE_CONFIG_HPP

#include <string>

#include "staticlib/json.hpp"

#include "wilton/support/exception.hpp"

namespace wilton {
namespace serverconf {

class mustache_config {
public:
    std::vector<std::string> partialsDirs;

    mustache_config(const mustache_config&) = delete;

    mustache_config& operator=(const mustache_config&) = delete;

    mustache_config(mustache_config&& other) :
    partialsDirs(std::move(other.partialsDirs)) { }

    mustache_config& operator=(mustache_config&& other) {
        this->partialsDirs = std::move(other.partialsDirs);
        return *this;
    }

    mustache_config() { }

    mustache_config(const sl::json::value& json) {
        for (const sl::json::field& fi : json.as_object()) {
            auto& name = fi.name();
            if ("partialsDirs" == name) {
                for (const sl::json::value& va : fi.as_array_or_throw(name)) {
                    if (sl::json::type::string != va.json_type() || va.as_string().empty()) {
                        throw support::exception(TRACEMSG(
                                "Invalid 'mustache.partialsDirs.el' value,"
                                " type: [" + sl::json::stringify_json_type(va.json_type()) + "]," +
                                " value: [" + va.dumps() + "]"));
                    }
                    partialsDirs.emplace_back(va.as_string());
                }
            } else {
                throw support::exception(TRACEMSG("Unknown 'mustache' field: [" + name + "]"));
            }
        }
    }

    sl::json::value to_json() const {
        return {
            { "partialsDirs", [this]{
                auto ra = sl::ranges::transform(partialsDirs, [](const std::string& el) {
                    return sl::json::value(el);
                });
                return ra.to_vector();
            }() }
        };
    }
};

} // namespace
}

#endif /* WILTON_SERVERCONF_MUSTACHE_CONFIG_HPP */

