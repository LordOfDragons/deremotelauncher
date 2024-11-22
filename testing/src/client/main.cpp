#include <derlLauncherClient.h>
#include <stdlib.h>
#include <iostream>
#include <thread>
#include <chrono>

class Logger : public denLogger{
public:
	Logger() = default;
	void Log(LogSeverity severity, const std::string &message) override{
		switch(severity){
		case LogSeverity::debug:
			std::cout << "[DD] ";
			break;
			
		case LogSeverity::warning:
			std::cout << "[WW] ";
			break;
			
		case LogSeverity::error:
			std::cout << "[EE] ";
			break;
			
		case LogSeverity::info:
		default:
			std::cout << "[II] ";
		}
		
		const std::time_t t = std::time(nullptr);
		std::cout << "[" << std::put_time(std::localtime(&t), "%F %T") << "] ";
		
		std::cout << message << std::endl;
	}
};

class Client : public derlLauncherClient{
public:
	enum class State{
		connecting,
		connected
	};
	
	bool exit = false;
	State state = State::connecting;
	
	Client() = default;
	int Run(int argc, char *argv[]){
		// SetEnableDebugLog(true);
		SetLogger(std::make_shared<Logger>());
		SetName("Test Client");
		SetPathDataDir(std::filesystem::path(argv[1]));
		
		ConnectTo("localhost");
		
		std::unique_lock guard(GetMutex());
		std::chrono::steady_clock::time_point last(std::chrono::steady_clock::now());
		while(!exit){
			std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
			const int64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - last).count();
			
			// if(elapsed_us < 10){
			// 	continue;
			// }
			
			last = now;
			guard.unlock();
			Update((float)elapsed_us / 1e6f);
			guard.lock();
			
			switch(state){
			case State::connected:
				break;
				
			default:
				break;
			}
		}
		return 0;
	}
	
	void OnConnectionEstablished() override{
		const std::lock_guard guard(GetMutex());
		std::cout << "OnConnectionEstablished" << std::endl;
		state = State::connected;
	}
	
	void OnConnectionFailed(denConnection::ConnectionFailedReason reason) override{
		const std::lock_guard guard(GetMutex());
		std::cout << "OnConnectionFailed: reason=" << (int)reason << std::endl;
		exit = 1;
	}
	
	void OnConnectionClosed() override{
		const std::lock_guard guard(GetMutex());
		std::cout << "OnConnectionClosed" << std::endl;
		exit = 1;
	}
};

int main(int argc, char *argv[]){
	std::cout << "DERemoteLauncher Test: Client" << std::endl;
	return Client().Run(argc, argv);
}
