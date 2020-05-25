#include "rosBridgeClient.h"

#define SLEEP_TIME_AFTER_SEND 1


ekrem::ROSBridgeClient::ROSBridgeClient(std::string ipAddress, int port)
{
	//Initialize Winsock
	WSADATA data;
	WORD ver = MAKEWORD(2, 2);
	int wsResult = WSAStartup(ver, &data);
	if (wsResult != 0) {
		std::cerr << "winsock error" << std::endl;
	}

	//Create socket
	this->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (this->sock == INVALID_SOCKET) {
		std::cerr << "Can't create socket" << std::endl;
		WSACleanup();
	}

	//Fill in a hint structure
	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(port);
	inet_pton(AF_INET, ipAddress.c_str(), &hint.sin_addr);

	//Connect to server
	int connResult = connect(this->sock, (sockaddr*)&hint, sizeof(hint));
	if (connResult == SOCKET_ERROR) {
		std::cerr << "Cant connect to server " << std::endl;
		closesocket(this->sock);
		WSACleanup();
	}
	
	this->maxIncomingMessageSize = 4096;
	this->activelyListening = true;
	this->mainListener = new std::thread(&ekrem::ROSBridgeClient::serverListener, this);
}

void ekrem::ROSBridgeClient::destroy()
{
	std::cout << "destroy waiting for main listener\n";
	this->mainListener->join();
	std::cout << "main listener terminated\n";
	closesocket(this->sock);
	WSACleanup();
}

bool ekrem::ROSBridgeClient::sendString(std::string message)
{
	//Send a string message through tcp socket
	int sendResult = send(this->sock, message.c_str(), message.size() + 1, 0);
	if (sendResult == SOCKET_ERROR) {
		std::cerr << "Problem sending message" << std::endl;
		return false;
	}

	Sleep(SLEEP_TIME_AFTER_SEND);
	return true;

}

void ekrem::ROSBridgeClient::advertise(std::string topic, std::string type)
{
	//type has to be match with a msg type in server
	nlohmann::json advertise_msg;

	advertise_msg["op"] = "advertise";
	advertise_msg["topic"] = topic;
	advertise_msg["type"] = type;

	this->sendString(advertise_msg.dump());
}

bool ekrem::ROSBridgeClient::publish(std::string topic, nlohmann::json msg)
{
	//msg fields have to match with message type
	nlohmann::json publish_msg;

	publish_msg["op"] = "publish";
	publish_msg["topic"] = topic;
	publish_msg["msg"] = msg;

	return this->sendString(publish_msg.dump());
}

void ekrem::ROSBridgeClient::serverListener()
{
	uint8_t threadId = 0;
	while (this->activelyListening) {
		//Inializa a buffer with maximum expected message size and set all mem zero
		char* buf;
		buf = (char*)std::calloc(this->maxIncomingMessageSize, sizeof(char));
		//ZeroMemory(buf, this->maxIncomingMessageSize);
		//ERROR BEFORE HERE 
		//Recieve next incoming message from ROSBridge
		int bytesReceived = recv(this->sock, buf, this->maxIncomingMessageSize, 0);
		if (bytesReceived > 0) {
			//Parse incoming message to a json object
			std::string incomingMessage = std::string(buf, 0, bytesReceived);
			nlohmann::json incomingMsgJson = nlohmann::json::parse(incomingMessage);

			std::string incomingMessageTopicName = incomingMsgJson["topic"];
			nlohmann::json msg = incomingMsgJson["msg"];
			//Iterate over subscribed topics

			std::map< std::string, void(*)(nlohmann::json) >::iterator mapItr;
			for (mapItr = this->topicsCallbackMap.begin(); mapItr != this->topicsCallbackMap.end(); mapItr++) {
				//If topic name matches incomin
				if (incomingMessageTopicName == mapItr->first) {
					if (this->topicsCallbackMap.find(incomingMessageTopicName) == this->topicsCallbackMap.end()) {
						std::cerr << "No callback for this topic! : " << this->topicsCallbackMap.size() << "\n";
					}
					else {
						//ERROR NOT AFTER HERE
						if (this->topicsActiveThreadMap[incomingMessageTopicName] == 1) {
							//Calback of this topic is already in use
							std::cerr << "Calback of incoming topic is already in use\n";
						}
						else {
							//There is a callback for this and it is not in use
							if (threadId < this->isThreadWorking.size()) {
								this->allThreads[threadId] = new std::thread(&ekrem::ROSBridgeClient::callCallback, this, incomingMessageTopicName, msg, threadId);
								this->isThreadWorking[threadId] = true;
								this->isThreadDead[threadId] = false;
							}
							else {
								this->isThreadWorking.push_back(true);
								this->isThreadDead.push_back(false);
								this->allThreads.push_back(new std::thread(&ekrem::ROSBridgeClient::callCallback, this, incomingMessageTopicName, msg, threadId));
							}
							threadId++; // threadId is needed so this thread can mark itself completed
						}
					}
				}
			}
		}
		this->killZombies();
	}
}

void ekrem::ROSBridgeClient::callCallback(std::string topic, nlohmann::json msg, uint8_t threadId)
{
	//std::cout << "Thread #" << (int)threadId << " started\n";
	this->topicsCallbackMap.find(topic)->second(msg);
	//std::cout << "Thread #" << (int)threadId << " called callback\n";
	this->isThreadWorking[threadId] = false;
}

bool ekrem::ROSBridgeClient::subscribe(std::string topic, std::string type, int msgSize, void(*callback)(nlohmann::json))
{
	//Need approximate msg size in bytes
	if (msgSize > this->maxIncomingMessageSize) {
		this->maxIncomingMessageSize = msgSize;
	}
	nlohmann::json subscribe_msg;

	subscribe_msg["op"] = "subscribe";
	subscribe_msg["topic"] = topic;
	subscribe_msg["type"] = type;

	bool sendResult = this->sendString(subscribe_msg.dump());

	if (!sendResult) {
		std::cout << "prblem sending sub req" << std::endl;
		return false;
	}
	
	std::cout << "subscribed succesfully" << std::endl;
	this->topicsCallbackMap.insert(std::pair<std::string, void(*)(nlohmann::json)>(topic, callback));
	std::cerr << "new size of map : " << this->topicsCallbackMap.size() << "\n";

	return false;

}

void ekrem::ROSBridgeClient::killZombies()
{
	for (int i = 0; i < this->isThreadWorking.size(); i++) {
		if (!this->isThreadWorking[i] && !this->isThreadDead[i]) {
			try {
				this->allThreads[i]->join();
				this->isThreadDead[i] = true;
			}
			catch (const std::exception& e) {
				std::cerr << "\nERROR-------> " << e.what() << std::endl;
			}
		}
	}
}

