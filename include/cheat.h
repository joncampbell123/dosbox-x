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
	std::string segname; 		// Only CS, DS, and ES are valid. Any other value stored will be ignored.
	uint16_t seg;						// This value is ignored when segname is CS, DS, or ES. When this value is 0, it indicates linear memory mode.
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
  // Project Name
	std::string mPname;
	// Full Path of Project Folder
	std::string mDir;
	// Project File (Full Path + Filename)
	std::string mFile;
	// SNAP Mode Memory Snapshot File
	std::string mSnapFile;
	// window handle
	WINDOW* mWin;
	// Search Mode
	uint8_t mMode;
	// Segment register name
	std::string mSegName;
	// Segment Value
	uint16_t mSeg;
	// Offset Value
	uint32_t mOff;
	// Search Length
	uint32_t mLength;
	// Data to be compared
	uint8_t mGoal[4];
	// Temporarily store the loaded data
	uint8_t mRead[4];
	// Save the confirmed search results.
	std::vector<DATA> data;
	// Save temporary search results.
	std::vector<SNAP> snap;
	// Total number of memory blocks.
	uint32_t mTolMem;
	// Set to true when getAddress() exceeds the memory range.
	bool mOver;
	// Unit of memory operation mode
	uint8_t mMemMode;
	// Segment register name for memory operation mode
	std::string mMemSegName;
	// Segment register value for memory operation mode
	uint16_t mMemSeg;
	// Offset address for memory operation mode
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