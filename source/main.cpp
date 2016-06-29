#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <malloc.h>
#include <regex>
#include <streambuf>
#include <cerrno>

#include <typeinfo>
#include <cmath>
#include <numeric>
#include <iterator>
#include <algorithm>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <3ds.h>

#include "utils.h"
#include "data.h"
#include "menu.h"
#include "json/json.h"

static const u16 top = 0x140;
static std::string region = "ALL";
static bool bExit = false;
int sourceDataType;
Json::Value sourceData;

std::string upper(std::string s)
{
  std::string ups;
  
  for(unsigned int i = 0; i < s.size(); i++)
  {
    ups.push_back(std::toupper(s[i]));
  }
  
  return ups;
}

struct display_item {
  int ld;
  int index;
};

bool compareByLD(const display_item &a, const display_item &b)
{
    return a.ld < b.ld;
}

bool FileExists (std::string name){
    struct stat buffer;
    return (stat (name.c_str(), &buffer) == 0);
}

int mkpath(std::string s,mode_t mode)
{
    size_t pre=0,pos;
    std::string dir;
    int mdret = 0;
    if(s[s.size()-1]!='/'){
        s+='/';
    }
    while((pos=s.find_first_of('/',pre))!=std::string::npos){
        dir=s.substr(0,pos++);
        pre=pos;
        if(dir.size()==0) continue; // if leading / first time is 0 length
        if((mdret=mkdir(dir.c_str(),mode)) && errno!=EEXIST){
            return mdret;
        }
    }
    return mdret;
}

char parse_hex(char c)
{
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    std::abort();
}

char* parse_string(const std::string & s)
{
    char* buffer = new char[s.size() / 2];
    for (std::size_t i = 0; i != s.size() / 2; ++i)
        buffer[i] = 16 * parse_hex(s[2 * i]) + parse_hex(s[2 * i + 1]);
    return buffer;
}

std::string GetTicket(std::string titleId, std::string encTitleKey, char* titleVersion)
{
    std::ostringstream ofs;
    ofs.write(tikTemp, 0xA50);
    ofs.seekp(top+0xA6, std::ios::beg);
    ofs.write(titleVersion, 0x2);
    ofs.seekp(top+0x9C, std::ios::beg);
    ofs.write(parse_string(titleId), 0x8);
    ofs.seekp(top+0x7F, std::ios::beg);
    ofs.write(parse_string(encTitleKey), 0x10);
    return ofs.str();
}



void removeForbiddenChar(std::string* s)
{
    std::string::iterator it;
    std::string illegalChars = "\\/:?\"<>|";
    for (it = s->begin() ; it < s->end() ; ++it){
        bool found = illegalChars.find(*it) != std::string::npos;
        if(found)
        {
            *it = ' ';
        }
    }
}

std::string ToHex(const std::string& s)
{
    std::ostringstream ret;
    for (std::string::size_type i = 0; i < s.length(); ++i)
    {
        int z = s[i]&0xff;
        ret << std::hex << std::setfill('0') << std::setw(2) << z;
    }
    return ret.str();
}


void load_JSON_data() 
{
    printf("loading horns.json...\n");
    std::ifstream ifs("/TIKdevil/horns.json");
    Json::Reader reader;
    Json::Value obj;
    reader.parse(ifs, obj);
    sourceData = obj; // array of characters
    
    if(sourceData[0]["titleID"].isString()) {
      sourceDataType = JSON_TYPE_ONLINE;
    } else if (sourceData[0]["titleid"].isString()) {
      sourceDataType = JSON_TYPE_HORNS;
    }
}


std::string get_file_contents(const char *filename)
{
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (in)
  {
    return(std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
  }
  throw(errno);
}

int util_compare_u64(const void* e1, const void* e2) {
    u64 id1 = *(u64*) e1;
    u64 id2 = *(u64*) e2;

    return id1 > id2 ? 1 : id1 < id2 ? -1 : 0;
}

std::vector<std::string> util_get_installed_tickets()
{
	std::vector<std::string> vTickets;
	Result res = 0;
    u32 ticketCount = 0;
    if(R_SUCCEEDED(res = AM_GetTicketCount(&ticketCount))) {
        u64* ticketIDs = (u64*) calloc(ticketCount, sizeof(u64));
        if(ticketIDs != NULL) {
            if(R_SUCCEEDED(res = AM_GetTicketList(&ticketCount, ticketCount, 0, ticketIDs))) {
                qsort(ticketIDs, ticketCount, sizeof(u64), util_compare_u64);
				char cur[34];
                for(u32 i = 0; i < ticketCount && R_SUCCEEDED(res); i++) {
					sprintf(cur,"%016llX", ticketIDs[i]);
					vTickets.push_back(cur);
				}
			}
		}
	}
	return vTickets;
}


void action_missing_tickets(std::vector<std::string> &vEncTitleKey, std::vector<std::string> &vTitleID, std::vector<std::string> &vTitleRegion, int &n, std::string regionFilter, bool del)
{
	// Set up the reading of json
	if (check_JSON(del == false))
	{
		load_JSON_data();
	} else
		return;
	
	if(del == false){
		printf("Checking for already installed tiks...\n\n");
	} else {
		printf("Checking for out of region tiks...\n\n");
	}
  
	n = 0;
	
	std::vector<std::string> vNANDTiks = util_get_installed_tickets();
	
	const char*  ctitleId = nullptr;
	const char*  cencTitleKey = nullptr;
	const char*  ctitleName = nullptr;
	
	std::string titleType;
	std::string titleId;
	std::string encTitleKey;
	std::string titleRegion;
	std::string curTik;
	bool isNotSystemTitle;
	int dellastPrint = 0;
	int index = sourceData.size() - 1;
	for (unsigned int i = 0; i < sourceData.size(); i++) {
		// Check that the encTitleKey isn't null
		if (sourceData[i]["encTitleKey"].isNull())
		{
			continue;
		}
		ctitleId = sourceData[i]["titleID"].asCString();
		cencTitleKey = sourceData[i]["encTitleKey"].asCString();
		ctitleName = sourceData[i]["name"].asCString();
		
		titleRegion = sourceData[i]["region"].asString();
		titleType = sourceData[i]["titleID"].asString().substr(4,4);
		
		isNotSystemTitle = (titleType == ESHOP_GAMEAPP or titleType == ESHOP_DLC or titleType == ESHOP_DSIWARE);
		
		if(del == false)
		{
			
			if(regionFilter != "ALL")
			{
				// a specific region is selected
				if(ctitleId != NULL and cencTitleKey != NULL and ctitleName != NULL and isNotSystemTitle == true and (titleRegion == regionFilter || titleRegion == "ALL" || titleRegion == ""))
				{
					// add it if it isn't a system title and the region matches
					titleId = ctitleId;
					bool found = false;
					for(unsigned int foo =0; foo < vNANDTiks.size(); foo++)
					{
						curTik = vNANDTiks.at(foo);
						std::transform(curTik.begin(), curTik.end(), curTik.begin(), ::tolower);
						if(titleId == curTik) 
						{
							found=true;				
						}
					}
					if(found==false)
					{
						n++;
						
						encTitleKey = cencTitleKey;
						encTitleKey.erase(remove_if(encTitleKey.begin(), encTitleKey.end(), isspace), encTitleKey.end());
						
						vTitleID.push_back(titleId);
						vEncTitleKey.push_back(encTitleKey);
						vTitleRegion.push_back(titleRegion);
					}
				}
			} else {
				if(ctitleId != NULL and cencTitleKey != NULL and ctitleName != NULL and isNotSystemTitle == true)
				{
					// add anything that isn't a system title ticket
					titleId = ctitleId;
					bool found = false;
					for(unsigned int foo =0; foo < vNANDTiks.size(); foo++)
					{
						curTik = vNANDTiks.at(foo);
						std::transform(curTik.begin(), curTik.end(), curTik.begin(), ::tolower);
						if(titleId == curTik) 
						{
							found=true;				
						}
					}
					if(found==false)
					{
						encTitleKey = cencTitleKey;
						encTitleKey.erase(remove_if(encTitleKey.begin(), encTitleKey.end(), isspace), encTitleKey.end());
						n++;
						vTitleID.push_back(titleId);
						vEncTitleKey.push_back(encTitleKey);
					}
				}
			}
		} else {
			
			u64 curr;
			if(regionFilter != "ALL")
			{
				// If region is not ALL
				if(ctitleId != NULL and cencTitleKey != NULL and ctitleName != NULL and isNotSystemTitle == true and ((regionFilter != "REGION FREE" && titleRegion != regionFilter && titleRegion != "ALL" && titleRegion != "") || (regionFilter == "REGION FREE" && (titleRegion != "ALL" && titleRegion != ""))))
				{
					// If region matches selection and it not a system title
					n++;
					curr = strtoull(ctitleId, NULL, 16) ;
					AM_DeleteTicket(curr);
				}
			} else {
				if(ctitleId != NULL and cencTitleKey != NULL and ctitleName != NULL and isNotSystemTitle == true)
				{
					// This isn't a system title ticket, remove it
					n++;
					curr = strtoull(ctitleId, NULL, 16) ;
					AM_DeleteTicket(curr);
				}
			}
			int delprogress = i*100/index;
			if((delprogress%10==0 and delprogress>dellastPrint)or(delprogress==0 and dellastPrint==0)){
				printf("%d%% ", delprogress);
				dellastPrint = delprogress+1;
			}
		}
	}
	
	if(del==false){
		printf("Missing tickets: %d\n\n", n);
	} else if(del==true){
		printf("100%%\n\n");
		printf("Deleted tickets: %d\n\n", n);
	}
}

void action_install(std::vector<std::string> vEncTitleKey,std::vector<std::string> vTitleID, int index)
{
	printf("Installing missing tickets...\n\n");
	char titleVersion[2] = {0x00, 0x00};
	Handle hTik;
	u32 writtenbyte;
	int instlastPrint = 0;
	for (unsigned int i =0; i < vTitleID.size(); i++)
	{
		
		AM_InstallTicketBegin(&hTik);
		std::string curr = GetTicket(vTitleID.at(i), vEncTitleKey.at(i), titleVersion);
		FSFILE_Write(hTik, &writtenbyte, 0, curr.c_str(), 0x150000, 0);
		AM_InstallTicketFinish(hTik);
		
		int instprogress = i*100/index;
		if((instprogress%10==0 and instprogress>instlastPrint)or(instprogress==0 and instlastPrint==0)){
			printf("%d%% ", instprogress);
			instlastPrint = instprogress+1;
		}
	}
	printf("100%%\n  Done!\n\n");
}


void action_about()
{
    consoleClear();
	
	printf(CONSOLE_RED "\n\n\n  TIKdevil by Kyraminol\n\n" CONSOLE_RESET);
    printf("    Generate only missing tickets\n");
    printf("    and directly install them!\n\n\n");
    printf(CONSOLE_BLUE "  Special thanks to:\n\n" CONSOLE_RESET);
	printf("    cearp, Drakia, steveice10, Mmcx125,\n    and DanTheMan827.\n" CONSOLE_RESET);
    wait_key_specific("\n\n  Press A to continue.\n", KEY_A);
}

void action_toggle_region()
{
	if(region == "REGION FREE")region = "ALL"; 
	else if(region == "ALL") region = "EUR";
	else if(region == "EUR") region = "USA";
	else if(region == "USA") region = "JPN";
	else if(region == "JPN") region = "CHN";
	else if(region == "CHN") region = "KOR";
	else if(region == "KOR") region = "TWN";
	else if(region == "TWN") region = "REGION FREE";
}

int action_getconfirm(bool removing){
	int ret = 0;

	char msg[72];
	
	if(region == "ALL"){
		sprintf(msg, "Region set to ALL are you sure?");
			const char *confirm[] = {
			"No, take me to main menu.",
			"No, I only want EUR.",
			"No, I only want USA.",
			"No, I only want JPN.",
			"No, I only want CHN.",
			"No, I only want KOR.",
			"No, I only want TWN.",
			"No, I only want region free.",
			"ARR ARR SELECT THEM ALL."
		};
		while(ret==0)
		{
			int result = menu_draw(msg, " ", 0, sizeof(confirm) / sizeof(char*), confirm);
			switch (result)
			{
				case 0:
					ret=-1; break;
				case 1:
					ret=1; region = "EUR"; break;
				case 2:
					ret=1; region = "USA"; break;
				case 3:
					ret=1; region = "JPN"; break;
				case 4:
					ret=1; region = "CHN"; break;
				case 5:
					ret=1; region = "KOR"; break;
				case 6:
					ret=1; region = "TWN"; break;
				case 7:
					ret=1; region = "REGION FREE"; break;
				case 8:
					ret=1; break;
			}
			clear_screen(GFX_BOTTOM);
		}
		consoleClear();
	} else {
		printf("Region set to %s, press A to continue,\nor any other to cancel.", region.c_str());
		u32 keys = wait_key();
		if (keys & KEY_A) {
			ret = 1;
		} else {
			ret = -1;
		}
		consoleClear();
	}

	if(removing == true && region == "ALL"){
		printf(CONSOLE_RED "\nDanger, Will Robinson!\nThis will remove ALL tickets\nincluding ones for your region!" CONSOLE_RESET "\n\nPress A to continue,\nor any other to cancel.\n\n");
      u32 keys = wait_key();
      
      if (keys & KEY_A) {
        ret = 1;
      } else {
				ret = -1;
			}
	}
	if(removing == true && region == "REGION FREE"){
		printf(CONSOLE_RED "\nDanger, Will Robinson!\nThis will remove everything except region free\ntickets, this may include ones you have installed!" CONSOLE_RESET "\n\nPress A to continue,\nor any other to cancel.\n\n");
      u32 keys = wait_key();
      
      if (keys & KEY_A) {
        ret = 1;
      } else {
				ret = -1;
			}
	}
	
	return ret;
}



void select_oneclick()
{
	consoleClear();
	int w = action_getconfirm(false);
	if(w<0)return;
	std::vector<std::string> Keys;
	std::vector<std::string> IDs;
	std::vector<std::string> Regions;
	int n;
	action_missing_tickets(Keys, IDs, Regions, n, region, false);
	action_install(Keys, IDs, n);
	wait_key_specific("\n  Press A to continue.\n", KEY_A);
}



void select_removeout(){
	consoleClear();
	int w = action_getconfirm(true);
	if(w<0)return;
	printf("Deleting selected tickets...\n");
	std::vector<std::string> Keys;
	std::vector<std::string> IDs;
	std::vector<std::string> Regions;
	int n;
	action_missing_tickets(Keys, IDs, Regions, n, region, true);
	printf("Done!");
	wait_key_specific("\nPress A to continue.\n", KEY_A);
}


bool menu_main_keypress(int selected, u32 key, void*)
{
    if (key & KEY_A)
    {
        switch (selected)
        {
            case 0:
                select_oneclick();
            break;
			case 1:
				select_removeout();
			break;
			
			
            case 2:
                action_about(); break;
            case 3:
                bExit = true; break;
        }
        return true;
    }
    else if (key & KEY_L)
    {
        action_toggle_region();
        return true;
    }

    return false;
}

void menu_main()
{
    const char *options[] = {
		"Update your Tickets!",
		"Remove out-of-region tickets",
        "About TIKdevil",
        "Exit",
    };
    char footer[37];

    while (!bExit)
    {
		sprintf(footer, "Region: [%s] (Press L to change)", region.c_str());
		menu_multkey_draw("TIKdevil by Kyraminol", footer, 0, sizeof(options) / sizeof(char*), options, NULL, menu_main_keypress);
        clear_screen(GFX_BOTTOM);
    }
}

int main(int argc, const char* argv[])
{
    u32 *soc_sharedmem, soc_sharedmem_size = 0x100000;
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    httpcInit(0);
    soc_sharedmem = (u32 *)memalign(0x1000, soc_sharedmem_size);
    socInit(soc_sharedmem, soc_sharedmem_size);
    sslcInit(0);
	amInit();
    cfguInit();
    AM_InitializeExternalTitleDatabase(false);

    init_menu(GFX_TOP);
    
    // Make sure the TIKdevil directory exists on the SD card
    mkpath("/TIKdevil/", 0777);
    
    // Load the region from system secure info
    region = GetSystemRegion();
    
    menu_main();
	
    cfguExit();
	amExit();
    gfxExit();
    httpcExit();
    socExit();
    sslcExit();
}
