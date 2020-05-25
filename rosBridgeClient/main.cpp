#include "rosBridgeClient.h"

void callback(nlohmann::json msg);
void callback2(nlohmann::json msg);

int main() {
	//IN ROS side do:
	//Start ros by roscore
	//Then open a tcp rosbridge by "rosrun rosbridge_server rosbridge_tcp"

	ekrem::ROSBridgeClient client("192.168.1.7", 9090);

	client.subscribe("/topic_name_to_sub1", "std_msgs/String", 4096, callback2);
	client.subscribe("/topic_name_to_sub2", "std_msgs/Float32", 4096, callback);

	//I will make an cleaner msg class later to support messages outside of std_msgs
	nlohmann::json msgString = { {"data", "hello ROS"} }; // msg to be published
	nlohmann::json msgFloat32 = { {"data", 0.123} }; // msg to be published

	//Need to advertise first in order to publish later
	//But I will get rid of this logic later and simply just publishing will be enough
	client.advertise("topic_name_string", "std_msgs/String");
	client.advertise("topic_name_float", "std_msgs/Float32");

	for (int i = 0; i < 10; i++) {
		client.publish("topic_name_string", msgString);
		client.publish("topic_name_float", msgFloat32);
		Sleep(1000);
	}

	client.destroy();
	return 0;
}

void callback(nlohmann::json msg) {
	float data = msg["data"];
	std::cout << "\n2>>>>>>>>>>>>>>>>>>>>>>>>>> " << data << std::endl;
}

void callback2(nlohmann::json msg) {
	std::string data = msg["data"];
	std::cout << "\n1>>>>>>>>>>>>>>>>>>>>>>>>>> " << data << std::endl;
}