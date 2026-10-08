// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/narrative.hpp"
#include <fstream>
#include <algorithm>

namespace cordel::narrative {
Protocol::Protocol(const std::filesystem::path& path) {
    std::ifstream in(path);if(!in) throw std::runtime_error("Missing narrative schema");
    schema_=Json::parse(std::string(std::istreambuf_iterator<char>(in),{}));
}
void Protocol::validate(const Json& m) const {
    if(!m.is("object")) throw std::invalid_argument("Envelope must be object");
    for(const auto& [key,kind]:schema_.at("envelope").object())
        if(!m.contains(key)||!m.at(key).is(kind.string())) throw std::invalid_argument("Wrong or missing field: "+key);
    if(m.at("protocol_version").string()!=version) throw std::invalid_argument("Incompatible narrative protocol");
    const auto& types=schema_.at("messages");
    if(!types.contains(m.at("type").string())) throw std::invalid_argument("Unknown narrative message type");
    for(const auto& key:{"session_id","message_id"}) {
        auto& id=m.at(key).string();
        if(id.empty()||id.size()>128||std::any_of(id.begin(),id.end(),[](unsigned char c){return c>127;}))
            throw std::invalid_argument("Invalid stable identifier");
    }
    if(m.at("sequence").number()<1) throw std::invalid_argument("Invalid sequence");
    auto& spec=types.at(m.at("type").string());
    if(spec.at("correlated").boolean()&&!m.contains("correlation_id")) throw std::invalid_argument("Missing correlation");
    if(m.contains("correlation_id")) {
        const auto& corr=m.at("correlation_id");
        if(!corr.is("string")||corr.string().empty()||corr.string().size()>128) throw std::invalid_argument("Invalid correlation");
    }
    for(const auto& [key,kind]:spec.at("required").object()) {
        const auto& p=m.at("payload");
        if(!p.contains(key)||!p.at(key).is(kind.string())) throw std::invalid_argument("Wrong payload field: "+key);
        if(kind.string()=="integer"&&p.at(key).number()<0) throw std::invalid_argument("Negative ordinal: "+key);
    }
}
}
