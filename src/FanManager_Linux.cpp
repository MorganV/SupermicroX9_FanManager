// FanManager.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <sstream>
#include <cmath>

using namespace std;

string performCMD(string command) {
	array<char, 128> buffer;
	string result;
	unique_ptr<FILE, decltype(&pclose)> pipe(popen(&command.at(0), "r"), pclose);
	if (!pipe) {
		throw runtime_error("popen() failed!");
	}
	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
		result += buffer.data();
	}
	return result;
}

double calculateFanSpeed(double temp_CPU1, double temp_CPU2) {
	double temp = max(temp_CPU1, temp_CPU2);
	double maxTemp = 55; //temperature at which fans should ramp to full
	int pwr = 3; //effectively, how fast fans ramp up --> higher is slower
	double speedPcnt = (pow(temp, pwr)) / (pow(maxTemp, pwr));
	return speedPcnt;
}

string toHexStr(int value) { ostringstream oss; oss << hex << value; return oss.str(); }

void getTemps(double& temp1, double& temp2) {
	string cmd = performCMD("ipmiutil sensor -c | grep CPU");
	istringstream stream(cmd);

	string buf;
	getline(stream, buf, '|');
	getline(stream, buf, '|');
	getline(stream, buf, '|');
	getline(stream, buf, '|');
	getline(stream, buf, '|');
	getline(stream, buf, '|');
	stream >> temp1;
	stream >> buf;
	getline(stream, buf, '|');
	getline(stream, buf, '|');
	getline(stream, buf, '|');
	getline(stream, buf, '|');
	getline(stream, buf, '|');
	getline(stream, buf, '|');
	stream >> temp2;
}

const double FAN_DUTY_SETTINGS[]{102, 140, 180, 220, 255}; //fan speed settings, I will use five different ones
const int NUM_SPEEDS = 5;										     // 30% -> 50% -> 65% -> 80% -> 100%
const int WAIT_MAPPINGS[]{ 0, 5, 10, 10, 15};		   	//minimum fan speed cycles before recalculation / adjustment 
																	//one cycle is a few seconds
int main()
{
	performCMD("IPMICFG -fan 1"); //default to full before calculation completes
	int prevSpeed = 255;
	int cycleSkipCount = 0;
	while (1) {
		//get temps	
		double temp1, temp2;
		getTemps(temp1, temp2);
		double speed = calculateFanSpeed(temp1, temp2);
		speed *= 255; //convert from percent to hex 0x00 <-> FF speed value
		speed -= 1; //edge case of max speed calculation
		double setSpeed=0; //boundary confined, always use next highest
		int i;
		for (i = 0; speed > setSpeed && i<NUM_SPEEDS; i++) {
			setSpeed = FAN_DUTY_SETTINGS[i];
		}
		if (setSpeed > prevSpeed) {
			cycleSkipCount = WAIT_MAPPINGS[i];
			prevSpeed = setSpeed;
			//set higher speed
			performCMD("IPMICFG -raw 0x30 0x91 0x5A 0x3 0x10 0x" + toHexStr(setSpeed));
			cout << "Setting higher speed: " << setSpeed << endl;
		}
		else if (cycleSkipCount <= 0 && setSpeed<prevSpeed) {
		//set lower speed
			performCMD("IPMICFG -raw 0x30 0x91 0x5A 0x3 0x10 0x" +toHexStr(setSpeed));
			cout << "Setting lower speed: " << setSpeed << endl;
			prevSpeed = setSpeed;
		}
		cycleSkipCount--;
		cout << "Cycle skip min: " << cycleSkipCount << endl;
	}

	return 0;
}

