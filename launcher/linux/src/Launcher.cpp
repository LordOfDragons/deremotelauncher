/**
 * MIT License
 * 
 * Copyright (c) 2024 DragonDreams (info@dragondreams.ch)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Launcher.h"
#include "WindowMain.h"

#include <delauncher/game/delGameXML.h>
#include <delauncher/game/delGameRunParams.h>
#include <dragengine/common/file/decMemoryFile.h>
#include <dragengine/common/file/decMemoryFileReader.h>


// Class LauncherLogger
/////////////////////////

Launcher::LauncherLogger::LauncherLogger(WindowMain &windowMain, denLogger &logger) :
pWindowMain(windowMain),
pLogger(logger){
}

void Launcher::LauncherLogger::LogInfo(const char *source, const char *message){
	std::stringstream ss;
	ss << "[" << source << "] " << message;
	pLogger.Log(denLogger::LogSeverity::info, ss.str());
	
	const Client::Ref client(pWindowMain.GetClient());
	if(client){
		client->SendLog(denLogger::LogSeverity::info, source, message);
	}
}

void Launcher::LauncherLogger::LogWarn(const char *source, const char *message){
	std::stringstream ss;
	ss << "[" << source << "] " << message;
	pLogger.Log(denLogger::LogSeverity::warning, ss.str());
	
	const Client::Ref client(pWindowMain.GetClient());
	if(client){
		client->SendLog(denLogger::LogSeverity::warning, source, message);
	}
}

void Launcher::LauncherLogger::LogError(const char *source, const char *message){
	std::stringstream ss;
	ss << "[" << source << "] " << message;
	pLogger.Log(denLogger::LogSeverity::error, ss.str());
	
	const Client::Ref client(pWindowMain.GetClient());
	if(client){
		client->SendLog(denLogger::LogSeverity::error, source, message);
	}
}


// Constructors, destructors
//////////////////////////////

Launcher::Launcher(WindowMain &windowMain, const denLogger::Ref &logger) :
pWindowMain(windowMain),
pLogger(logger),
pLauncherLogger(deLogger::Ref::New(new LauncherLogger(windowMain, *pLogger)))
{
	#ifdef OS_W32
	delEngineInstanceThreaded::SetDefaultExecutableName("deremotelauncher-engine");
	#endif
	
	logger->Log(denLogger::LogSeverity::info, "Preparing launcher...");
	
	GetLogger()->AddLogger(pLauncherLogger);
	
	pThreadPrepareLauncher = std::make_shared<std::thread>([&](){
		try{
			Prepare();
			pLogger->Log(denLogger::LogSeverity::info, "Launcher ready");
			pState = State::ready;
			
		}catch(const deException &e){
			const decStringList lines(e.FormatOutput());
			const int count = lines.GetCount();
			std::stringstream ss;
			int i;
			
			ss << "Prepaing launcher failed: ";
			
			for(i=0; i<count; i++){
				if(i > 0){
					ss << std::endl;
				}
				ss << lines.GetAt(i).GetString();
			}
			
			logger->Log(denLogger::LogSeverity::error, ss.str());
			pState = State::prepareFailed;
		}
		
		pWindowMain.RequestUpdateUIStates();
	});
}

Launcher::~Launcher(){
	if(pThreadPrepareLauncher){
		pThreadPrepareLauncher->join();
		pThreadPrepareLauncher.reset();
	}
}


// Management
///////////////

Launcher::State Launcher::GetState(){
	const State state = pState;
	if(state != State::preparing && pThreadPrepareLauncher){
		pThreadPrepareLauncher->join();
		pThreadPrepareLauncher.reset();
	}
	
	return state;
}

void Launcher::RunGame(const std::filesystem::path &dataPath, const derlRunParameters &runParams){
	switch(pState){
	case State::preparing:
		DETHROW_INFO(deeInvalidAction, "Launcher not fully prepared yet");
		
	case State::prepareFailed:
		DETHROW_INFO(deeInvalidAction, "Launcher prepare failed");
		
	case State::ready:
		break;
		
	case State::running:
		DETHROW_INFO(deeInvalidAction, "Game is already running");
		
	default:
		return;
	}
	
	const delGame::Ref game(delGame::Ref::New(CreateGame()));
	
	{
	const decMemoryFile::Ref fileGameConfig(decMemoryFile::Ref::New(new decMemoryFile("game.degame")));
	fileGameConfig->Resize(runParams.GetGameConfig().size());
	runParams.GetGameConfig().copy(fileGameConfig->GetPointer(), runParams.GetGameConfig().size());
	
	delGameXML gameXml(pLauncherLogger, "DERemoteLauncher");
	gameXml.ReadFromFile(decMemoryFileReader::Ref::New(
		new decMemoryFileReader(fileGameConfig)), game);
	}
	
	game->SetGameDirectory(dataPath.string().c_str());
	game->SetDefaultLogFile();
	
	if(game->GetPathConfig().IsEmpty()){
		DETHROW_INFO(deeInvalidFileFormat, "No configuration path specified, ignoring game file.");
	}
	if(game->GetPathCapture().IsEmpty()){
		DETHROW_INFO(deeInvalidFileFormat, "No capture path specified, ignoring game file.");
	}
	
	// load configuration if the game is not installed. this allows to keep the parameter
	// changes alive done by the player inside the game
	if(!GetGameManager().GetGames().Has(game)){
		game->LoadConfig();
	}
	
	game->VerifyRequirements();
	
	if(game->GetCanRun()){
		delGameProfile *profile = game->GetProfileToUse();
		
		if(!runParams.GetProfileName().empty()){
			profile = GetGameManager().GetProfiles().GetNamed(runParams.GetProfileName().c_str());
			if(!profile){
				std::stringstream ss;
				ss << "No profile found named '" << runParams.GetProfileName() << "'";
				DETHROW_INFO(deeInvalidParam, ss.str().c_str());
			}
		}
		
		if(profile->GetValid()){
			delGameRunParams launchRunParams;
			launchRunParams.SetGameProfile(profile);
			
			decString error;
			if(!launchRunParams.FindPatches(*game, game->GetUseLatestPatch(), game->GetUseCustomPatch(), error)){
				DETHROW_INFO(deeInvalidParam, error);
			}
			
			std::stringstream arguments;
			
			if(!profile->GetReplaceRunArguments()){
				arguments << game->GetRunArguments().GetString() << " ";
			}
			
			arguments << profile->GetRunArguments().GetString();
			
			if(!runParams.GetArguments().empty()){
				arguments << runParams.GetArguments();
			}
			
			launchRunParams.SetRunArguments(arguments.str().c_str());
			
			launchRunParams.SetFullScreen(profile->GetFullScreen());
			launchRunParams.SetWidth(profile->GetWidth());
			launchRunParams.SetHeight(profile->GetHeight());
			
			if(game->GetWindowSize() != decPoint()){
				launchRunParams.SetWidth(game->GetWindowSize().x);
				launchRunParams.SetHeight(game->GetWindowSize().y);
				launchRunParams.SetFullScreen(false);
			}
			
			game->StartGame(launchRunParams);
			pGame = game;
			pState = State::running;
		}
		
	}else if(!game->GetAllFormatsSupported()){
		DETHROW_INFO(deeInvalidParam, "One or more File Formats required by the game are not working.");
		
	}else{
		DETHROW_INFO(deeInvalidParam, "Game related properties are incorrect.");
	}
}

void Launcher::StopGame(){
	switch(pState){
	case State::preparing:
		DETHROW_INFO(deeInvalidAction, "Launcher not fully prepared yet");
		
	case State::prepareFailed:
		DETHROW_INFO(deeInvalidAction, "Launcher prepare failed");
		
	case State::ready:
		break;
		
	default:
		return;
	}
	
	if(!pGame){
		DETHROW_INFO(deeInvalidAction, "Game missing");
	}
	
	pGame->StopGame();
	
	pGame = nullptr;
	pState = State::ready;
}

void Launcher::KillGame(){
	switch(pState){
	case State::preparing:
		DETHROW_INFO(deeInvalidAction, "Launcher not fully prepared yet");
		
	case State::prepareFailed:
		DETHROW_INFO(deeInvalidAction, "Launcher prepare failed");
		
	case State::ready:
		break;
		
	default:
		return;
	}
	
	if(!pGame){
		DETHROW_INFO(deeInvalidAction, "Game missing");
	}
	
	pGame->KillGame();
	
	pGame = nullptr;
	pState = State::ready;
}

void Launcher::Pulse(){
	switch(pState){
	case State::running:
		if(pGame){
			pGame->PulseChecking();
			if(pGame->IsRunning()){
				break;
			}
		}
		
		pLauncherLogger->LogInfo("Launcher", "Application stopped running.");
		
		pGame = nullptr;
		pState = State::ready;
		break;
		
	default:
		break;
	}
}
