#include "TitleManagerWrapper.h"
#include <sstream>

using namespace v8;
using namespace std;

// GS2 callbacks

static void GS2Command(void* owner, int cmd) {
  TitleManagerWrapper *sender = (TitleManagerWrapper*)owner;
  Nan::HandleScope scope;
  v8::Local<v8::Value> argv[2] = {
    Nan::New("command").ToLocalChecked(),
    Nan::New<Number>(cmd)
  };
  Nan::MakeCallback(sender->handle(), "emit", 2, argv);
}

static void GS2Disconnect(void* owner) {
  TitleManagerWrapper *sender = (TitleManagerWrapper*)owner;
  Nan::HandleScope scope;
  v8::Local<v8::Value> argv[1] = {
    Nan::New("disconnect").ToLocalChecked()
  };
  Nan::MakeCallback(sender->handle(), "emit", 1, argv);
}

static void GS2PickResult(void* owner, BSTR xmlResult) {
  TitleManagerWrapper *sender = (TitleManagerWrapper*)owner;
  Nan::HandleScope scope;
  v8::Local<v8::Value> argv[2] = {
    Nan::New("pickResult").ToLocalChecked(),
    Nan::New((uint16_t*)xmlResult).ToLocalChecked()
  };
  Nan::MakeCallback(sender->handle(), "emit", 2, argv);
}

static void GS2ZoneStateChange(void* owner, BSTR zoneName, bool zoneState) {
  TitleManagerWrapper *sender = (TitleManagerWrapper*)owner;
  Nan::HandleScope scope;
  v8::Local<v8::Value> argv[3] = {
    Nan::New("zoneStateChanged").ToLocalChecked(),
    Nan::New((uint16_t*)zoneName).ToLocalChecked(),
    Nan::New(zoneState)
  };
  Nan::MakeCallback(sender->handle(), "emit", 3, argv);
}

static void GS2Message(void* owner, BSTR msg) {
  TitleManagerWrapper *sender = (TitleManagerWrapper*)owner;
  Nan::HandleScope scope;
  v8::Local<v8::Value> argv[2] = {
    Nan::New("message").ToLocalChecked(),
    Nan::New((uint16_t*)msg).ToLocalChecked()
  };
  Nan::MakeCallback(sender->handle(), "emit", 2, argv);
}

// You could also take an existing vector as a parameter.
vector<string> split(string str, char delimiter) {
  vector<string> internal;
  stringstream ss(str); // Turn the string into a stream.
  string tok;

  while (getline(ss, tok, delimiter)) {
    internal.push_back(tok);
  }

  return internal;
}

TitleManagerWrapper::TitleManagerWrapper()
{
  CoInitialize(NULL);

  //moduleDir includes build/release, remove last two folders to find module root
  vector<string> v = split(*moduleDir, '\\');
  string path("");
  for (size_t i = 0; i < v.size() - 2; i++) {
    path.append(v[i].append("\\"));
  }

  ACTCTX actCtx;
  memset((void*)&actCtx, 0, sizeof(ACTCTX));
  actCtx.cbSize = sizeof(ACTCTX);
#if defined(_M_X64)
  actCtx.lpSource = path.append("TitleManager.x.64.manifest").c_str(); 
#else
  actCtx.lpSource = path.append("TitleManager.x.32.manifest").c_str();
#endif

  HANDLE hCtx = ::CreateActCtx(&actCtx);
  if (hCtx == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    cout << "CreateActCtx returned: INVALID_HANDLE_VALUE"
      << err
      << endl;
  } 
  else
  {
    ULONG_PTR cookie;
    if (::ActivateActCtx(hCtx, &cookie))
    {
      HRESULT hr = CoCreateInstance(CLSID_TitleManager, NULL, CLSCTX_ALL, IID_IGSTitleManager, (void **)&ptm);
      ::DeactivateActCtx(0, cookie);
    }
  }
  
  HRESULT hr;
  // side-by-side failed, try to create titlemanager the regular way..
  if (ptm == NULL) {
    hr = CoCreateInstance(CLSID_TitleManager, NULL, CLSCTX_ALL, IID_IGSTitleManager, (void **)&ptm);
  }

  if (ptm == NULL) {
    Nan::ThrowError("GSTK5 cannot be instantiated");
    return;
  }

  IGSClient* client;
  hr = ptm->getClient(&client);
  client->createAsyncProcessor(&asyncProc);

  EventWorker::Run(asyncProc);

  eventSink = new CmdEventSink((IDispatch*)client, (void*)this);
  eventSink->onCommandEvent = &GS2Command;
  eventSink->onDisconnectEvent = &GS2Disconnect;
  eventSink->onPickResultEvent = &GS2PickResult;
  eventSink->onZoneChangeEvent = &GS2ZoneStateChange;
  eventSink->onMessageEvent = &GS2Message;

  hr = ptm->QueryInterface(IID_ISupportErrorInfo, (void **)&supportErrorInfo);
  if SUCCEEDED(hr) {
    hr = supportErrorInfo->InterfaceSupportsErrorInfo(IID_IGSTitleManager);
  }
}

TitleManagerWrapper::~TitleManagerWrapper()
{
  if (ptm) { 
    asyncProc->close();
    
    if (eventSink) {
      eventSink->DisconnectSink();
      eventSink = NULL;
    }

    ptm->Release();
    CoUninitialize();
  }
}
