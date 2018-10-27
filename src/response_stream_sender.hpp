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
 * File:   response_stream_sender.hpp
 * Author: alex
 *
 * Created on April 5, 2015, 10:58 PM
 */

#ifndef WILTON_SERVER_RESPONSE_STREAM_SENDER_HPP
#define WILTON_SERVER_RESPONSE_STREAM_SENDER_HPP

#include <istream>
#include <memory>

#include "asio.hpp"

#include "staticlib/pion.hpp"

#include "staticlib/io.hpp"

namespace wilton {
namespace server {

class response_stream_sender {
    sl::pion::response_writer_ptr writer;
    std::unique_ptr<std::istream> stream;
    std::function<void(bool)> finalizer;

    std::array<char, 4096> buf;

public:
    response_stream_sender(sl::pion::response_writer_ptr writer, 
            std::unique_ptr<std::istream> stream, 
            std::function<void(bool)> finalizer = [](bool){}) :
    writer(std::move(writer)),
    stream(std::move(stream)),
    finalizer(std::move(finalizer)) { }

    static void send(std::unique_ptr<response_stream_sender> self) {
        std::error_code ec;
        self->handle_write(std::move(self), ec, 0);
    }

    static void handle_write(std::unique_ptr<response_stream_sender> self,
            const std::error_code& ec, size_t /* bytes_written */) {
        if (!ec) {
            auto src = sl::io::streambuf_source(self->stream->rdbuf());
            size_t len = sl::io::read_all(src, self->buf);
            self->writer->clear();
            if (len > 0) {
                if (self->buf.size() == len) {
                    self->writer->write_nocopy({self->buf.data(), len});
                    auto& wr = self->writer;
                    auto self_shared = sl::support::make_shared_with_release_deleter(self.release());
                    wr->send_chunk(
                        [self_shared](const std::error_code& ec, size_t bt) {
                            auto self = sl::support::make_unique_from_shared_with_release_deleter(self_shared);
                            if (nullptr != self.get()) {
                                handle_write(std::move(self), ec, bt);
                            }
                        });
                } else {
                    self->writer->write({self->buf.data(), len});
                    self->writer->send_final_chunk(std::move(self->writer));
                    self->finalizer(true);
                }
            } else {
                self->writer->send_final_chunk(std::move(self->writer));
                self->finalizer(true);
            }
        } else {
            // make sure it will get closed
            self->writer->get_connection()->set_lifecycle(sl::pion::tcp_connection::lifecycle::close);
            self->finalizer(false);
        }
    }

};

} // namespace
}

#endif /* WILTON_SERVER_RESPONSE_STREAM_SENDER_HPP */
