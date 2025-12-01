#ifndef CHEAT_H
#define CHEAT_H

#include <string>
#include <fstream>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <cmath>
#include <cstdlib>

#include "curses.h"
#include "regs.h"
#include "paging.h"
#include "mem.h"

#define PAIR_BYELLOW_BLACK 2

class CHEAT;

std::string GetDOSBoxXPath(bool withexe=false);
const char* MSG_Get(const char*);
void DEBUG_ShowMsg(char const* format,...);
const char* MSG_Get(char const *);
bool CodePageHostToGuestUTF8(char *, const char *s);
extern uint32_t GetCr0(void);
extern bool cheating;
extern std::string sCheatDir;
extern bool isDirExists(const std::string&);
extern bool createDirectory(const std::string&);

struct DATA {
  std::string msg;
  uint8_t mode;
	std::string segname; 		// 只能是CS, DS, ES, 存入其他值會忽略此值
	uint16_t seg;						// segname為CS, DS, ES時會忽略此值, 此值為0時為線性模式
	uint32_t off;
	uint32_t add;
};

struct SNAP {
	uint32_t add;
	uint32_t value;
};

union UBUF {
	uint32_t dword;
	uint16_t word[2];
	uint8_t  byte[4];
};

class CHEAT {
private:
	static const std::string MODE[];
	static const std::string OLD[];
	static constexpr int OLDS = 9;
  // 專案名稱
	std::string mPname;
	// 專案資料夾完整路徑
	std::string mDir;
	// 專案檔案(完整路徑+檔名)
	std::string mFile;
	// SNAP模式記憶體快照檔
	std::string mSnapFile;
	// 儲存視窗handle
	WINDOW* mWin;
	// 儲存搜尋模式
	uint8_t mMode;
	// 儲存段暫存器名稱
	std::string mSegName;
	// 儲存段暫存器值
	uint16_t mSeg;
	// 儲存偏移位址
	uint32_t mOff;
	// 儲存搜尋長度
	uint32_t mLength;
	// 儲存要比對的資料
	uint8_t mGoal[4];
	// 暫存讀取進來的資料
	uint8_t mRead[4];
	// 儲存已確定的搜尋結果
	std::vector<DATA> data;
	// 儲存暫時搜尋結果
	std::vector<SNAP> snap;
	// 總記憶體數量
	uint32_t mTolMem;
	// getAddress()時若超出記憶體範圍會設定為true
	bool mOver;
	// 記憶體模式
	uint8_t mMemMode;
	// 記憶體模式暫存器名
	std::string mMemSegName;
	// 記憶體模式暫存器值
	uint16_t mMemSeg;
	// 記憶體模式偏移位址
	uint32_t mMemOff;
	
	void setSegName(std::string);
	uint16_t getSegValue(void);
	uint16_t getMmodeSegValue(void);
	uint32_t getAddress(uint32_t);
	uint32_t getMemAddress(uint32_t);
	uint32_t getMmodeAddress(uint32_t);
	uint8_t getMemValue(uint32_t);
	bool setMemValue(uint32_t, uint8_t);	
	void setColor(bool, bool);
	uint32_t cvtValue(std::string);
	uint32_t cvtHex(std::string);
	uint32_t cvtDec(std::string);
	bool doCompare(void);
	void showCounts(void);
	uint32_t computeValue(void);
	bool snapCompare(bool, UBUF, uint32_t, uint32_t);
	bool createProjectDir(void);
	
public:
  static const std::string HELP[];
	static const std::string CMDS[];
	static constexpr int HELPS = 32;
  // 是否搜尋中
	bool mSearching;
	bool mSnap;
  CHEAT(WINDOW*);
	CHEAT(WINDOW*, std::string);
	~CHEAT();
	bool isOlds(std::string);
	void showStatus(void);
	void setMode(std::string);
	void setMemMode(std::string);
	void setSeg(std::string);
	void setMemSeg(std::string);
	void setOff(std::string);
	void setMemOff(std::string);
	void setLength(std::string);
	std::string setDview(std::string);
	std::string setDVview(std::string);
	std::string setMemDview(void);
	void prtMmodeValue(void);
	bool setMmodeValue(std::string);
	void listMemory(std::string, std::string);
	void listTemp(void);
	void deleteTemp(std::string);
	void makeSearch(std::string);
	void makeOldSearch(std::string);
	void writeTemp(std::string, std::string);
	void saveTemp(std::string);
	void loadTemp(std::string);
	void addRecord(std::string);
	void addMemRecord(std::string);
	void prtRecord(std::string);
	void writeRecord(std::string, std::string);
	void saveRecord(void);
	void readRecord(void);
	void resetSearch(void);
	void snapStart(void);
	bool snapSave(void);
	void snapSearch(bool, std::string);
	void snapSearchValue(std::string);
	void snapOld(bool, std::string);
	void dspValue(std::string);
	
	static void prtHelp(void);
	static void showMsg(const std::string);
	static void showDetailMsg(const std::string);
};

#endif