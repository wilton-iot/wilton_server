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
 * File:   server.cpp
 * Author: alex
 * 
 * Created on June 2, 2016, 5:33 PM
 */

#include "server.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "asio.hpp"
#include "openssl/x509.h"

#include "staticlib/config.hpp"
#include "staticlib/json.hpp"
#include "staticlib/io.hpp"
#include "staticlib/pimpl/forward_macros.hpp"
#include "staticlib/pion.hpp"
#include "staticlib/tinydir.hpp"
#include "staticlib/utils.hpp"

#include "wilton/support/exception.hpp"

#include "conf/server_config.hpp"
#include "file_handler.hpp"
#include "mustache_cache.hpp"
#include "request.hpp"
#include "request_payload_handler.hpp"
#include "zip_handler.hpp"

namespace wilton {
namespace server {

namespace { // anonymous

using partmap_type = const std::map<std::string, std::string>&;

const std::string mustache_ext = ".mustache";

void handle_not_found_request(sl::pion::http_request_ptr req, sl::pion::response_writer_ptr resp) {
    auto msg = sl::json::value({
        {"error", {
            { "code", sl::pion::http_request::RESPONSE_CODE_NOT_FOUND },
            { "message", sl::pion::http_request::RESPONSE_MESSAGE_NOT_FOUND },
            { "path", req->get_resource() }}}
    }).dumps();
    resp->get_response().set_status_code(404);
    resp->get_response().set_status_message(sl::pion::http_request::RESPONSE_MESSAGE_NOT_FOUND);
    resp->write(msg);
    resp->send(std::move(resp));
}

} // namespace

class server::impl : public sl::pimpl::object::impl {
    mustache_cache mustache_templates;
    std::map<std::string, std::string> mustache_partials;
    std::unique_ptr<sl::pion::http_server> server_ptr;

public:
    impl(serverconf::server_config conf, std::vector<sl::support::observer_ptr<http_path>> paths) :
    mustache_templates(),
    mustache_partials(load_partials(conf.mustache)),
    server_ptr(std::unique_ptr<sl::pion::http_server>(new sl::pion::http_server(
            conf.numberOfThreads, 
            conf.tcpPort,
            asio::ip::address_v4::from_string(conf.ipAddress),
            conf.readTimeoutMillis,
            conf.ssl.keyFile,
            create_pwd_cb(conf.ssl.keyPassword),
            conf.ssl.verifyFile,
            create_verifier_cb(conf.ssl.verifySubjectSubstr)))) {
        auto conf_ptr = std::make_shared<serverconf::request_payload_config>(conf.requestPayload.clone());
        for (auto& pa : paths) {
            auto ha = pa->handler; // copy
            if (sl::utils::starts_with(pa->method, "WS")) {
                bool response_allowed = "WSCLOSE" != pa->method;
                server_ptr->add_websocket_handler(pa->method, pa->path,
                        [ha, response_allowed](sl::pion::websocket_ptr ws) {
                            request req_ws{static_cast<void*> (std::addressof(ws)), response_allowed};
                            ha(req_ws);
                            req_ws.finish();
                        });
            } else {
                server_ptr->add_handler(pa->method, pa->path,
                        [ha, this](sl::pion::http_request_ptr req, sl::pion::response_writer_ptr resp) {
                            request req_wrap{static_cast<void*> (std::addressof(req)),
                                    static_cast<void*> (std::addressof(resp)),
                                    this->mustache_templates, this->mustache_partials};
                            ha(req_wrap);
                            req_wrap.finish();
                        });
                server_ptr->add_payload_handler(pa->method, pa->path, [conf_ptr](sl::pion::http_request_ptr& /* request */) {
                    return request_payload_handler(*conf_ptr);
                });
            }
        }
        if (!conf.root_redirect_location.empty()) {
            std::string location = conf.root_redirect_location;
            server_ptr->add_handler("GET", "/", 
                    [location](sl::pion::http_request_ptr req, sl::pion::response_writer_ptr resp) {
                        if("/" == req->get_resource()) {
                            auto& rp = resp->get_response();
                            rp.set_status_code(303);
                            rp.set_status_message("See Other");
                            rp.change_header("Location", location);
                            resp->send(std::move(resp));
                        } else {
                            handle_not_found_request(std::move(req), std::move(resp));
                        }
                    });
        }
        for (const auto& dr : conf.documentRoots) {
            if (dr.dirPath.length() > 0) {
                check_dir_path(dr.dirPath);
                server_ptr->add_handler("GET", dr.resource, file_handler(dr));
            } else if (dr.zipPath.length() > 0) {
                check_zip_path(dr.zipPath);
                server_ptr->add_handler("GET", dr.resource, zip_handler(dr));
            } else throw support::exception(TRACEMSG(
                    "Invalid 'documentRoot': [" + dr.to_json().dumps() + "]"));
        }
        server_ptr->get_scheduler().set_thread_stop_hook([]() STATICLIB_NOEXCEPT {
            auto tid_str = sl::support::to_string_any(std::this_thread::get_id());
            wilton_clean_tls(tid_str.c_str(), static_cast<int>(tid_str.length()));
        });
        server_ptr->start();
    }

    void stop(server&) {
        server_ptr->stop();
    }

    void broadcast_websocket(server&, const std::string& path, sl::io::span<const char> message,
            const std::set<std::string>& dest_ids) {
        server_ptr->broadcast_websocket(path, message, sl::websocket::frame_type::text, dest_ids);
    }

private:
    static std::function<std::string(std::size_t, asio::ssl::context::password_purpose)> create_pwd_cb(const std::string& password) {
        return [password](std::size_t, asio::ssl::context::password_purpose) {
            return password;
        };
    }

    static std::string extract_subject(asio::ssl::verify_context& ctx) {
        if (nullptr == ctx.native_handle()) return "";
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
        auto cert = ctx.native_handle()->current_cert;
#else
        auto cert = X509_STORE_CTX_get0_cert(ctx.native_handle());
#endif
        // X509_NAME_oneline
        auto name_struct_ptr = X509_get_subject_name(cert);
        if (nullptr == name_struct_ptr) return "";
        auto name_ptr = X509_NAME_oneline(name_struct_ptr, nullptr, 0);
        if (nullptr == name_ptr) return "";
        auto deferred = sl::support::defer([name_ptr]() STATICLIB_NOEXCEPT {
            OPENSSL_free(name_ptr);
        });
        return std::string(name_ptr);
    }

    static std::function<bool(bool, asio::ssl::verify_context&)> create_verifier_cb(const std::string& subject_part) {
        return [subject_part](bool preverify_ok, asio::ssl::verify_context & ctx) {
            // cert validation fail
            if (!preverify_ok) {
                return false;
            }
            // not the leaf certificate
            auto error_depth = X509_STORE_CTX_get_error_depth(ctx.native_handle());
            if (error_depth > 0) {
                return true;
            }
            // no subject restrictions
            if (subject_part.empty()) {
                return true;
            }
            // check substr
            std::string subject = extract_subject(ctx);
            auto pos = subject.find(subject_part);
            return std::string::npos != pos;
        };
    }
    
    static std::map<std::string, std::string> load_partials(const serverconf::mustache_config& cf) {
        std::map<std::string, std::string> res;
        for (const std::string& dirpath : cf.partialsDirs) {
            for (const sl::tinydir::path& tf : sl::tinydir::list_directory(dirpath)) {
                if (!sl::utils::ends_with(tf.filename(), mustache_ext)) continue;
                std::string name = std::string(tf.filename().data(), tf.filename().length() - mustache_ext.length());
                std::string val = read_file(tf);
                auto pa = res.insert(std::make_pair(std::move(name), std::move(val)));
                if (!pa.second) throw support::exception(TRACEMSG(
                        "Invalid duplicate 'mustache.partialsDirs' element," +
                        " dirpath: [" + dirpath + "], path: [" + tf.filepath() + "]"));
            }
        }
        return res;
    }

    static std::string read_file(const sl::tinydir::path& tf) {
        auto fd = tf.open_read();
        sl::io::string_sink sink{};
        sl::io::copy_all(fd, sink);
        return std::move(sink.get_string());
    }

    static void check_dir_path(const std::string& dir) {
        auto path = sl::tinydir::path(dir);
        if (!(path.exists() && path.is_directory())) throw support::exception(TRACEMSG(
                "Invalid non-existing 'dirPath' specified, path: [" + dir + "]"));
    }

    static void check_zip_path(const std::string& zip) {
        auto path = sl::tinydir::path(zip);
        if (!(path.exists() && path.is_regular_file())) throw support::exception(TRACEMSG(
                "Invalid non-existing 'zipPath' specified, path: [" + zip + "]"));
    }
    
};
PIMPL_FORWARD_CONSTRUCTOR(server, (serverconf::server_config)(std::vector<sl::support::observer_ptr<http_path>>), (), support::exception)
PIMPL_FORWARD_METHOD(server, void, stop, (), (), support::exception)
PIMPL_FORWARD_METHOD(server, void, broadcast_websocket, (const std::string&)
        (sl::io::span<const char>)(const std::set<std::string>&), (), support::exception)

} // namespace
}
