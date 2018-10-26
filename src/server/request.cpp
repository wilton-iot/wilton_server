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
 * File:   request.cpp
 * Author: alex
 * 
 * Created on June 2, 2016, 5:16 PM
 */

#include "server/request.hpp"

#include <cctype>
#include <algorithm>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "staticlib/config.hpp"
#include "staticlib/io.hpp"
#include "staticlib/pion.hpp"
#include "staticlib/pion/http_parser.hpp"
#include "staticlib/mustache.hpp"
#include "staticlib/pimpl/forward_macros.hpp"
#include "staticlib/json.hpp"
#include "staticlib/tinydir.hpp"
#include "staticlib/utils.hpp"

#include "wilton/support/exception.hpp"
#include "wilton/support/misc.hpp"

#include "server/response_stream_sender.hpp"
#include "server/request_payload_handler.hpp"
#include "server/server.hpp"

#include "serverconf/header.hpp"
#include "serverconf/response_metadata.hpp"
#include "serverconf/request_metadata.hpp"

namespace wilton {
namespace server {

namespace { // anonymous

using partmap_type = const std::map<std::string, std::string>&;

const std::unordered_set<std::string> HEADERS_DISCARD_DUPLICATES{
    "age", "authorization", "content-length", "content-type", "etag", "expires",
    "from", "host", "if-modified-since", "if-unmodified-since", "last-modified", "location",
    "max-forwards", "proxy-authorization", "referer", "retry-after", "user-agent"
};

} //namespace

// note: supports both HTTP and WebSocker to minimize client changes
class request::impl : public sl::pimpl::object::impl {

    enum class request_state {
        created, committed
    };
    std::atomic<request_state> state;

    // http state
    // owning ptrs here to not restrict clients async ops
    sl::pion::http_request_ptr req;
    sl::pion::response_writer_ptr resp;
    sl::support::observer_ptr<const std::map<std::string, std::string>> mustache_partials;

    // ws state
    sl::pion::websocket_ptr ws;
    bool websocket = false;

public:

    impl(void* /* sl::pion::http_request_ptr&& */ req, void* /* sl::pion::response_writer_ptr&& */ resp,
            const std::map<std::string, std::string>& mustache_partials) :
    state(request_state::created),
    req(std::move(*static_cast<sl::pion::http_request_ptr*>(req))),
    resp(std::move(*static_cast<sl::pion::response_writer_ptr*> (resp))),
    mustache_partials(mustache_partials) { }

    impl(void* /* sl::pion::websocket_ptr&& */ wsocket, bool response_allowed) :
    state(response_allowed ? request_state::created : request_state::committed),
    ws(std::move(*static_cast<sl::pion::websocket_ptr*>(wsocket))),
    websocket(true) { }

    serverconf::request_metadata get_request_metadata(request&) {
        auto& rq = get_request();
        std::string http_ver = sl::support::to_string(rq.get_version_major()) +
                "." + sl::support::to_string(rq.get_version_minor());
        auto headers = get_request_headers(rq);
        auto queries = get_queries(rq);
        std::string protocol = get_conn().get_ssl_flag() ? "https" : "http";
        return serverconf::request_metadata(http_ver, protocol, rq.get_method(), rq.get_resource(),
                rq.get_query_string(), std::move(queries), std::move(headers));
    }

    const std::string& get_request_data(request&) {
        if (websocket) throw support::exception(TRACEMSG(
                "Cached data not supported with WebSocket"));
        return request_payload_handler::get_data_string(req);
    }

    support::buffer get_request_data_buffer(request&) {
        if (!websocket) throw support::exception(TRACEMSG(
                "Buffer data not supported with HTTP"));
        auto src = ws->message_data();
        return support::make_source_buffer(src);
    }

    sl::json::value get_request_form_data(request&) {
        if (websocket) throw support::exception(TRACEMSG(
                "Form data not supported with WebSocket"));
        const std::string& data = request_payload_handler::get_data_string(req);
        auto dict = std::unordered_multimap<std::string, std::string, sl::pion::algorithm::ihash, sl::pion::algorithm::iequal_to>();
        auto err = sl::pion::http_parser::parse_url_encoded(dict, data);
        if (!err) throw support::exception(TRACEMSG(
                "Error parsing request body as 'application/x-www-form-urlencoded'"));
        auto res = std::vector<sl::json::field>();
        for (auto& en : dict) {
            res.emplace_back(en.first, en.second);
        }
        return sl::json::value(std::move(res));
    }

    const std::string& get_request_data_filename(request&) {
        if (websocket) throw support::exception(TRACEMSG(
                "Persistent request data not supported with WebSocket"));
        return request_payload_handler::get_data_filename(req);
    }

    void set_response_metadata(request&, serverconf::response_metadata rm) {
        if (request_state::created != state.load(std::memory_order_acquire)) throw support::exception(TRACEMSG(
                "Invalid request lifecycle operation meta, request is already committed"));
        if (websocket) throw support::exception(TRACEMSG(
                "Response metadata not supported with WebSocket"));
        resp->get_response().set_status_code(rm.statusCode);
        resp->get_response().set_status_message(rm.statusMessage);
        for (const serverconf::header& ha : rm.headers) {
            resp->get_response().change_header(ha.name, ha.value);
        }
    }

    void send_response(request&, sl::io::span<const char> data) {
        auto state_expected = request_state::created;
        if (!state.compare_exchange_strong(state_expected, request_state::committed,
                std::memory_order_acq_rel, std::memory_order_relaxed)) throw support::exception(TRACEMSG(
                "Invalid request lifecycle operation, request is already committed"));
        if (!websocket) {
            resp->write(data);
            resp->send(std::move(resp));
        } else {
            ws->write(data);
            ws->send(std::move(ws));
        }
    }

    void send_file(request&, std::string file_path, std::function<void(bool)> finalizer) {
        if (websocket) throw support::exception(TRACEMSG(
                "Files sending not supported with WebSocket"));
        auto fd = sl::tinydir::file_source(file_path);
        auto state_expected = request_state::created;
        if (!state.compare_exchange_strong(state_expected, request_state::committed,
                std::memory_order_acq_rel, std::memory_order_relaxed)) throw support::exception(TRACEMSG(
                "Invalid request lifecycle operation, request is already committed"));
        auto fd_ptr = sl::io::make_source_istream_ptr(std::move(fd));
        auto sender = sl::support::make_unique<response_stream_sender>(std::move(resp), std::move(fd_ptr), std::move(finalizer));
        sender->send(std::move(sender));
    }

    void send_mustache(request&, std::string mustache_file_path, sl::json::value json) {
        if (websocket) throw support::exception(TRACEMSG(
                "Mustache not supported with WebSocket"));
        auto state_expected = request_state::created;
        if (!state.compare_exchange_strong(state_expected, request_state::committed,
                std::memory_order_acq_rel, std::memory_order_relaxed)) throw support::exception(TRACEMSG(
                "Invalid request lifecycle operation, request is already committed"));
        std::string mpath = [&mustache_file_path] () -> std::string {
            if (sl::utils::starts_with(mustache_file_path, support::file_proto_prefix)) {
                return mustache_file_path.substr(support::file_proto_prefix.length());
            }
            return mustache_file_path;
        } ();
        auto mp = sl::mustache::source(mpath, std::move(json), *mustache_partials);
        auto mp_ptr = sl::io::make_source_istream_ptr(std::move(mp));
        auto sender = sl::support::make_unique<response_stream_sender>(std::move(resp), std::move(mp_ptr));
        sender->send(std::move(sender));
    }
    
    response_writer send_later(request&) {
        if (websocket) throw support::exception(TRACEMSG(
                "Delayed responses not supported with WebSocket"));
        auto state_expected = request_state::created;
        if (!state.compare_exchange_strong(state_expected, request_state::committed,
                std::memory_order_acq_rel, std::memory_order_relaxed)) throw support::exception(TRACEMSG(
                "Invalid request lifecycle operation, request is already committed"));
        sl::pion::response_writer_ptr writer = std::move(this->resp);
        return response_writer{static_cast<void*>(std::addressof(writer))};
    }

    void finish(request&) {
        auto state_expected = request_state::created;
        if (state.compare_exchange_strong(state_expected, request_state::committed,
                std::memory_order_acq_rel, std::memory_order_relaxed)) {
            if (!websocket) {
                resp->send(std::move(resp));
            } else {
                ws->receive(std::move(ws));
            }
        }
    }

    bool is_websocket(request&) {
        return websocket;
    }

    void close_websocket(request&) {
        if (!websocket) throw support::exception(TRACEMSG(
                "Close connection operation not supported with HTTP"));
        auto state_expected = request_state::created;
        if (!state.compare_exchange_strong(state_expected, request_state::committed,
                std::memory_order_acq_rel, std::memory_order_relaxed)) throw support::exception(TRACEMSG(
                "Invalid request lifecycle operation, request is already committed"));
        ws->close(std::move(ws));
    }

private:

    sl::pion::http_request& get_request() {
        if (!websocket) {
            return *req;
        } else {
            return ws->get_request();
        }
    }

    sl::pion::tcp_connection& get_conn() {
        if (!websocket) {
            return *resp->get_connection();
        } else {
            return ws->get_connection();
        }
    }

    // todo: cookies
    // Duplicates in raw headers are handled in the following ways, depending on the header name:
    // Duplicates of age, authorization, content-length, content-type, etag, expires, 
    // from, host, if-modified-since, if-unmodified-since, last-modified, location, 
    // max-forwards, proxy-authorization, referer, retry-after, or user-agent are discarded.
    // For all other headers, the values are joined together with ', '.
    static std::vector<serverconf::header> get_request_headers(sl::pion::http_request& req) {
        std::unordered_map<std::string, serverconf::header> map{};
        for (const auto& en : req.get_headers()) {
            auto ha = serverconf::header(en.first, en.second);
            std::string key = en.first;
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            auto inserted = map.emplace(key, std::move(ha));
            if (!inserted.second && 0 == HEADERS_DISCARD_DUPLICATES.count(key)) {
                append_with_comma(inserted.first->second.value, en.second);
            }
        }
        std::vector<serverconf::header> res{};
        for (auto& en : map) {
            res.emplace_back(std::move(en.second));
        }
        std::sort(res.begin(), res.end(), [](const serverconf::header& el1, const serverconf::header & el2) {
            return el1.name < el2.name;
        });
        return res;
    }
    
    static std::vector<std::pair<std::string, std::string>> get_queries(sl::pion::http_request& req) {
        std::unordered_map<std::string, std::string> map{};
        for (const auto& en : req.get_queries()) {
            auto inserted = map.emplace(en.first, en.second);
            if (!inserted.second) {
                append_with_comma(inserted.first->second, en.second);
            }
        }
        std::vector<std::pair<std::string, std::string>> res{};
        for (auto& en : map) {
            res.emplace_back(en.first, en.second);
        }
        return res;
    }

    static void append_with_comma(std::string& str, const std::string& tail) {
        if (str.empty()) {
            str = tail;
        } else if (!tail.empty()) {
            str.push_back(',');
            str.append(tail);
        }
    }

};
PIMPL_FORWARD_CONSTRUCTOR(request, (void*)(void*)(partmap_type), (), support::exception)
PIMPL_FORWARD_CONSTRUCTOR(request, (void*)(bool), (), support::exception)
PIMPL_FORWARD_METHOD(request, serverconf::request_metadata, get_request_metadata, (), (), support::exception)
PIMPL_FORWARD_METHOD(request, const std::string&, get_request_data, (), (), support::exception)
PIMPL_FORWARD_METHOD(request, support::buffer, get_request_data_buffer, (), (), support::exception)
PIMPL_FORWARD_METHOD(request, sl::json::value, get_request_form_data, (), (), support::exception)
PIMPL_FORWARD_METHOD(request, const std::string&, get_request_data_filename, (), (), support::exception)
PIMPL_FORWARD_METHOD(request, void, set_response_metadata, (serverconf::response_metadata), (), support::exception)
PIMPL_FORWARD_METHOD(request, void, send_response, (sl::io::span<const char>), (), support::exception)
PIMPL_FORWARD_METHOD(request, void, send_file, (std::string)(std::function<void(bool)>), (), support::exception)
PIMPL_FORWARD_METHOD(request, void, send_mustache, (std::string)(sl::json::value), (), support::exception)
PIMPL_FORWARD_METHOD(request, response_writer, send_later, (), (), support::exception)
PIMPL_FORWARD_METHOD(request, void, finish, (), (), support::exception)
PIMPL_FORWARD_METHOD(request, bool, is_websocket, (), (), support::exception)
PIMPL_FORWARD_METHOD(request, void, close_websocket, (), (), support::exception)

} // namespace
}

