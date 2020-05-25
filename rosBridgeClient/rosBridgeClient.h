#include <iostream>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <thread>
#include <vector>
#include "json.hpp"

#pragma comment(lib, "ws2_32.lib")

namespace ekrem {
	class ROSBridgeClient {
	private:
		SOCKET sock;
		int maxIncomingMessageSize;

		bool activelyListening;
		std::thread* mainListener;
		
		std::map<std::string, void(*)(nlohmann::json)> topicsCallbackMap;
		std::map<std::string, uint8_t> topicsActiveThreadMap; // 1 for active, 2 for inactive

		std::vector<std::thread*> allThreads;
		std::vector<bool> isThreadWorking;
		std::vector<bool> isThreadDead;

	public:
		ROSBridgeClient(std::string ipAddress, int port);
		void destroy();

		bool sendString(std::string message);

		void advertise(std::string topic, std::string type);
		bool subscribe(std::string topic, std::string type, int msgSize, void (*callback)(nlohmann::json));

		bool publish(std::string topic, nlohmann::json msg);

		void serverListener();

		void callCallback(std::string topic, nlohmann::json msg, uint8_t threadId);

		void killZombies();
		
	};

	
}