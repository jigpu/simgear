
#include <cstdio>
#include <cstring>

#include <unistd.h> // for STDOUT_FILENO
#include <iostream>
#include <boost/foreach.hpp>

#include <simgear/io/sg_file.hxx>
#include <simgear/io/HTTPClient.hxx>
#include <simgear/io/HTTPRequest.hxx>
#include <simgear/io/sg_netChannel.hxx>
#include <simgear/misc/strutils.hxx>

using namespace simgear;
using std::cout;
using std::endl;
using std::cerr;
using std::string;

class ARequest : public HTTP::Request
{
public:
    ARequest(string& url) :
        Request(url),
        _complete(false),
        _file(NULL)
    {
        
    }
    
    void setFile(SGFile* f)
    {
        _file = f;
    }
    
    bool complete() const
        { return _complete; }
        
    void addHeader(const string& h)
    {
        int colonPos = h.find(':');
        if (colonPos < 0) {
            cerr << "malformed header: " << h << endl;
            return;
        }
        
        string key = h.substr(0, colonPos);
        _headers[key] = h.substr(colonPos + 1);
    }
    
    virtual string_list requestHeaders() const
    {
        string_list r;
        std::map<string, string>::const_iterator it;
        for (it = _headers.begin(); it != _headers.end(); ++it) {
            r.push_back(it->first);
        }
        
        return r;
    }
    
    virtual string header(const string& name) const
    {
        std::map<string, string>::const_iterator it = _headers.find(name);
        if (it == _headers.end()) {
            return string();
        }
        
        return it->second;
    }
protected:
    virtual void responseHeadersComplete()
    {
    }

    virtual void responseComplete()
    {
        _complete = true;
    }  

    virtual void gotBodyData(const char* s, int n)
    {
        _file->write(s, n);
    }
private:    
    bool _complete;
    SGFile* _file;
    std::map<string, string> _headers;
};

int main(int argc, char* argv[])
{
    HTTP::Client cl;
    SGFile* outFile;
    string proxy, proxyAuth;
    string_list headers;
    string url;
    
    for (int a=0; a<argc;++a) {
        if (argv[a][0] == '-') {
            if (!strcmp(argv[a], "--user-agent")) {
                cl.setUserAgent(argv[++a]);
            } else if (!strcmp(argv[a], "--proxy")) {
                proxy = argv[++a];
            } else if (!strcmp(argv[a], "--auth")) {
                proxyAuth = argv[++a];
            } else if (!strcmp(argv[a], "-f") || !strcmp(argv[a], "--file")) {
                outFile = new SGFile(argv[++a]);
                if (!outFile->open(SG_IO_OUT)) {
                    cerr << "failed to open output for writing:" << outFile->get_file_name() << endl;
                    return EXIT_FAILURE;
                }
            } else if (!strcmp(argv[a], "--header")) {
                headers.push_back(argv[++a]);
            }
        } else { // of argument starts with a hyphen
            url = argv[a];
        }
    } // of arguments iteration

    if (!proxy.empty()) {
        int colonPos = proxy.find(':');
        string proxyHost = proxy;
        int proxyPort = 8800;
        if (colonPos >= 0) {
            proxyHost = proxy.substr(0, colonPos);
            proxyPort = strutils::to_int(proxy.substr(colonPos + 1));
            cout << proxyHost << " " << proxyPort << endl;
        }
        
        cl.setProxy(proxyHost, proxyPort, proxyAuth);
    }

    if (!outFile) {
        outFile = new SGFile(STDOUT_FILENO);
    }

    if (url.empty()) {
        cerr << "no URL argument specificed" << endl;
        return EXIT_FAILURE;
    }

    ARequest* req = new ARequest(url);
    BOOST_FOREACH(string h, headers) {
        req->addHeader(h);
    }
    
    req->setFile(outFile);
    cl.makeRequest(req);
    
    while (!req->complete()) {
        NetChannel::poll(100);
    }
    
    if (req->responseCode() != 200) {
        cerr << "got response:" << req->responseCode() << endl;
        cerr << "\treason:" << req->responseReason() << endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}