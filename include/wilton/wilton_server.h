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
 * File:   wilton_server.hpp
 * Author: alex
 *
 * Created on December 3, 2017, 6:59 PM
 */

#ifndef WILTON_SERVER_HPP
#define WILTON_SERVER_HPP

#include "wilton/wilton.h"

#ifdef __cplusplus
extern "C" {
#endif

struct wilton_Server;
typedef struct wilton_Server wilton_Server;

struct wilton_Request;
typedef struct wilton_Request wilton_Request;

struct wilton_ResponseWriter;
typedef struct wilton_ResponseWriter wilton_ResponseWriter;

struct wilton_HttpPath;
typedef struct wilton_HttpPath wilton_HttpPath;

char* wilton_HttpPath_create(
        wilton_HttpPath** http_path_out,
        const char* method,
        int method_len,
        const char* path,
        int path_len,
        void* handler_ctx,
        void (*handler_cb)(
        void* handler_ctx,
        wilton_Request* request));

char* wilton_HttpPath_destroy(
        wilton_HttpPath* path);

/*
    {
        "numberOfThreads": uint32_t, 
        "tcpPort": uint16_t,
        "ipAddress": "x.x.x.x",
        "ssl": {
            "keyFile": "path/to/file",
            "keyPassword": "pwd",
            "verifyFile": "path/to/file",
            "verifySubjectSubstr": "CN=some_name",
        },
        "documentRoots": [{
            "resource": "/path/to/hanldler",
            "dirPath": "path/to/directory",
            "zipPath": "path/to/directory.zip",
            "zipInnerPrefix": "some/dir",
            "cacheMaxAgeSeconds": uint32_t,
            "mimeTypes": [{
                "extension": ".css",
                "mime": "text/css"
            }, ...]
        }, ...],
        "requestPayload": {
            "tmpDirPath": "path/to/writable/directory",
            "tmpFilenameLength": uint16_t,
            "memoryLimitBytes": uint32_t
        },
        "mustache": {
            "partialsDirs": ["path/to/dir1", "path/to/dir2" ...]
        },
        "rootRedirectLocation": "http://some/url"
    }
 */
char* wilton_Server_create(
        wilton_Server** server_out,
        const char* conf_json,
        int conf_json_len,
        wilton_HttpPath** paths,
        int paths_len);

char* wilton_Server_stop(
        wilton_Server* server);

char* wilton_Server_broadcast_websocket(
        wilton_Server* server,
        const char* path,
        int path_len,
        const char* message,
        int message_length,
        const char* dest_ids_list_json,
        int dest_ids_list_json_len);

/*
// Duplicates in raw headers are handled in the following ways, depending on the header name:
// Duplicates of age, authorization, content-length, content-type, etag, expires, 
// from, host, if-modified-since, if-unmodified-since, last-modified, location, 
// max-forwards, proxy-authorization, referer, retry-after, or user-agent are discarded.
// For all other headers, the values are joined together with ', '.
{
    "httpVersion": "1.1",
    "protocol": "http|https",
    "method": "GET|POST|PUT|DELETE",
    "url": "/path/to/hanldler?param1=val1...",
    "pathname": "/path/to/hanldler",
    "query": "param1=val1...",
    "queries": {
        "param1": "val1",
        "param2": "val21,val22"
        ...
    },
    "headers": {
        "Header-Name": "header_value",
        ...
    }
}
 */
char* wilton_Request_get_request_metadata(
        wilton_Request* request,
        char** metadata_json_out,
        int* metadata_json_len_out);

char* wilton_Request_get_request_data(
        wilton_Request* request,
        char** data_out,
        int* data_len_out);

char* wilton_Request_get_request_form_data(
        wilton_Request* request,
        char** data_out,
        int* data_len_out);

char* wilton_Request_get_request_data_filename(
        wilton_Request* request,
        char** filename_out,
        int* filename_len_out);

/*
{
    "statusCode": uint16_t,
    "statusMessage": "Status message",
    "headers": {
        "Header-Name": "header_value",
        ...
    }
}
 */
char* wilton_Request_set_response_metadata(
        wilton_Request* request,
        const char* metadata_json,
        int metadata_json_len);

char* wilton_Request_send_response(
        wilton_Request* request,
        const char* data,
        int data_len);

char* wilton_Request_send_file(
        wilton_Request* request,
        const char* file_path,
        int file_path_len,
        void* finalizer_ctx,
        void (*finalizer_cb)(
        void* finalizer_ctx,
        int sent_successfully));

char* wilton_Request_send_mustache(
        wilton_Request* request,
        const char* mustache_file_path,
        int mustache_file_path_len,
        const char* values_json,
        int values_json_len);

char* wilton_Request_send_later(
        wilton_Request* request,
        wilton_ResponseWriter** writer_out);

char* wilton_Request_close_websocket(
        wilton_Request* request);

char* wilton_ResponseWriter_set_metadata(
        wilton_ResponseWriter* writer,
        const char* metadata_json,
        int metadata_json_len);

char* wilton_ResponseWriter_send(
        wilton_ResponseWriter* writer,
        const char* data,
        int data_len);


// mustache

char* wilton_render_mustache(
        const char* template_text,
        int template_text_len,
        const char* values_json,
        int values_json_len,
        char** output_text_out,
        int* output_text_len_out);

char* wilton_render_mustache_file(
        const char* template_file_path,
        int template_file_path_len,
        const char* values_json,
        int values_json_len,
        char** output_text_out,
        int* output_text_len_out);

#ifdef __cplusplus
}
#endif

#endif	/* WILTON_SERVER_HPP */

