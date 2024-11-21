#include <derlServer.h>
#include <derlGlobal.h>
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

class Client : public derlRemoteClient{
public:
	enum class State{
		connecting,
		connected,
		synchronize,
		delay,
		disconnecting
	};
	
	State state;
	std::chrono::steady_clock::time_point timerBegin;
	
	Client(derlServer &server, const derlRemoteClientConnection::Ref &connection) :
	derlRemoteClient(server, connection),
	state(State::connecting)
	{
		//SetEnableDebugLog(true);
		SetLogger(std::make_shared<Logger>());
	}
	
	void Update(float elapsed) override{
		derlRemoteClient::Update(elapsed);
		
		std::unique_lock guard(GetMutex());
		switch(state){
		case State::connected:
			if(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - timerBegin).count() >= 1000){
				std::cout << "[" << GetName() << "] Timeout => synchronize" << std::endl;
				state = State::synchronize;
				guard.unlock();
				Synchronize();
			}
			break;
			
		case State::delay:
			if(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - timerBegin).count() >= 1000){
				std::cout << "[" << GetName() << "] Timeout => disconnect" << std::endl;
				state = State::disconnecting;
				guard.unlock();
				Disconnect();
			}
			break;
		}
	}
	
	void OnConnectionEstablished() override{
		std::cout << "[" << GetName() << "] OnConnectionEstablished" << std::endl;
		timerBegin = std::chrono::steady_clock::now();
		state = State::connected;
	}
	
	void OnConnectionClosed() override{
		std::cout << "[" << GetName() << "] OnConnectionClosed" << std::endl;
	}
	
	void OnSynchronizeBegin() override{
		std::cout << "[" << GetName() << "] OnSynchronizeBegin" << std::endl;
	}
	
	void OnSynchronizeUpdate() override{
		/*
		std::cout << "[" << GetName() << "] OnSynchronizeUpdate["
			<< (int)GetSynchronizeStatus() << "]: " << GetSynchronizeDetails() << std::endl;
		*/
	}
	
	void OnSynchronizeFinished() override{
		const std::lock_guard guard(GetMutex());
		std::cout << "[" << GetName() << "] OnSynchronizeFinished["
			<< (int)GetSynchronizeStatus() << "]: " << GetSynchronizeDetails() << std::endl;
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
		
		ListenOn("localhost");
		
		std::unique_lock guard(GetMutex());
		std::chrono::steady_clock::time_point last(std::chrono::steady_clock::now());
		while(!exit){
			std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
			const int64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - last).count();
			
			if(elapsed_us < 10){
				continue;;
			}
			
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
