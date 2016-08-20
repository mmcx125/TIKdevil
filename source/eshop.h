#ifdef _3DS
    #include <3ds.h>
#endif
#include <string>

// eShop Title IDs
#define JPN_ESHOP_TITLEID 0x0004001000020900
#define USA_ESHOP_TITLEID 0x0004001000021900
#define EUR_ESHOP_TITLEID 0x0004001000022900
#define KOR_ESHOP_TITLEID 0x0004001000027900
#define TWN_ESHOP_TITLEID 0x0004001000028900

// Title ID type definitions
#define ESHOP_GAMEAPP "0000"
#define ESHOP_DLC "008c"
#define ESHOP_UPDATE "000e"
#define ESHOP_DSIWARE "8004"

// Functions
bool launch_eshop();
#ifdef _3DS
    std::string eshop_title_type(std::string);
#endif
