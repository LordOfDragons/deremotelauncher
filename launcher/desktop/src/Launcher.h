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

#ifndef _LAUNCHER_H_
#define _LAUNCHER_H_

#include <atomic>
#include <thread>
#include <filesystem>

#include <delauncher/delLauncher.h>
#include <delauncher/game/delGame.h>
#include <denetwork/denLogger.h>
#include <dragengine/common/file/decBaseFileReader.h>

class WindowMain;
class derlRunParameters;


/**
 * Launcher.
 */
class Launcher : protected delLauncher{
public:
	/** \brief Reference. */
	typedef std::shared_ptr<Launcher> Ref;
	
	/** \brief Launcher state. */
	enum class State{
		/** \brief Launcher is preparing. */
		preparing,
		
		/** \brief Launcher is ready to run. */
		ready,
		
		/** \brief Game is running. */
		running,
		
		/** \brief Preparing launcher failed. */
		prepareFailed
	};
	
	
private:
	class LauncherLogger : public deLogger{
	private:
		WindowMain &pWindowMain;
		denLogger &pLogger;
		
	public:
		LauncherLogger(WindowMain &windowMain, denLogger &logger);
		
		void LogInfo(const char *source, const char *message) override;
		void LogWarn(const char *source, const char *message) override;
		void LogError( const char *source, const char *message ) override;
	};
	
	
	WindowMain &pWindowMain;
	const denLogger::Ref pLogger;
	deLogger::Ref pLauncherLogger;
	
	std::atomic<State> pState = State::preparing;
	std::shared_ptr<std::thread> pThreadPrepareLauncher;
	
	delGame::Ref pGame;
	decBaseFileReader::Ref pReaderGameLogs;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create client. */
	Launcher(WindowMain &windowMain, const denLogger::Ref &logger);
	
	/** \brief Clean up client. */
	~Launcher() override;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Launcher state. */
	State GetState();
	
	/** \brief Run game. */
	void RunGame(const std::filesystem::path &dataPath, const derlRunParameters &runParams);
	
	/** \brief Stop game. */
	void StopGame();
	
	/** \brief Kill game. */
	void KillGame();
	
	/** \brief Pulse check game state. */
	void Pulse();
	
	/** \brief Read game logs sending them to server. */
	void ReadGameLogs();
	/*@}*/
};

#endif
