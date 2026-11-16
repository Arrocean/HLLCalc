// SPDX-License-Identifier: GPL-3.0-only
#include "update/update.h"
#include "update/update_core.h"
#include "update/update_internal.h"
#include <winhttp.h>
#include <algorithm>
#include <atomic>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace updater {
namespace {
struct Internet {HINTERNET handle=nullptr;~Internet(){if(handle)WinHttpCloseHandle(handle);}explicit Internet(HINTERNET value=nullptr):handle(value){}Internet(const Internet&)=delete;Internet& operator=(const Internet&)=delete;};
struct Cancelled:std::runtime_error{Cancelled():std::runtime_error("Cancelled") {}};
struct Manager {
    std::mutex stateMutex,lifecycle;
    Snapshot snapshot;
    detail::Release release;
    detail::Prepared prepared;
    std::string repo;
    std::wstring exe;
    std::thread worker;
    std::atomic<bool> cancelled{false},running{false};
    std::atomic<HINTERNET> request{nullptr};
    bool initialized=false;
    ~Manager(){cancelled=true;auto h=request.exchange(nullptr);if(h)WinHttpCloseHandle(h);if(worker.joinable())worker.join();}
};
Manager& State(){static Manager state;return state;}
void CheckCancelled(Manager& state){if(state.cancelled)throw Cancelled();}
void Error(Manager& state,const std::string& text){std::lock_guard<std::mutex> lock(state.stateMutex);state.snapshot.phase=state.cancelled?Phase::Cancelled:Phase::Error;state.snapshot.error=state.cancelled?"":text;}
std::vector<unsigned char> Fetch(Manager& state,std::string url,std::uint64_t limit,std::uint64_t expected=0,const std::function<void(std::uint64_t)>& progress={}) {
    Internet session(WinHttpOpen(L"HLLCalc-Updater/1.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.handle)throw std::runtime_error("Cannot initialize HTTPS");
    WinHttpSetTimeouts(session.handle,5000,10000,10000,15000);
    const auto deadline=GetTickCount64()+120000;
    for(unsigned redirect=0;redirect<=5;++redirect) {
        CheckCancelled(state);
        if(GetTickCount64()>=deadline)throw std::runtime_error("Update download timeout");
        if(!detail::AllowedHttpsUrl(url))throw std::runtime_error("Update URL is not an allowed GitHub HTTPS host");
        auto wide=detail::Wide(url);URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=static_cast<DWORD>(-1);parts.dwUrlPathLength=static_cast<DWORD>(-1);parts.dwExtraInfoLength=static_cast<DWORD>(-1);parts.dwUserNameLength=static_cast<DWORD>(-1);parts.dwPasswordLength=static_cast<DWORD>(-1);
        if(!WinHttpCrackUrl(wide.c_str(),static_cast<DWORD>(wide.size()),0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS||parts.nPort!=443||parts.dwUserNameLength||parts.dwPasswordLength)throw std::runtime_error("Invalid HTTPS update URL");
        std::wstring host(parts.lpszHostName,parts.dwHostNameLength),path(parts.lpszUrlPath,parts.dwUrlPathLength);if(path.empty())path=L"/";if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
        Internet connection(WinHttpConnect(session.handle,host.c_str(),parts.nPort,0));
        if(!connection.handle)throw std::runtime_error("Cannot connect to GitHub");
        HINTERNET request=WinHttpOpenRequest(connection.handle,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
        if(!request)throw std::runtime_error("Cannot create HTTPS request");
        state.request.store(request);
        struct Active {Manager& state;HINTERNET handle;~Active(){auto expected=handle;if(state.request.compare_exchange_strong(expected,nullptr))WinHttpCloseHandle(handle);}} active{state,request};
        DWORD noRedirect=WINHTTP_DISABLE_REDIRECTS;
        if(!WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&noRedirect,sizeof(noRedirect)))throw std::runtime_error("Cannot enforce HTTPS redirect policy");
        DWORD logon=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;WinHttpSetOption(request,WINHTTP_OPTION_AUTOLOGON_POLICY,&logon,sizeof(logon));
        const wchar_t* headers=L"Accept: application/vnd.github+json, application/octet-stream\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
        if(!WinHttpSendRequest(request,headers,static_cast<DWORD>(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr)){CheckCancelled(state);throw std::runtime_error("GitHub HTTPS request failed or timed out");}
        DWORD status=0,size=sizeof(status);
        if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("Cannot read HTTPS status");
        if(status==301||status==302||status==303||status==307||status==308) {
            if(redirect==5)throw std::runtime_error("Too many update redirects");
            wchar_t location[4097]{};DWORD bytes=sizeof(location);
            if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location,&bytes,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("Invalid HTTPS redirect");
            std::string next=detail::Utf8(location);
            if(!next.empty()&&next[0]=='/'&&(next.size()<2||next[1]!='/'))next="https://"+detail::Utf8(host)+next;
            if(!detail::AllowedHttpsUrl(next))throw std::runtime_error("Redirect outside trusted GitHub asset hosts");
            url=std::move(next);continue;
        }
        if(status!=200)throw std::runtime_error("GitHub returned HTTP "+std::to_string(status)+(status==403?" (API rate limit may apply)":""));
        wchar_t length[64]{};DWORD lengthBytes=sizeof(length);
        if(WinHttpQueryHeaders(request,WINHTTP_QUERY_CONTENT_LENGTH,WINHTTP_HEADER_NAME_BY_INDEX,length,&lengthBytes,WINHTTP_NO_HEADER_INDEX)) {
            wchar_t* end=nullptr;auto n=wcstoull(length,&end,10);if(end==length||*end||n>limit||(expected&&n!=expected))throw std::runtime_error("Unexpected update download size");
        }
        std::vector<unsigned char> result;result.reserve(static_cast<size_t>((std::min)(expected?expected:65536ull,limit)));
        unsigned char buffer[65536];
        for(;;) {
            CheckCancelled(state);if(GetTickCount64()>=deadline)throw std::runtime_error("Update download timeout");
            DWORD read=0;if(!WinHttpReadData(request,buffer,sizeof(buffer),&read)){CheckCancelled(state);throw std::runtime_error("Update download interrupted");}
            if(!read)break;if(read>limit-result.size())throw std::runtime_error("Update download exceeds size limit");
            result.insert(result.end(),buffer,buffer+read);if(progress)progress(result.size());
        }
        if(expected&&result.size()!=expected)throw std::runtime_error("Truncated update asset");
        return result;
    }
    throw std::runtime_error("Update redirect limit");
}
template<class Work> void Start(Manager& state,Phase phase,Work work) {
    std::lock_guard<std::mutex> lifecycle(state.lifecycle);
    if(!state.initialized||state.running)return;
    if(state.worker.joinable())state.worker.join();
    state.cancelled=false;state.running=true;
    {std::lock_guard<std::mutex> lock(state.stateMutex);state.snapshot.phase=phase;state.snapshot.error.clear();state.snapshot.downloaded=0;state.snapshot.total=phase==Phase::Downloading?state.release.archive.size:0;}
    try {state.worker=std::thread([&state,work](){try{work();}catch(const Cancelled&){std::lock_guard<std::mutex> lock(state.stateMutex);state.snapshot.phase=Phase::Cancelled;state.snapshot.error.clear();}catch(const std::exception& e){Error(state,e.what());}state.running=false;});}
    catch(const std::exception& e){state.running=false;Error(state,e.what());}
}
}
bool Initialize(const std::string& version,const std::wstring& path,const std::string& repo) {
    Shutdown();auto& state=State();
    try {
        detail::Version parsed;if(!detail::ParseVersion(version,parsed)||!detail::ValidRepository(repo))throw std::runtime_error("Invalid application version or update repository");
        const auto full=detail::FullPath(path);if(_wcsicmp(full.c_str(),detail::ModulePath().c_str()))throw std::runtime_error("Updater path is not the running executable");
        std::lock_guard<std::mutex> lifecycle(state.lifecycle);state.exe=full;state.repo=repo;state.initialized=true;
        {std::lock_guard<std::mutex> lock(state.stateMutex);state.snapshot={};state.snapshot.currentVersion=version;}
        return true;
    } catch(const std::exception& e){Error(state,e.what());return false;}
}
void CheckAsync() {
    auto& state=State();
    Start(state,Phase::Checking,[&state](){
        detail::RemoveStage(state.prepared.stage);state.prepared={};
        auto data=Fetch(state,"https://api.github.com/repos/"+state.repo+"/releases/latest",2*1024*1024);
        detail::Release release;std::string error;if(!detail::ParseRelease(std::string(data.begin(),data.end()),state.repo,release,error))throw std::runtime_error(error);
        detail::Version current,latest;{std::lock_guard<std::mutex> lock(state.stateMutex);detail::ParseVersion(state.snapshot.currentVersion,current);}
        detail::ParseVersion(release.version,latest);CheckCancelled(state);
        std::lock_guard<std::mutex> lock(state.stateMutex);state.release=release;state.snapshot.latestVersion=release.version;state.snapshot.releaseUrl=release.url;state.snapshot.notes=release.notes;state.snapshot.phase=detail::CompareVersions(latest,current)>0?Phase::Available:Phase::UpToDate;
    });
}
void DownloadAsync() {
    auto& state=State();
    {std::lock_guard<std::mutex> lock(state.stateMutex);if(state.snapshot.phase!=Phase::Available&&state.snapshot.phase!=Phase::Error&&state.snapshot.phase!=Phase::Cancelled)return;if(state.release.version.empty())return;}
    Start(state,Phase::Downloading,[&state](){
        detail::RemoveStage(state.prepared.stage);state.prepared={};detail::Release release;
        {std::lock_guard<std::mutex> lock(state.stateMutex);release=state.release;}
        auto sums=Fetch(state,release.checksums.url,65536,release.checksums.size);
        std::string digest,error;if(!detail::ParseChecksum(std::string(sums.begin(),sums.end()),release.archive.name,digest,error))throw std::runtime_error(error);
        auto zip=Fetch(state,release.archive.url,detail::MaxArchiveBytes,release.archive.size,[&state](std::uint64_t count){std::lock_guard<std::mutex> lock(state.stateMutex);state.snapshot.downloaded=count;});
        if(detail::HashBytes(zip)!=digest)throw std::runtime_error("SHA-256 mismatch; update was not prepared");
        std::vector<std::string> files;if(!detail::ValidateArchive(zip,files,error))throw std::runtime_error(error);
        CheckCancelled(state);auto stage=detail::CreateStage();
        try {
            detail::WriteFileBytes(stage+L"\\update.zip",zip);detail::ExtractPackage(stage,state.cancelled);CheckCancelled(state);
            detail::ValidateExecutable(stage+L"\\payload\\HLLCalc.exe",release.version);
            detail::Prepared ready{stage,state.exe,release.version,files};
            std::lock_guard<std::mutex> lock(state.stateMutex);state.prepared=std::move(ready);state.snapshot.phase=Phase::Ready;state.snapshot.downloaded=release.archive.size;
        } catch(...){detail::RemoveStage(stage);throw;}
    });
}
void AcknowledgeReady(){auto& state=State();std::wstring exe;std::string version;{std::lock_guard<std::mutex> lock(state.stateMutex);if(!state.initialized)return;exe=state.exe;version=state.snapshot.currentVersion;}detail::AcknowledgeStartup(exe,version);}
Snapshot GetSnapshot(){auto& state=State();std::lock_guard<std::mutex> lock(state.stateMutex);return state.snapshot;}
void Cancel(){auto& state=State();state.cancelled=true;auto request=state.request.exchange(nullptr);if(request)WinHttpCloseHandle(request);std::lock_guard<std::mutex> lock(state.stateMutex);if(state.snapshot.phase!=Phase::Launching&&state.snapshot.phase!=Phase::Idle&&state.snapshot.phase!=Phase::UpToDate)state.snapshot.phase=Phase::Cancelled;}
void Shutdown(){auto& state=State();std::lock_guard<std::mutex> lifecycle(state.lifecycle);state.cancelled=true;auto request=state.request.exchange(nullptr);if(request)WinHttpCloseHandle(request);if(state.worker.joinable())state.worker.join();state.running=false;bool keep=false;{std::lock_guard<std::mutex> lock(state.stateMutex);keep=state.snapshot.phase==Phase::Launching;state.initialized=false;}if(!keep)detail::RemoveStage(state.prepared.stage);state.prepared={};}
bool Install(){auto& state=State();std::lock_guard<std::mutex> lifecycle(state.lifecycle);if(state.running)return false;detail::Prepared ready;{std::lock_guard<std::mutex> lock(state.stateMutex);if(state.snapshot.phase!=Phase::Ready)return false;ready=state.prepared;}
    std::string error;const bool launched=detail::LaunchInstaller(ready,error);std::lock_guard<std::mutex> lock(state.stateMutex);state.snapshot.error=error;if(launched)state.snapshot.phase=Phase::Launching;return launched;
}
}
