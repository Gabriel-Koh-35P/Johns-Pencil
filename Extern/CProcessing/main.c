#include "cprocessing.h"
#include "scale.h"


int main(void)
{
	CP_System_SetWindowSize(1600, 900);
	CP_Engine_SetNextGameState(scale_init, scale_update, scale_exit);
	CP_Engine_Run(0);
	return 0;
}
