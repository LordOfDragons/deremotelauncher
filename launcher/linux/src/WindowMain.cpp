/**
 * MIT License
 * 
 * Copyright (c) 2022 DragonDreams (info@dragondreams.ch)
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

#include "WindowMain.h"
#include "Application.h"
#include "resources/icons.h"


FXDEFMAP(WindowMain) WindowMainMap[] = {
	FXMAPFUNC(SEL_CLOSE, 0, WindowMain::onClose),
	FXMAPFUNC(SEL_MINIMIZE, 0, WindowMain::onMinimized),
	FXMAPFUNC(SEL_RESTORE, 0, WindowMain::onRestored),
	FXMAPFUNC(SEL_MAXIMIZE, 0, WindowMain::onMaximized)
};

FXIMPLEMENT(WindowMain, FXMainWindow, WindowMainMap, ARRAYNUMBER(WindowMainMap))


// Constructors
/////////////////

WindowMain::WindowMain(){
}

WindowMain::WindowMain(Application &app) :
FXMainWindow(&app, "Drag[en]gine Remote Launcher",
	new FXBMPIcon(&app, icon_appicon), new FXBMPIcon(&app, icon_appicon),
	DECOR_ALL, 0, 0, 800, 600)
{
	create();
	show(FX::PLACEMENT_SCREEN);
}

// Management
///////////////

void WindowMain::OnFrameUpdate(){
}

bool WindowMain::CloseWindow(){
	return true;
}

void WindowMain::Close(){
	// we use close() on purpose instead of delete because fox requires this
	close(false);
}

// Events
///////////

long WindowMain::onClose(FXObject*, FXSelector, void*){
	if(CloseWindow()){
		Close();
	}
	return 1;
}

long WindowMain::onMinimized(FXObject*, FXSelector, void*){
	return 0;
}

long WindowMain::onRestored(FXObject*, FXSelector, void*){
	return 0;
}

long WindowMain::onMaximized(FXObject*, FXSelector, void*){
	return 0;
}
