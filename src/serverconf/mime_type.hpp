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
 * File:   mime_type.hpp
 * Author: alex
 *
 * Created on May 11, 2016, 1:23 PM
 */

#ifndef WILTON_MIME_TYPE_HPP
#define WILTON_MIME_TYPE_HPP

#include <string>
#include <vector>

#include "wilton/support/exception.hpp"

#include "staticlib/json.hpp"

namespace wilton {
namespace serverconf {

class mime_type {
public:
    std::string extension = "";
    std::string mime = "";

    mime_type(const mime_type&) = delete;

    mime_type& operator=(const mime_type&) = delete;

    mime_type(mime_type&& other) :
    extension(std::move(other.extension)),
    mime(std::move(other.mime)) { }

    mime_type& operator=(mime_type&& other) {
        this->extension = std::move(other.extension);
        this->mime = std::move(other.mime);
        return *this;
    }

    mime_type() { }
    
    mime_type(const std::string& extension, const std::string& mime) :
    extension(extension.data(), extension.length()),
    mime(mime.data(), mime.length()) { }

    mime_type(const sl::json::value& json) {
        for (const sl::json::field& fi : json.as_object()) {
            auto& name = fi.name();
            if ("extension" == name) {
                this->extension = fi.as_string_nonempty_or_throw(name);
            } else if ("mime" == name) {
                this->mime = fi.as_string_nonempty_or_throw(name);
            } else {
                throw support::exception(TRACEMSG("Unknown 'mimeType' field: [" + name + "]"));
            }
        }
        if (0 == extension.length()) throw support::exception(TRACEMSG(
                "Invalid 'mimeType.extension' field: []"));
        if (0 == mime.length()) throw support::exception(TRACEMSG(
                "Invalid 'mimeType.mime' field: []"));
    }
    
    sl::json::value to_json() const {    
        return {
            {"extension", extension},
            {"mime", mime}
        };
    }
    
    mime_type clone() const {
        return mime_type{extension, mime};
    }
    
};

} // namespace
}

#endif /* WILTON_MIME_TYPE_HPP */

