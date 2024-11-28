#include <derlLauncherClient.h>
#include <stdlib.h>
#include <iostream>
#include <thread>
#include <chrono>

class Logger : public denLogger{
private:
	std::mutex pMutex;
	
public:
	Logger() = default;
	void Log(LogSeverity severity, const std::string &message) override{
		const std::lock_guard guard(pMutex);
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
		
		//const std::time_t t = std::time(nullptr);
		//std::cout << "[" << std::put_time(std::localtime(&t), "%F %T") << "] ";
		const auto tp = std::chrono::system_clock::now();
		const auto t = std::chrono::system_clock::to_time_t(tp);
		std::cout << "[" << std::put_time(std::localtime(&t), "%F %T");
		auto dur = tp.time_since_epoch();
		auto ss = std::chrono::duration_cast<std::chrono::milliseconds>(dur) % std::chrono::seconds{1};
		std::cout << "." << std::setfill('0') << std::setw(3) << ss.count() << "] ";
		
		std::cout << message << std::endl;
	}
};

class Client : public derlLauncherClient{
public:
	enum class State{
		connecting,
		connected
	};
	
	std::atomic<bool> exit = false;
	std::atomic<State> state = State::connecting;
	
	Client() = default;
	int Run(int argc, char *argv[]){
		#ifdef ENABLE_CLIENT_DEBUG
		SetEnableDebugLog(true);
		#endif
		
		SetLogger(std::make_shared<Logger>());
		SetName("Test Client");
		SetPathDataDir(std::filesystem::path(argv[1]));
		
		ConnectTo(argv[2]);
		
		std::chrono::steady_clock::time_point last(std::chrono::steady_clock::now());
		while(!exit){
			std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
			const int64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - last).count();
			
			// if(elapsed_us < 10){
			// 	continue;
			// }
			
			last = now;
			Update((float)elapsed_us / 1e6f);
			
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
		Log(denLogger::LogSeverity::info, "OnConnectionEstablished", "Run");
		state = State::connected;
	}
	
	void OnConnectionFailed(denConnection::ConnectionFailedReason reason) override{
		std::stringstream ss;
		ss << ": reason=" << (int)reason;
		Log(denLogger::LogSeverity::info, "OnConnectionFailed", ss.str());
		exit = 1;
	}
	
	void OnConnectionClosed() override{
		Log(denLogger::LogSeverity::info, "OnConnectionClosed", "Run");
		exit = 1;
	}
	
	void StartApplication(const derlRunParameters &params) override{
		Log(denLogger::LogSeverity::info, "StartApplication", "Run");
	}
	
	void StopApplication() override{
		Log(denLogger::LogSeverity::info, "StopApplication", "Run");
	}
	
	void KillApplication() override{
		Log(denLogger::LogSeverity::info, "KillApplication", "Run");
	}
};

int main(int argc, char *argv[]){
	std::cout << "DERemoteLauncher Test: Client" << std::endl;
	return Client().Run(argc, argv);
}
