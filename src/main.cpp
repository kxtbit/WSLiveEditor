#define WIN32_LEAN_AND_MEAN  
#include <winsock2.h>

#include <matdash.hpp>

#include <matdash/minhook.hpp>
#include <matdash/boilerplate.hpp>
#include <fmt/format.h>
#include <gd.h>
#include <support/zip_support/ZipUtils.h>

#define USE_WIN32_CONSOLE
#include <ixwebsocket/IXWebSocketServer.h>
#include "wsle.h"

#include <string>
#include <fstream>
#include <streambuf>
#include "base64.h"

using namespace gd;
using namespace cocos2d;

#include <functional>
#include <vector>


std::vector<std::function<void()>> workFuncs;
std::mutex workMutex;


std::string decompressStr(std::string compressedLvlStr)
{
	if (compressedLvlStr.empty()) return "";

	std::replace(compressedLvlStr.begin(), compressedLvlStr.end(), '_', '/');
	std::replace(compressedLvlStr.begin(), compressedLvlStr.end(), '-', '+');

	std::string decoded = base64_decode(compressedLvlStr);

	unsigned char* data = (unsigned char*)decoded.data();
	unsigned char* a = nullptr;
	ssize_t deflatedLen = ZipUtils::ccInflateMemory(data, decoded.length(), &a);

	std::string levelString((char *)a);

	//robtop changed ccInflateMemory to use new instead of malloc
	delete a;

	return levelString;
}

void runServer()
{
	ix::initNetSystem();
	// Run a server on localhost at a given port.
	// Bound host name, max connections and listen backlog can also be passed in as parameters.
	int port = 1313;
	std::string host("127.0.0.1"); // If you need this server to be accessible on a different machine, use "0.0.0.0"
	ix::WebSocketServer server(port, host);

	server.setOnClientMessageCallback([&server](std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg)
	{
		// The ConnectionState object contains information about the connection,
		// at this point only the client ip address and the port.

		if (msg->type == ix::WebSocketMessageType::Open)
		{
			webSocket.send("hello there");
			puts("new connection opened");
		}
		else if (msg->type == ix::WebSocketMessageType::Message)
		{
			if(!gd::LevelEditorLayer::get())
			{
				return wsle::sendResult({ false, "User is not in the editor" }, &webSocket);
			}
			
			try
			{
				bool gzip = msg->str.starts_with("H4sIAAAAAAAA");
				json::jobject result = json::jobject::parse(gzip ? decompressStr(msg->str) : msg->str);
				wsle::handle(result, &webSocket);
			}
			catch(std::exception& e)
			{
				std::cout << "exception: " << e.what() << '\n';
				wsle::sendResult({false, e.what()}, &webSocket);
			}
		}
		else if(msg->type == ix::WebSocketMessageType::Close)
		{
		}
	});

	auto res = server.listen();
	if (!res.first)
	{
		std::cout << res.second << '\n';
		return;
	}
	
	std::cout << "Server started correctly\n";
	// Per message deflate connection is enabled by default. It can be disabled
	// which might be helpful when running on low power devices such as a Rasbery Pi
	server.disablePerMessageDeflate();

	// Run the server in the background. Server can be stoped by calling server.stop()
	server.start();

	// Block until server.stop() is called.
	server.wait();

	ix::uninitNetSystem();
}

class MenuLayerMod : public MenuLayer {
public:
	// here the name cant be `init` as that would make it a virtual
	// which doesnt work with the current code
	bool init_() {
		if (!matdash::orig<&MenuLayerMod::init_>(this)) return false;
		
		//std::string str = fmt::format("Hello from {}", "xmake");
		//auto label = CCLabelBMFont::create(str.c_str(), "bigFont.fnt");
		//label->setPosition(ccp(200, 200));
		//addChild(label);
		
		std::thread([]{ runServer(); }).detach();

		return true;
	}
};


void (__thiscall* LevelEditorLayer_updateO)(void*, float);
void __fastcall LevelEditorLayer_updateH(LevelEditorLayer* self, void* edx, float dt)
{
	workMutex.lock();
	if(!workFuncs.empty())
	{
		for(const auto& f : workFuncs)
		{
			puts("calling action function");
			f();
		}
		workFuncs.clear();
	}
	workMutex.unlock();
	
	LevelEditorLayer_updateO(self, dt);
	
}


void dese(EditorUI* ui, void* sender)
{
	puts("helloi world");
	std::ifstream t("C:\\Users\\marca\\Desktop\\projects\\WSLiveEditor\\message.txt");
	std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
	std::vector<std::string> objs = wsle::splitByDelim(str, ';');
	
	auto editor = gd::LevelEditorLayer::get();
	int i = 0;
	for(const auto& s : objs)
	{
		editor->addObjectFromString(s);
	}
/*
	printf("str: %s\n", str.c_str());

	try
	{
		bool gzip = str.starts_with("H4sIAAAAAAAA");
		json::jobject result = json::jobject::parse(gzip ? decompressStr(str) : str);
		wsle::handle(result, nullptr);
	}
	catch(std::exception& e)
	{
		std::cout << "exception: " << e.what();
	}
	*/
	//gd::LevelEditorLayer::get()->createObjectsFromSetup(str);
}

void wsle::queueAction(const std::function<void()>& func)
{
	workMutex.lock();
	workFuncs.push_back(func);
	workMutex.unlock();
}


void mod_main(HMODULE) {
	

	#ifdef USE_WIN32_CONSOLE
		if(AllocConsole()) {
			freopen("CONOUT$", "wt", stdout);
			freopen("CONIN$", "rt", stdin);
			freopen("CONOUT$", "w", stderr);
			std::ios::sync_with_stdio(1);
		}
	#endif
	
	puts("WSLiveEditor 1.0 | Debug Console");
	
	matdash::add_hook<&MenuLayerMod::init_>(base + 0x1907b0);
	matdash::add_hook<&dese>(base + 0x87340);
	
	MH_CreateHook(
		reinterpret_cast<void*>(gd::base + 0x1632b0),
		reinterpret_cast<void*>(&LevelEditorLayer_updateH),
		reinterpret_cast<void**>(&LevelEditorLayer_updateO)
	);
	MH_EnableHook(reinterpret_cast<void*>(gd::base + 0x1632b0));

}