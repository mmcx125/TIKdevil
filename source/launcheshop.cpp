#include "launcheshop.h"

void launch_eshop(){
	u8 region = -1;
	CFGU_SecureInfoGetRegion(&region);

	// Prepare for the app launch
	if(region == CFG_REGION_JPN)
		APT_PrepareToDoApplicationJump(0, JPN_ESHOP_TITLEID, 0);

	if(region == CFG_REGION_USA)
		APT_PrepareToDoApplicationJump(0, USA_ESHOP_TITLEID, 0);

	if(region == CFG_REGION_EUR)
		APT_PrepareToDoApplicationJump(0, EUR_ESHOP_TITLEID, 0);

	if(region == CFG_REGION_KOR)
		APT_PrepareToDoApplicationJump(0, KOR_ESHOP_TITLEID, 0);

	if(region == CFG_REGION_TWN)
		APT_PrepareToDoApplicationJump(0, TWN_ESHOP_TITLEID, 0);

	if(
		region == CFG_REGION_JPN ||
		region == CFG_REGION_USA ||
		region == CFG_REGION_EUR ||
		region == CFG_REGION_KOR ||
		region == CFG_REGION_TWN
	){
		// We need these 2 buffers for APT_DoAppJump()
		u8 param[0x300];
		u8 hmac[0x20];

		// Tell APT to trigger the app launch and set the status of this app to exit
		APT_DoApplicationJump(param, sizeof(param), hmac);
	}
}
