//---------------------------------------------------------
// file:	main.c
// author:	Gabriel Koh Quan Kai
// email:	gabrielquankai.koh@digipen.edu
//
// brief:	Main entry point for the sample project
//			of the CProcessing library
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#include "cprocessing.h"
#include "splashscreen.h"

static int isFullscreen = 0;

// Full screen toggle hotkey (F)
void HandleGlobalHotkeys(void)
{
    if (CP_Input_KeyTriggered(KEY_F))
    {
        isFullscreen = !isFullscreen;
        if (isFullscreen)
        {
            CP_System_Fullscreen();
        }
        else
        {
            /* Restore your default windowed resolution (e.g., 1600x900 or 1280x720) */
            CP_System_SetWindowSize(1600, 900);
        }
    }
}

// main() the starting point for the program
// CP_Engine_SetNextGameState() tells CProcessing which functions to use for init, update and exit
// CP_Engine_Run() is the core function that starts the simulation
int main(void)
{
    CP_Engine_SetNextGameState(Splash_Init, Splash_Update, Splash_Exit);
	CP_System_SetWindowSize(1600, 900);
    CP_Engine_SetPreUpdateFunction(HandleGlobalHotkeys);
    CP_Engine_Run(0);
	return 0;
}
