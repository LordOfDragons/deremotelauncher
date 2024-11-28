#include <derlServer.h>
#include <derlGlobal.h>
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

class Client : public derlRemoteClient{
public:
	enum class State{
		connecting,
		connected,
		synchronize,
		delay,
		disconnecting
	};
	
	std::atomic<State> state;
	std::atomic<std::chrono::steady_clock::time_point> timerBegin;
	std::atomic<std::chrono::steady_clock::time_point> syncStartTime;
	
	Client(derlServer &server, const derlRemoteClientConnection::Ref &connection) :
	derlRemoteClient(server, connection),
	state(State::connecting)
	{
		#ifdef ENABLE_CLIENT_DEBUG
		SetEnableDebugLog(true);
		#endif
		
		SetLogger(std::make_shared<Logger>());
	}
	
	void Update(float elapsed) override{
		derlRemoteClient::Update(elapsed);
		
		switch(state){
		case State::connected:
			if(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - timerBegin.load()).count() >= 1000){
				Log(denLogger::LogSeverity::info, "Update", "Timeout => synchronize");
				state = State::synchronize;
				Synchronize();
			}
			break;
			
		case State::delay:
			if(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - timerBegin.load()).count() >= 1000){
				Log(denLogger::LogSeverity::info, "Update", "Timeout => disconnect");
				state = State::disconnecting;
				Disconnect();
			}
			break;
		}
	}
	
	void OnConnectionEstablished() override{
		Log(denLogger::LogSeverity::info, "OnConnectionEstablished", "Run");
		timerBegin = std::chrono::steady_clock::now();
		state = State::connected;
	}
	
	void OnConnectionClosed() override{
		Log(denLogger::LogSeverity::info, "OnConnectionClosed", "Run");
	}
	
	void OnSynchronizeBegin() override{
		Log(denLogger::LogSeverity::info, "OnSynchronizeBegin", "Run");
		syncStartTime = std::chrono::steady_clock::now();
	}
	
	void OnSynchronizeUpdate() override{
		/*
		std::cout << "[" << GetName() << "] OnSynchronizeUpdate["
			<< (int)GetSynchronizeStatus() << "]: " << GetSynchronizeDetails() << std::endl;
		*/
	}
	
	void OnSynchronizeFinished() override{
		const int64_t syncElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - syncStartTime.load()).count();
		std::stringstream ss;
		ss << (int)GetSynchronizeStatus() << ": " << GetSynchronizeDetails()
			<< " elapsed " << syncElapsed << "ms";
		Log(denLogger::LogSeverity::info, "OnSynchronizeFinished", ss.str());
		timerBegin = std::chrono::steady_clock::now();
		state = State::delay;
	}
};

class Server : public derlServer{
public:
	bool exit = false;
	
	Server() = default;
	int Run(int argc, char *argv[]){
		SetLogger(std::make_shared<Logger>());
		SetPathDataDir(std::filesystem::path(argv[1]));
		
		ListenOn(argv[2]);
		
		std::unique_lock guard(GetMutex());
		std::chrono::steady_clock::time_point last(std::chrono::steady_clock::now());
		while(!exit){
			std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
			const int64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - last).count();
			
			// if(elapsed_us < 10){
			// 	continue;
			// }
			
			last = now;
			const std::size_t clientCount = GetClients().size();
			
			guard.unlock();
			Update((float)elapsed_us / 1e6f);
			guard.lock();
			
			if(clientCount > 0 && GetClients().empty()){
				exit = true;
			}
		}
		guard.unlock();
		
		StopListening();
		WaitAllClientsDisconnected();
		
		return 0;
	}
	
	derlRemoteClient::Ref CreateClient(const derlRemoteClientConnection::Ref &connection) override{
		std::cout << "CreateClient: " << connection->GetRemoteAddress() << std::endl;
		return std::make_shared<Client>(*this, connection);
	}
};

int main(int argc, char *argv[]){
	std::cout << "DERemoteLauncher Test: Server" << std::endl;
	return Server().Run(argc, argv);
}
