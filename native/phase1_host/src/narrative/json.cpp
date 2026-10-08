// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/json.hpp"
#include <charconv>
#include <cctype>

namespace cordel {
namespace {
class Parser {
    const std::string& s;
    std::size_t p{};
    [[noreturn]] void bad() const { throw std::invalid_argument("Invalid JSON at byte "+std::to_string(p)); }
    char take() { if(p==s.size()) bad();return s[p++]; }
    void ws() { while(p<s.size()&&(s[p]==' '||s[p]=='\n'||s[p]=='\r'||s[p]=='\t')) ++p; }
    unsigned hex4() {
        unsigned v=0;
        for(int i=0;i<4;++i) {
            char c=take();unsigned d;
            if(c>='0'&&c<='9') d=unsigned(c-'0');
            else if(c>='a'&&c<='f') d=unsigned(c-'a'+10);
            else if(c>='A'&&c<='F') d=unsigned(c-'A'+10);
            else bad();
            v=v*16+d;
        }
        return v;
    }
    static void utf8(std::string& out,unsigned v) {
        if(v<128) out+=char(v);
        else if(v<2048) { out+=char(0xc0|(v>>6));out+=char(0x80|(v&63)); }
        else if(v<65536) { out+=char(0xe0|(v>>12));out+=char(0x80|((v>>6)&63));out+=char(0x80|(v&63)); }
        else { out+=char(0xf0|(v>>18));out+=char(0x80|((v>>12)&63));out+=char(0x80|((v>>6)&63));out+=char(0x80|(v&63)); }
    }
    std::string str() {
        if(take()!='"') bad();
        std::string out;
        while(true) {
            unsigned char c=static_cast<unsigned char>(take());
            if(c=='"') return out;
            if(c<32) bad();
            if(c=='\\') {
                char e=take();
                switch(e) {
                    case '"':case '\\':case '/':out+=e;break;
                    case 'b':out+='\b';break;case 'f':out+='\f';break;
                    case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
                    case 'u': {
                        unsigned v=hex4();
                        if(v>=0xd800&&v<=0xdbff) {
                            if(take()!='\\'||take()!='u') bad();
                            unsigned low=hex4();if(low<0xdc00||low>0xdfff) bad();
                            v=65536+((v-0xd800)<<10)+(low-0xdc00);
                        } else if(v>=0xdc00&&v<=0xdfff) bad();
                        utf8(out,v);break;
                    }
                    default:bad();
                }
            } else if(c>=128) {
                // Validate raw UTF-8, including overlong, surrogate and range checks.
                unsigned count=0,v=0,min=0;
                if(c>=0xc2&&c<=0xdf) {count=1;v=c&31;min=128;}
                else if(c>=0xe0&&c<=0xef) {count=2;v=c&15;min=2048;}
                else if(c>=0xf0&&c<=0xf4) {count=3;v=c&7;min=65536;}
                else bad();
                out+=char(c);
                for(unsigned i=0;i<count;++i) {
                    auto b=static_cast<unsigned char>(take());if((b&0xc0)!=0x80) bad();
                    out+=char(b);v=(v<<6)|(b&63);
                }
                if(v<min||v>0x10ffff||(v>=0xd800&&v<=0xdfff)) bad();
            } else out+=char(c);
        }
    }
    Json value(unsigned depth) {
        if(depth>32) bad();
        ws();if(p==s.size()) bad();char c=s[p];
        if(c=='"') return str();
        if(c=='{'||c=='[') {
            ++p;ws();bool obj=c=='{';char close=obj?'}':']';
            Json::Object object;Json::Array array;
            if(p<s.size()&&s[p]==close) {++p;return obj?Json(object):Json(array);}
            while(true) {
                ws();std::string key;
                if(obj) {key=str();ws();if(take()!=':') bad();}
                auto item=value(depth+1);
                if(obj) {if(!object.emplace(key,std::move(item)).second) bad();} else array.push_back(std::move(item));
                ws();char end=take();if(end==close) break;if(end!=',') bad();
            }
            return obj?Json(object):Json(array);
        }
        for(const auto& literal:{"true","false","null"}) {
            std::string word=literal;
            if(s.compare(p,word.size(),word)==0) {p+=word.size();if(word=="null") return Json();return Json(word=="true");}
        }
        std::size_t start=p;
        if(c=='-') ++p;
        if(p>=s.size()) bad();
        if(s[p]=='0') ++p;
        else {if(s[p]<'1'||s[p]>'9') bad();while(p<s.size()&&std::isdigit(static_cast<unsigned char>(s[p]))) ++p;}
        if(p<s.size()&&s[p]=='.') {
            ++p;auto before=p;while(p<s.size()&&std::isdigit(static_cast<unsigned char>(s[p]))) ++p;if(before==p) bad();
        }
        if(p<s.size()&&(s[p]=='e'||s[p]=='E')) {
            ++p;if(p<s.size()&&(s[p]=='+'||s[p]=='-')) ++p;
            auto before=p;while(p<s.size()&&std::isdigit(static_cast<unsigned char>(s[p]))) ++p;if(before==p) bad();
        }
        double n=0;auto [end,error]=std::from_chars(s.data()+start,s.data()+p,n);
        if(error!=std::errc{}||end!=s.data()+p||!std::isfinite(n)) bad();
        return n;
    }
public:
    explicit Parser(const std::string& text):s(text) {}
    Json run() {auto result=value(0);ws();if(p!=s.size()) bad();return result;}
};
}
Json Json::parse(const std::string& text) {
    if(text.size()>16384) throw std::invalid_argument("JSON framing limit exceeded");
    return Parser(text).run();
}
}
