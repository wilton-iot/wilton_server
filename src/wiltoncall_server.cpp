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
 * File:   wiltoncall_server.cpp
 * Author: alex
 *
 * Created on January 11, 2017, 10:05 AM
 */

#include <cstdio>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "staticlib/support.hpp"
#include "staticlib/json.hpp"
#include "staticlib/utils.hpp"

#include "wilton/wilton_server.h"
#include "wilton/wiltoncall.h"

#include "wilton/support/buffer.hpp"
#include "wilton/support/exception.hpp"
#include "wilton/support/logging.hpp"
#include "wilton/support/misc.hpp"
#include "wilton/support/registrar.hpp"
#include "wilton/support/unique_handle_registry.hpp"

namespace wilton {
namespace server {

namespace { //anonymous

class http_view {
public:
    std::string method;
    std::string path;
    sl::json::value callbackScript;

    http_view(const http_view&) = delete;

    http_view& operator=(const http_view&) = delete;

    http_view(http_view&& other) :
    method(std::move(other.method)),
    path(std::move(other.path)),
    callbackScript(std::move(other.callbackScript)) { }

    http_view& operator=(http_view&&) = delete;

    http_view(const sl::json::value& json) {
        if (sl::json::type::object != json.json_type()) throw support::exception(TRACEMSG(
                "Invalid 'views' entry: must be an 'object'," +
                " entry: [" + json.dumps() + "]"));
        for (const sl::json::field& fi : json.as_object()) {
            auto& name = fi.name();
            if ("method" == name) {
                method = fi.as_string_nonempty_or_throw(name);
            } else if ("path" == name) {
                path = fi.as_string_nonempty_or_throw(name);
            } else if ("callbackScript" == name) {
                support::check_json_callback_script(fi);
                callbackScript = fi.val().clone();
            } else {
                throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
            }
        }
    }
};

class http_path_deleter {
public:
    void operator()(wilton_HttpPath* path) {
        wilton_HttpPath_destroy(path);
    }
};

class server_ctx {
    // iterators must be permanent
    std::list<sl::json::value> callbackScripts;

public:
    server_ctx(const server_ctx&) = delete;

    server_ctx& operator=(const server_ctx&) = delete;

    server_ctx(server_ctx&& other) :
    callbackScripts(std::move(other.callbackScripts)) { }

    server_ctx& operator=(server_ctx&&) = delete;

    server_ctx() { }

    sl::json::value& add_callback(const sl::json::value& callback) {
        callbackScripts.emplace_back(callback.clone());
        return callbackScripts.back();
    }
};

using server_reg_entry_type = std::pair<wilton_Server*, server_ctx>;

// initialized from wilton_module_init
std::shared_ptr<support::unique_handle_registry<server_reg_entry_type>> server_registry() {
    static auto registry = std::make_shared<support::unique_handle_registry<server_reg_entry_type>>(
            [](server_reg_entry_type* pa) STATICLIB_NOEXCEPT {
                wilton_Server_stop(pa->first);
                delete pa;
            });
    return registry;
}

// initialized from wilton_module_init
std::shared_ptr<support::unique_handle_registry<wilton_Request>> request_registry() {
    static auto registry = std::make_shared<support::unique_handle_registry<wilton_Request>>(
            [](wilton_Request* request) STATICLIB_NOEXCEPT {
                std::string conf = sl::json::dumps({
                    { "statusCode", 503 },
                    { "statusMessage", "Service Unavailable" }
                });
                wilton_Request_set_response_metadata(request, conf.c_str(), static_cast<int> (conf.length()));
                wilton_Request_send_response(request, "", 0);
            });
    return registry;
}

// initialized from wilton_module_init
std::shared_ptr<support::unique_handle_registry<wilton_ResponseWriter>> response_writer_registry() {
    static auto registry = std::make_shared<support::unique_handle_registry<wilton_ResponseWriter>>(
            [](wilton_ResponseWriter* writer) STATICLIB_NOEXCEPT {
                wilton_ResponseWriter_send(writer, "", 0);
            });
    return registry;
}

std::vector<http_view> extract_and_delete_views(sl::json::value& conf) {
    std::vector<sl::json::field>& fields = conf.as_object_or_throw(TRACEMSG(
            "Invalid configuration object specified: invalid type," +
            " conf: [" + conf.dumps() + "]"));
    std::vector<http_view> views;
    uint32_t i = 0;
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        sl::json::field& fi = *it;
        if ("views" == fi.name()) {
            if (sl::json::type::array != fi.json_type()) throw support::exception(TRACEMSG(
                    "Invalid configuration object specified: 'views' attr is not a list," +
                    " conf: [" + conf.dumps() + "]"));
//            if (0 == fi.as_array().size()) throw support::exception(TRACEMSG(
//                    "Invalid configuration object specified: 'views' attr is am empty list," +
//                    " conf: [" + conf.dumps() + "]"));
            for (auto& va : fi.as_array()) {
                if (sl::json::type::object != va.json_type()) throw support::exception(TRACEMSG(
                        "Invalid configuration object specified: 'views' is not a 'object'," +
                        "index: [" + sl::support::to_string(i) + "], conf: [" + conf.dumps() + "]"));
                views.emplace_back(http_view(va));
            }
            // drop views attr and return immediately (iters are invalidated)
            fields.erase(it);
            return views;
        }
        i++;
    }
    throw support::exception(TRACEMSG(
            "Invalid configuration object specified: 'views' list not specified," +
            " conf: [" + conf.dumps() + "]"));
}

void send_system_error(int64_t requestHandle, std::string errmsg) {
    auto rreg = request_registry();
    wilton_Request* request = rreg->remove(requestHandle);
    if (nullptr != request) {
        std::string conf = sl::json::dumps({
            { "statusCode", 500 },
            { "statusMessage", "Internal Server Error" }
        });
        wilton_Request_set_response_metadata(request, conf.c_str(), static_cast<int>(conf.length()));
        wilton_Request_send_response(request, errmsg.c_str(), static_cast<int>(errmsg.length()));
        rreg->put(request);
    }
}

std::vector<std::unique_ptr<wilton_HttpPath, http_path_deleter>> create_paths(
        const std::vector<http_view>& views, server_ctx& ctx) {
    // assert(views.size() == ctx.get_modules_names().size())
    std::vector<std::unique_ptr<wilton_HttpPath, http_path_deleter>> res;
    for (auto& vi : views) {
        // todo: think, maybe pass registries here too
        sl::json::value& cbs_to_pass = ctx.add_callback(vi.callbackScript);
        wilton_HttpPath* ptr = nullptr;
        auto err = wilton_HttpPath_create(std::addressof(ptr), 
                vi.method.c_str(), static_cast<int>(vi.method.length()),
                vi.path.c_str(), static_cast<int>(vi.path.length()),
                static_cast<void*> (std::addressof(cbs_to_pass)),
                [](void* passed, wilton_Request* request) {
                    auto rreg = request_registry();
                    int64_t request_handle = rreg->put(request);
                    sl::json::value* cb_ptr = static_cast<sl::json::value*> (passed);
                    sl::json::value params = cb_ptr->clone();
                    // params structure is pre-checked
                    if (sl::json::type::nullt == params["args"].json_type()) {
                        auto args = std::vector<sl::json::value>();
                        args.emplace_back(request_handle);
                        params.as_object_or_throw().emplace_back("args", std::move(args));
                    } else {
                        // args attr type is pre-checked
                        std::vector<sl::json::value>& args = params.getattr_or_throw("args").as_array_or_throw();
                        args.emplace_back(request_handle);
                    }
                    std::string params_str = params.dumps();
                    std::string engine = params["engine"].as_string();
                    // output will be ignored
                    char* out = nullptr;
                    int out_len = 0;
                    auto err = wiltoncall_runscript(engine.c_str(), static_cast<int> (engine.length()),
                            params_str.c_str(), static_cast<int> (params_str.length()),
                            std::addressof(out), std::addressof(out_len));
                    if (nullptr == err) {
                        if (nullptr != out) {
                            wilton_free(out);
                        }
                    } else {
                        std::string msg = TRACEMSG(err);
                        wilton_free(err);
                        support::log_error("wilton.server", msg);
                        send_system_error(request_handle, msg);
                    }
                    rreg->remove(request_handle);
                });
        if (nullptr != err) throw support::exception(TRACEMSG(err));
        res.emplace_back(ptr, http_path_deleter());
    }
    return res;
}

std::vector<wilton_HttpPath*> wrap_paths(std::vector<std::unique_ptr<wilton_HttpPath, http_path_deleter>>&paths) {
    std::vector<wilton_HttpPath*> res;
    for (auto& pa : paths) {
        res.push_back(pa.get());
    }
    return res;
}

} // namespace

support::buffer server_create(sl::io::span<const char> data) {
    auto conf_in = sl::json::load(data);
    auto views = extract_and_delete_views(conf_in);
    auto conf = conf_in.dumps();
    server_ctx ctx;
    auto paths = create_paths(views, ctx);
    auto paths_pass = wrap_paths(paths);
    wilton_Server* server = nullptr;
    char* err = wilton_Server_create(std::addressof(server),
            conf.c_str(), static_cast<int>(conf.length()), 
            paths_pass.data(), static_cast<int>(paths_pass.size()));
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    auto sreg = server_registry();
    auto en_ptr = new server_reg_entry_type(server, std::move(ctx));
    int64_t handle = sreg->put(en_ptr);
    return support::make_json_buffer({
        { "serverHandle", handle}
    });
}

support::buffer server_stop(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("serverHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'serverHandle' not specified"));
    // get handle
    auto sreg = server_registry();
    auto pa = sreg->remove(handle);
    if (nullptr == pa->first) throw support::exception(TRACEMSG(
            "Invalid 'serverHandle' parameter specified"));
    // call wilton
    char* err = wilton_Server_stop(pa->first);
    if (nullptr != err) {
        sreg->put(pa);
        support::throw_wilton_error(err, TRACEMSG(err));
    }
    // cleanup
    delete pa;
    return support::make_null_buffer();
}

support::buffer server_broadcast_websocket(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    auto rpath = std::ref(sl::utils::empty_string());
    auto rmessage = std::ref(sl::utils::empty_string());
    auto ids_list = std::string("[]");
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("serverHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("path" == name) {
            rpath = fi.as_string_nonempty_or_throw(name);
        } else if("message" == name) {
            rmessage = fi.as_string_nonempty_or_throw(name);
        } else if("idList" == name) {
            ids_list = fi.val().dumps();
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'serverHandle' not specified"));
    if (rpath.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'path' not specified"));
    const std::string& path = rpath.get();
    if (rmessage.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'message' not specified"));
    const std::string& message = rmessage.get();
    // get handle
    auto sreg = server_registry();
    auto pa = sreg->remove(handle);
    if (nullptr == pa->first) throw support::exception(TRACEMSG(
            "Invalid 'serverHandle' parameter specified"));
    // call wilton
    char* err = wilton_Server_broadcast_websocket(pa->first,
            path.c_str(), static_cast<int>(path.length()),
            message.c_str(), static_cast<int>(message.length()),
            ids_list.c_str(), static_cast<int>(ids_list.length()));
    sreg->put(pa);
    if (nullptr != err) {
        support::throw_wilton_error(err, TRACEMSG(err));
    }
    return support::make_null_buffer();
}

support::buffer request_get_metadata(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("requestHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'requestHandle' not specified"));
    // get handle
    auto rreg = request_registry();
    wilton_Request* request = rreg->remove(handle);
    if (nullptr == request) throw support::exception(TRACEMSG(
            "Invalid 'requestHandle' parameter specified"));
    // call wilton
    char* out = nullptr;
    int out_len = 0;
    char* err = wilton_Request_get_request_metadata(request,
            std::addressof(out), std::addressof(out_len));
    rreg->put(request);
    if (nullptr != err) {
        support::throw_wilton_error(err, TRACEMSG(err));
    }
    return support::wrap_wilton_buffer(out, out_len);
}

support::buffer request_get_data(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("requestHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'requestHandle' not specified"));
    // get handle
    auto rreg = request_registry();
    wilton_Request* request = rreg->remove(handle);
    if (nullptr == request) throw support::exception(TRACEMSG(
            "Invalid 'requestHandle' parameter specified"));
    // call wilton
    char* out = nullptr;
    int out_len = 0;
    char* err = wilton_Request_get_request_data(request,
            std::addressof(out), std::addressof(out_len));
    rreg->put(request);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::wrap_wilton_buffer(out, out_len);
}

support::buffer request_get_form_data(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("requestHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'requestHandle' not specified"));
    // get handle
    auto rreg = request_registry();
    wilton_Request* request = rreg->remove(handle);
    if (nullptr == request) throw support::exception(TRACEMSG(
            "Invalid 'requestHandle' parameter specified"));
    // call wilton
    char* out = nullptr;
    int out_len = 0;
    char* err = wilton_Request_get_request_form_data(request,
            std::addressof(out), std::addressof(out_len));
    rreg->put(request);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::wrap_wilton_buffer(out, out_len);
}

support::buffer request_get_data_filename(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("requestHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'requestHandle' not specified"));
    // get handle
    auto rreg = request_registry();
    wilton_Request* request = rreg->remove(handle);
    if (nullptr == request) throw support::exception(TRACEMSG(
            "Invalid 'requestHandle' parameter specified"));
    // call wilton
    char* out = nullptr;
    int out_len = 0;
    char* err = wilton_Request_get_request_data_filename(request,
            std::addressof(out), std::addressof(out_len));
    rreg->put(request);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::wrap_wilton_buffer(out, out_len);
}

support::buffer request_set_response_metadata(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    std::string metadata = sl::utils::empty_string();
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("requestHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("metadata" == name) {
            metadata = fi.val().dumps();
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'requestHandle' not specified"));
    if (metadata.empty()) throw support::exception(TRACEMSG(
            "Required parameter 'metadata' not specified"));
    // get handle
    auto rreg = request_registry();
    wilton_Request* request = rreg->remove(handle);
    if (nullptr == request) throw support::exception(TRACEMSG(
            "Invalid 'requestHandle' parameter specified"));
    // call wilton
    char* err = wilton_Request_set_response_metadata(request, metadata.c_str(), static_cast<int>(metadata.length()));
    rreg->put(request);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::make_null_buffer();
}

support::buffer request_send_response(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    auto rdata = std::ref(sl::utils::empty_string());
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("requestHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("data" == name) {
            rdata = fi.as_string();
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'requestHandle' not specified"));
    const std::string& request_data = rdata.get().empty() ? "{}" : rdata.get();
    // get handle
    auto rreg = request_registry();
    wilton_Request* request = rreg->remove(handle);
    if (nullptr == request) throw support::exception(TRACEMSG(
            "Invalid 'requestHandle' parameter specified"));
    // call wilton
    char* err = wilton_Request_send_response(request, request_data.c_str(), static_cast<int>(request_data.length()));
    rreg->put(request);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::make_null_buffer();
}

support::buffer request_send_temp_file(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    std::string file = sl::utils::empty_string();
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("requestHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("filePath" == name) {
            file = fi.as_string_nonempty_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'requestHandle' not specified"));
    if (file.empty()) throw support::exception(TRACEMSG(
            "Required parameter 'filePath' not specified"));
    // get handle
    auto rreg = request_registry();
    wilton_Request* request = rreg->remove(handle);
    if (nullptr == request) throw support::exception(TRACEMSG(
            "Invalid 'requestHandle' parameter specified"));
    // call wilton
    char* err = wilton_Request_send_file(request, file.c_str(), static_cast<int>(file.length()),
            new std::string(file.data(), file.length()),
            [](void* ctx, int) {
                std::string* filePath_passed = static_cast<std::string*> (ctx);
                std::remove(filePath_passed->c_str());
                delete filePath_passed;
            });
    rreg->put(request);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::make_null_buffer();
}

support::buffer request_send_mustache(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    auto rfile = std::ref(sl::utils::empty_string());
    std::string values = sl::utils::empty_string();
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("requestHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("mustacheFilePath" == name) {
            rfile = fi.as_string_nonempty_or_throw(name);
        } else if ("values" == name) {
            values = fi.val().dumps();
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'requestHandle' not specified"));
    if (rfile.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'mustacheFilePath' not specified"));
    if (values.empty()) {
        values = "{}";
    }
    const std::string& file = rfile.get();
    // get handle
    auto rreg = request_registry();
    wilton_Request* request = rreg->remove(handle);
    if (nullptr == request) throw support::exception(TRACEMSG(
            "Invalid 'requestHandle' parameter specified"));
    // call wilton
    char* err = wilton_Request_send_mustache(request, file.c_str(), static_cast<int>(file.length()),
            values.c_str(), static_cast<int>(values.length()));
    rreg->put(request);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::make_null_buffer();
}

support::buffer request_send_later(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("requestHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'requestHandle' not specified"));
    // get handle
    auto rreg = request_registry();
    wilton_Request* request = rreg->remove(handle);
    if (nullptr == request) throw support::exception(TRACEMSG(
            "Invalid 'requestHandle' parameter specified"));
    // call wilton
    wilton_ResponseWriter* writer;
    char* err = wilton_Request_send_later(request, std::addressof(writer));
    rreg->put(request);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    auto wreg = response_writer_registry();
    int64_t rwhandle = wreg->put(writer);
    return support::make_json_buffer({
        { "responseWriterHandle", rwhandle}
    });
}

support::buffer request_close_websocket(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("requestHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'requestHandle' not specified"));
    // get handle
    auto rreg = request_registry();
    wilton_Request* request = rreg->remove(handle);
    if (nullptr == request) throw support::exception(TRACEMSG(
            "Invalid 'requestHandle' parameter specified"));
    // call wilton
    char* err = wilton_Request_close_websocket(request);
    rreg->put(request);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::make_null_buffer();
}

support::buffer request_set_metadata_with_response_writer(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    std::string metadata = sl::utils::empty_string();
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("responseWriterHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("metadata" == name) {
            metadata = fi.val().dumps();
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'responseWriterHandle' not specified"));
    if (metadata.empty()) throw support::exception(TRACEMSG(
            "Required parameter 'metadata' not specified"));
    // get handle
    auto wreg = response_writer_registry();
    wilton_ResponseWriter* writer = wreg->remove(handle);
    if (nullptr == writer) throw support::exception(TRACEMSG(
            "Invalid 'responseWriterHandle' parameter specified"));
    // call wilton
    char* err = wilton_ResponseWriter_set_metadata(writer, metadata.c_str(), static_cast<int>(metadata.length()));
    wreg->put(writer);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::make_null_buffer();
}

support::buffer request_send_with_response_writer(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    auto rdata = std::ref(sl::utils::empty_string());
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("responseWriterHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("data" == name) {
            rdata = fi.as_string();
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'responseWriterHandle' not specified"));
    const std::string& request_data = rdata.get().empty() ? "{}" : rdata.get();
    // get handle, note: won't be put back - one-off operation   
    auto wreg = response_writer_registry();
    wilton_ResponseWriter* writer = wreg->remove(handle);
    if (nullptr == writer) throw support::exception(TRACEMSG(
            "Invalid 'responseWriterHandle' parameter specified"));
    // call wilton
    char* err = wilton_ResponseWriter_send(writer, request_data.c_str(), static_cast<int>(request_data.length()));
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::make_null_buffer();
}

void initialize() {
    server_registry();
    request_registry();
    response_writer_registry();
}

} // namespace
}

extern "C" char* wilton_module_init() {
    try {
        // server
        wilton::server::initialize();
        wilton::support::register_wiltoncall("server_create", wilton::server::server_create);
        wilton::support::register_wiltoncall("server_stop", wilton::server::server_stop);
        wilton::support::register_wiltoncall("server_broadcast_websocket", wilton::server::server_broadcast_websocket);
        wilton::support::register_wiltoncall("request_get_metadata", wilton::server::request_get_metadata);
        wilton::support::register_wiltoncall("request_get_data", wilton::server::request_get_data);
        wilton::support::register_wiltoncall("request_get_form_data", wilton::server::request_get_form_data);
        wilton::support::register_wiltoncall("request_get_data_filename", wilton::server::request_get_data_filename);
        wilton::support::register_wiltoncall("request_set_response_metadata", wilton::server::request_set_response_metadata);
        wilton::support::register_wiltoncall("request_send_response", wilton::server::request_send_response);
        wilton::support::register_wiltoncall("request_send_temp_file", wilton::server::request_send_temp_file);
        wilton::support::register_wiltoncall("request_send_mustache", wilton::server::request_send_mustache);
        wilton::support::register_wiltoncall("request_send_later", wilton::server::request_send_later);
        wilton::support::register_wiltoncall("request_close_websocket", wilton::server::request_close_websocket);
        wilton::support::register_wiltoncall("request_set_metadata_with_response_writer", wilton::server::request_set_metadata_with_response_writer);
        wilton::support::register_wiltoncall("request_send_with_response_writer", wilton::server::request_send_with_response_writer);
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}
