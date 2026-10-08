// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/narrative.hpp"
#include <iostream>
using namespace cordel;
int main() {
    int checks=0;
    auto check=[&](bool b){++checks;if(!b) throw std::runtime_error("Protocol assertion failed "+std::to_string(checks));};
    auto invalid=[&](const std::string& s){try {Json::parse(s);check(false);}catch(const std::invalid_argument&) {check(true);}};
    try {
        narrative::Protocol protocol(CORDEL_NARRATIVE_SCHEMA);
        Json m=Json::Object{{"protocol_version",narrative::version},{"session_id","control"},{"message_id","n-1"},
            {"sequence",1},{"type","hello"},{"payload",Json::Object{{"client","CORDEL"}}}};
        check(protocol.decode(m.dump()).dump()==m.dump());
        check(Json::parse(Json(9007199254740991.).dump()).number()==9007199254740991.);
        check(Json::parse("\"\\u00e9\\ud83d\\ude00\"").string()=="é😀");
        check(Json::parse("[-1.5e2,true,false,null,{}]").array().size()==5);
        for(const auto& s:{"{","[1,]","01","NaN","1e9999","\"\\ud800\"","\"\\udc00\"","{\"a\":1,\"a\":2}","true false"}) invalid(s);
        invalid(std::string(40,'[')+std::string(40,']'));invalid(std::string(16385,' '));invalid(std::string("\"\xc0\xaf\""));
        for(const auto& [key,value]:Json::Object{{"protocol_version","wrong"},{"type","unknown"},{"payload",1},{"sequence",0},{"message_id",""}}) {
            auto fields=m.object();fields[key]=value;
            try {protocol.validate(fields);check(false);}catch(const std::invalid_argument&) {check(true);}
        }
        auto fields=m.object();fields["type"]="dialogue_ack";
        try {protocol.validate(fields);check(false);}catch(const std::invalid_argument&) {check(true);}
        fields["correlation_id"]="w-3";protocol.validate(fields);check(true);
        std::cout<<checks<<" native protocol/JSON assertions passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
