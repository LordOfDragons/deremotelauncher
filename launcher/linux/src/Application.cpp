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

#include "Application.h"
#include "WindowMain.h"


FXIMPLEMENT(Application, FXApp, nullptr, 0)

// Constructors
/////////////////

Application::Application(){
}

Application::Application(int argCount, char **args) :
FXApp("DERemoteLauncher", "DragonDreams"),
pToolTip(nullptr),
pWindowMain(nullptr)
{
	init(argCount, args);
	pToolTip = new FXToolTip(this, TOOLTIP_PERMANENT);
	create();
	
	pWindowMain = new WindowMain(*this);
}


// Management
///////////////

inline WindowMain &Application::GetWindowMain() const{
	if(!pWindowMain){
		throw FXErrorException("pWindowMain == nullptr");
	}
	return *pWindowMain;
}

int Application::Run(){
	try{
		return run();
		
	}catch(const FXException &e){
		FXMessageBox::error(this, FX::MBOX_OK, "Application Error", "%s", e.what());
		return 1;
	}
	
	return 0;
}

void Application::Quit(){
	if( pToolTip ){
		delete pToolTip;
		pToolTip = nullptr;
	}
	
	// fox deletes the window if closed. same goes for the application
	exit(0);
}

// FXMessageBox::error( FXApp::instance(), FX::MBOX_OK, "Application Error", "%s", foxMessage.GetString() );
