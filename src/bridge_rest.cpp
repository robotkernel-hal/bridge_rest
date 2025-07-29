//! command line interface robotkernel bridge
/*!
 * author: Robert Burger <robert.burger@dlr.de>
 */

/*
 * This file is part of module_ethercat.
 *
 * module_ethercat is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * module_ethercat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with module_ethercat; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "robotkernel/helpers.h"
#include "robotkernel/service.h"
#include "robotkernel/rk_type.h"

#include "bridge_rest.h"

using namespace std;
using namespace robotkernel;

BRIDGE_DEF(bridge_rest, bridge::rest);

using namespace bridge;
using namespace httpserver;

// Checks whether `str' starts with `start' ignoring case
static bool starts_with(const std::string& str, const std::string& start) {
    if (&start == &str) 
        return true; // str and start are the same string

    if (start.length() > str.length()) 
        return false;

    for (size_t i = 0; i < start.length(); ++i) {
        if (start[i] != str[i]) 
            return false;
    }

    return true;
}

rest::rest(const char*& bridgename, YAML::Node& node) :
    bridge_base(bridgename, "bridge_rest", node),
    ws(create_webserver(8080).regex_checking()), res(this)
{
    pthread_mutex_init(&service_map_lock, NULL);
    start();
    
    try {
        ws.register_resource(string("^/.*"), &res);
        ws.register_resource(string("/api/v2.0/list"), &res);
    } catch (std::exception& e) {
        log(error, "caught exception: %s\n", e.what());
    }
}

rest::~rest() {
    pthread_mutex_destroy(&service_map_lock);
    ws.stop();
    stop();
}

void rest::run() {
    log(info, "rest server running!\n");
    ws.start(true);
}

void rest::add_service(const robotkernel::service_t &svc) {
    pthread_mutex_lock(&service_map_lock);
    services.insert(svc.owner + "." + svc.name, svc);
    pthread_mutex_unlock(&service_map_lock);

    ws.register_resource(name, &res, true);
}

void rest::remove_service(const robotkernel::service_t &svc) {
    std::string name = string_printf("/api/v2.0/%s/%s", svc.owner.c_str(), svc.name.c_str());

    pthread_mutex_lock(&service_map_lock);
    services.remove(svc.owner + "." + svc.name);
    pthread_mutex_unlock(&service_map_lock);
}

const std::shared_ptr<http_response> rest::resource::render(const http_request& req) {
    return parent->render(req);
}

const std::shared_ptr<http_response> rest::render(const http_request& req) {
    //uint8_t svc[1024];
    //req.set_data(&svc[0], signature.c_str());
    //uint64_t adr = (uint64_t)&svc[0];
    std::string method = req.get_method();

    log(verbose, "got method \"%s\"\n", method.c_str());

    std::string content = req.get_content();
    log(verbose, "got content \"%s\"\n", content.c_str());
    auto content_node = YAML::Load(content);

//    services.printme("");

    std::string name = req.get_path();
    std::string svc_name = name;
    std::replace(svc_name.begin(), svc_name.end(), '/','.');
    auto rest_svc = services.get_rest_service(svc_name.substr(10));
//    if (rest_svc) {
//        log(verbose, "have rest svc for this node: %s, %p\n", rest_svc->name.c_str(), rest_svc->svc);
//    } else {
//        log(verbose, "NO rest svc for this node\n");
//    }

    
    if (name == "/api/v2.0/list") {
        return list_services(req);
    }

//    auto& _svc = service_map[name];
    if (!rest_svc)
        return list_services(req);
    
    log(verbose, "rendering \"%s\"\n", name.c_str());

    if ((method == "GET") && rest_svc) {
        YAML::Node answer;

        YAML::Node links(YAML::NodeType::Map);

        YAML::Node self_link;
        self_link["href"] = name;
        links["self"] = self_link;

        if (rest_svc->svc) {
            YAML::Node invoke_link;
            invoke_link["href"] = name;
            invoke_link["title"] = "Invoke Resource";
            invoke_link["method"] = "POST";
            invoke_link["type"] = "application/json";
            auto svcdef = YAML::Load(rest_svc->svc->service_definition);
            if (svcdef["request"]) {
                YAML::Node request_node;
                for (const auto& req_seq : svcdef["request"]) {
                    for (const auto& req : req_seq) {
                        YAML::Node tmp;
                        tmp["type"] = req.first;
                        request_node[req.second] = tmp;
                    }
                }
                invoke_link["request_fields"] = request_node;
            }
            if (svcdef["response"]) {
                YAML::Node response_node;
                for (const auto& req_seq : svcdef["response"]) {
                    for (const auto& req : req_seq) {
                        YAML::Node tmp;
                        tmp["type"] = req.first;
                        response_node[req.second] = tmp;
                    }
                }
                invoke_link["response_fields"] = response_node;
            }
            links["invoke"] = invoke_link;
        }

        YAML::Node list_link;
        list_link["href"] = "/api/v2.0/list";
        links["list"] = list_link;

        for (const auto& kv : rest_svc->children) {
            YAML::Node sub_list_link;
            sub_list_link["href"] = name + "/" + kv.first;
            links[kv.first] = sub_list_link;
        }

        answer["_links"] = links;

        YAML::Emitter emitter;
        emitter << YAML::DoubleQuoted << YAML::Flow << YAML::BeginSeq << answer;
        std::string json(emitter.c_str() + 1);

        auto response = std::shared_ptr<http_response>(
                new string_response(json, http::http_utils::http_ok, "application/json; charset=utf-8"));
         response->with_header("access-control-allow-origin", "*");
         return response;
    }

    if (!rest_svc->svc) {
        return list_services(req);
    }
    // request arguments
    robotkernel::service_arglist_t service_request;

    YAML::Node message_definition = YAML::Load(rest_svc->svc->service_definition);
    if (message_definition["request"]) {
        const YAML::Node& request = message_definition["request"];

        for (YAML::const_iterator it = request.begin(); 
                it != request.end(); ++it) {
            for (const auto& kv : *it) {
                string key   = kv.first.as<string>();
                string value = kv.second.as<string>();

                log(verbose, "request field: %s %s\n", key.c_str(), value.c_str());

                if (starts_with(key, "vector")) {
                    const size_t equals_idx = key.find_first_of('/');
                    if (std::string::npos != equals_idx) {
                        key = key.substr(equals_idx + 1);

#define add_vector_type(type) \
                        if (key.compare(#type) == 0) {                        \
                            std::vector<type> entries;                        \
                            for (const auto& value : content_node[value]) {   \
                                entries.push_back(value.as<type>());          \
                            }                                                 \
                            service_request.push_back(entries);               \
                        }
                        add_vector_type(uint64_t)
                        else add_vector_type(int64_t)
                        else add_vector_type(uint32_t)
                        else add_vector_type(int32_t)
                        else add_vector_type(uint16_t)
                        else add_vector_type(int16_t)
                        else add_vector_type(uint8_t)
                        else add_vector_type(int8_t)
                        else add_vector_type(float)
                        else add_vector_type(double)
                        else add_vector_type(char)
                        else add_vector_type(string)
#undef add_vector_type
                    }
                } else {
                    if (content_node[value]) {
                        log(verbose, "  field found in json message\n");
                    } else {
                        log(warning, "  field NOT found in json message\n");
                    }

#define push_back_type2(type, dec_type) \
                    if (key.compare(#type) == 0) {                      \
                        if (content_node[value]) {                      \
                            log(verbose, #type "  pushing to service_request, %d\n", (type)content_node[value].as<dec_type>()); \
                            service_request.push_back((type)content_node[value].as<dec_type>());         \
                        } else {                                        \
                            service_request.push_back((type)0);         \
                        }                                               \
                    }
#define push_back_type(type) \
                    if (key.compare(#type) == 0) {                      \
                        if (content_node[value]) {                      \
                            log(verbose, #type "  pushing to service_request, %d\n", (type)content_node[value].as<type>()); \
                            service_request.push_back((type)content_node[value].as<type>());         \
                        } else {                                        \
                            service_request.push_back((type)0);         \
                        }                                               \
                    }

                    push_back_type(uint64_t)
                    else push_back_type(int64_t)
                    else push_back_type(uint32_t)
                    else push_back_type(int32_t)
                    else push_back_type(uint16_t)
                    else push_back_type(int16_t)
                    else push_back_type2(uint8_t, uint16_t)
                    else push_back_type2(int8_t, int16_t)
                    else push_back_type(float)
                    else push_back_type(double)
                    else push_back_type(string)
#undef push_back_type
#undef push_back_type2
                }
            }
        }
    }

    // call robotkernel service
    robotkernel::service_arglist_t service_response;
    rest_svc->svc->callback(service_request, service_response);

    log(verbose, "service call returned, creating response...\n");

    std::list<uint8_t *> to_delete;

    YAML::Node answer = YAML::Node(YAML::NodeType::Map);

    if (message_definition["response"]) {
        const YAML::Node& response = message_definition["response"];
        int i = 0;

        for (YAML::const_iterator it = response.begin(); 
                it != response.end(); ++it) {
            for (const auto& kv : *it) {
                string key   = kv.first.as<string>();
                string value = kv.second.as<string>();

                log(verbose, "parsing field %s - %s\n", key.c_str(), value.c_str());

                if (starts_with(key, "vector")) {
                    const size_t equals_idx = key.find_first_of('/');
                    if (std::string::npos != equals_idx) {
                        string key = key.substr(equals_idx + 1);

#define push_back_type2(type, type2)                            \
                        if (key.compare(#type) == 0) {                            \
                            const std::vector<type>& elem = service_response[i]; \
                            for (unsigned i = 0; i < elem.size(); ++i) {    \
                                const type2& v = (type)(elem[i]);          \
                                answer[value].push_back(v);     \
                        } }
#define push_back_type(type)                            \
                        if (key.compare(#type) == 0) {                            \
                            const std::vector<type>& elem = service_response[i]; \
                            for (unsigned i = 0; i < elem.size(); ++i) {    \
                                answer[value].push_back((type)(elem[i]));     \
                        } }
                        
                        push_back_type(uint64_t)
                        else push_back_type(int64_t)
                        else push_back_type(uint32_t)
                        else push_back_type(int32_t)
                        else push_back_type(uint16_t)
                        else push_back_type(int16_t)
                        else push_back_type2(uint8_t, uint16_t)
                        else push_back_type2(int8_t, int16_t)
                        else push_back_type(float)
                        else push_back_type(double)
#undef push_back_type
#undef push_back_type2
                        if (key.compare("string") == 0) {
                            const std::vector<string>& elem = service_response[i];
                            for (unsigned j = 0; j < elem.size(); ++j) {
                                string v = elem[j];
                                //v.erase(std::remove(v.begin(), v.end(), '\x00'), v.end());
                                answer[value].push_back(v);
                            } 
                        }

                        i++;
                    }
                } else {
#define push_back_type2(type, type2)                                             \
                    if (key.compare(#type) == 0) {                               \
                        const type2& v = (type)(service_response[i++]);          \
                        answer[value] = v;                                       \
                    }
#define push_back_type(type)                                                     \
                    if (key.compare(#type) == 0) {                                          \
                        const type& v = (type)(service_response[i++]);           \
                        answer[value] = v;                                       \
                    }

                    push_back_type(uint64_t);
                    push_back_type(int64_t);
                    push_back_type(uint32_t);
                    push_back_type(int32_t);
                    push_back_type(uint16_t);
                    push_back_type(int16_t);
                    push_back_type2(uint8_t, uint16_t);
                    push_back_type2(int8_t, int16_t);
                    push_back_type(float);
                    push_back_type(double);
#undef push_back_type
#undef push_back_type2
                    if (key.compare("string") == 0) {
                        string v = service_response[i++];
                        v.erase(std::remove(v.begin(), v.end(), '\x00'), v.end());
                        answer[value] = v;
                    }
                }
            }
        }
    }

    for (std::list<uint8_t *>::iterator it = to_delete.begin();
            it != to_delete.end(); ++it) {
        delete[] (*it);
    }

    YAML::Node links(YAML::NodeType::Sequence);

    YAML::Node self_link;
    self_link["rel"] = "self";
    self_link["href"] = name;
    links.push_back(self_link);

    YAML::Node list_link;
    list_link["rel"] = "list";
    list_link["href"] = "/api/v2.0/list";
    links.push_back(list_link);

    answer["_links"] = links;

    YAML::Emitter emitter;
    emitter << YAML::DoubleQuoted << YAML::Flow << YAML::BeginSeq << answer;
    std::string json(emitter.c_str() + 1);
 
    auto response = std::shared_ptr<http_response>(
            new string_response(json, http::http_utils::http_ok, "application/json; charset=utf-8"));
    response->with_header("Access-Control-Allow-Origin", "*");
    return response;
}

const std::shared_ptr<http_response> rest::list_services(const http_request& req) {
    YAML::Node answer = YAML::Node(YAML::NodeType::Map);

    pthread_mutex_lock(&service_map_lock);

//    for (auto it = service_map.begin(); it != service_map.end(); ++it) {
//        answer["services"].push_back(it->first);
//    }

    pthread_mutex_unlock(&service_map_lock);

    YAML::Emitter emitter;
    emitter << YAML::DoubleQuoted << YAML::Flow << YAML::BeginSeq << answer;
    std::string json(emitter.c_str() + 1);  // Remove beginning [ character

    auto response = std::shared_ptr<http_response>(new string_response(json));
    response->with_header("access-control-allow-origin", "*");
    return response;
}

