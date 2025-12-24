#include <cross.h>

#include "cheat.h"

bool cheating = false;
const std::string CHEAT::MODE[] = {"", "BYTE", "WORD", "", "DWORD"};
const std::string CHEAT::OLD[] = {
	"MEMDUMPBIN", "VMEMDUMPBIN", "EV", "SM", "SMV", 
	"FM", "RUN", "D", "DV"
};
const std::string CHEAT::CMDS[] = {
	"HELP", "SM",   "SEG",  "OFF",  "SL",
	"D",    "DV",   "S",    "L",    "TL",
  "TD",   "TW",   "TS",   "TR",   "A",
  "R",    "WR",   "CLS",  "SAVE", "MM",
  "MSEG", "MOFF", "MD",   "MR",   "MW",
	"MA",   "SNAP", "SU",   "SD",   "SS",
	"CVT",  "EXIT"
};
const std::string CHEAT::HELP[] = { 
  "CHEAT_HELP_HELP", "CHEAT_HELP_SM",   "CHEAT_HELP_SEG",  "CHEAT_HELP_OFF",  "CHEAT_HELP_SL",
	"CHEAT_HELP_D",    "CHEAT_HELP_DV",   "CHEAT_HELP_S",    "CHEAT_HELP_L",    "CHEAT_HELP_TL", 
	"CHEAT_HELP_TD",   "CHEAT_HELP_TW",	  "CHEAT_HELP_TS",   "CHEAT_HELP_TR",   "CHEAT_HELP_A",
	"CHEAT_HELP_R",    "CHEAT_HELP_WR",   "CHEAT_HELP_CLS",  "CHEAT_HELP_SAVE", "CHEAT_HELP_MM",
	"CHEAT_HELP_MSEG", "CHEAT_HELP_MOFF", "CHEAT_HELP_MD",   "CHEAT_HELP_MR",  	"CHEAT_HELP_MW",
	"CHEAT_HELP_MA",   "CHEAT_HELP_SNAP", "CHEAT_HELP_SU",   "CHEAT_HELP_SD",  	"CHEAT_HELP_SS",
	"CHEAT_HELP_CVT",  "CHEAT_HELP_EXIT"
};

CHEAT::CHEAT(WINDOW* p1){
  mPname = "";
	mWin = p1;
	mMode = 2;
	mSegName = "DS";
	mSeg = 0;
	mOff = 0;
	mLength = 65536;
	for (int i=0; i<4; i++){
	  mGoal[i] = 0;
	  mRead[i] = 0;
	}
	snap.reserve(1000);
	mSearching = false;
	mSnap = false;
	
	if ((GetCr0() & 0x00000001) != 0) // protected mode
		mTolMem = (uint32_t)MemSize;
	else if (MEM_A20_Enabled()) // Hi memory
		mTolMem = 0x10FFF0;
	else // 640KB
		mTolMem = 655360;
		
	mOver = false;
	
	mMemMode = 2;
	mMemSegName = "DS";
	mMemSeg = 0;
	mMemOff = 0;
}

CHEAT::CHEAT(WINDOW* p1, std::string p2) : CHEAT(p1){
	/*mPname = p1;
	mMode = 2;
	mGoal = nullptr;
	mRead = nullptr;
	snap.reserve(1000);*/
	char cSlash = sCheatDir.back();
	mDir = sCheatDir + p2 + cSlash;
	mFile = mDir + p2 + ".prj";
	mSnapFile = mDir + "SNAP.BIN";
	readRecord();
}

CHEAT::~CHEAT(){
	if (snap.size() != 0){
		snap.clear();
	}
}

bool CHEAT::isOlds(std::string p1) {
	for (int i=0; i<OLDS; i++){
		if (OLD[i] == p1)
			return true;
	}
	return false;
}

void CHEAT::setSegName(std::string p1) {
	if (mSearching | mSnap)
		return;
	
	if (p1 == "CS" || p1 == "DS" || p1 == "ES")
		mSegName = p1;
	else
		mSegName = "";
}

uint16_t CHEAT::getSegValue(void) {
	if (mSegName == "CS")
		return SegValue(cs);
	else if (mSegName == "DS")
		return SegValue(ds);
	else if (mSegName == "ES")
		return SegValue(es);
	
	return mSeg;
}

uint16_t CHEAT::getMmodeSegValue(void) {
	if (mMemSegName == "CS")
		return SegValue(cs);
	else if (mMemSegName == "DS")
		return SegValue(ds);
	else if (mMemSegName == "ES")
		return SegValue(es);
	
	return mMemSeg;
}

uint32_t CHEAT::getAddress(uint32_t x) {
	uint32_t tmp = getSegValue();
	
	tmp = (tmp << 4) + mOff + x;
	
	if (tmp < mTolMem) {
		mOver = false;
		return tmp;
	} else {
		mOver = true;
		return (mTolMem-1);
	}
}

uint32_t CHEAT::getMmodeAddress(uint32_t x) {
	uint32_t tmp = getMmodeSegValue();
	
	tmp = (tmp << 4) + mMemOff + x;
	
	if (tmp < mTolMem) {
		mOver = false;
		return tmp;
	} else {
		mOver = true;
		return (mTolMem-1);
	}
}

uint8_t CHEAT::getMemValue(uint32_t off) {
	uint8_t tmp;
	uint32_t mem;
	
	mem = getAddress(off);
	if (mOver) {
		tmp = 0xFF;
	} else {
		if (mem_readb_checked((PhysPt)mem, &tmp)) tmp = (uint8_t)'X';
	}
	
	return tmp;
}

void CHEAT::prtMmodeValue(void) {
	uint8_t tmp;
	uint32_t mem;
	uint32_t val = 0;
	
	mem = getMmodeAddress(0);
	for (int i=0; i<mMemMode; i++) {
		mem_readb_checked((PhysPt)mem+i, &tmp);
		val = val + (uint32_t)std::pow(256, i) * tmp;
	}

	DEBUG_ShowMsg("Memory value : %u\n", val);
}

bool CHEAT::setMemValue(uint32_t off, uint8_t val) {
	uint32_t mem = getAddress(off);
	if (mOver) 
		return false;
	else
	  return mem_writeb_checked((PhysPt)mem, val);
}

bool CHEAT::setMmodeValue(std::string p1) {
	uint32_t val = cvtValue(p1);
	uint32_t mem = getMmodeAddress(0);
	if (mOver) 
		return false;
	
	bool flag = true;
	uint8_t rem;
	for (int i=0; i<mMemMode; i++) {
		rem = val % 256;
		flag = mem_writeb_checked((PhysPt)mem+i, rem);
		val = val / 256;
	}
	
	return flag;
}

void CHEAT::setColor(bool p1, bool p2=false) {
	if (p1) {
		if (p2){
		  if (has_colors()){
				wattrset(mWin, COLOR_PAIR(PAIR_BYELLOW_BLACK)|A_BOLD);
			}
		}
		else {
			if (has_colors()){
				wattrset(mWin, COLOR_PAIR(PAIR_BYELLOW_BLACK));
			}
		}
	} else {
		if (has_colors()){ 
		  wattrset(mWin, 0);
		}
	}
}

uint32_t CHEAT::cvtValue(std::string p1) {
	if (p1.substr(0, 2) == "0X")
		return cvtHex(p1.substr(2));
	else
		return cvtDec(p1);
}

void CHEAT::dspValue(std::string p1) {
	if (p1.substr(0, 2) == "0X") {
	  DEBUG_ShowMsg("%lu\n", cvtValue(p1));
	  return;
	}
	
	uint32_t tmp = cvtValue(p1);
	if (tmp >= 0 && tmp <= 255)
		DEBUG_ShowMsg("0x%02X\n", tmp);
	else if (tmp >= 256 && tmp <= 65535)
		DEBUG_ShowMsg("0x%04X\n", tmp);
	else
		DEBUG_ShowMsg("0x%08X\n", tmp);
}

uint32_t CHEAT::cvtHex(std::string p1) {
	std::string base("0123456789ABCDEF");
	uint32_t result = 0;

	for (char c : p1) {
		c = std::toupper(c);
		size_t pos = base.find(c);
		if (pos != std::string::npos)
			result = (result << 4) | static_cast<uint32_t>(pos);
		else
			return 0;
	}
	
	return result;
}

uint32_t CHEAT::cvtDec(std::string p1) {
	std::string base("0123456789");
	uint32_t result = 0;

	for (char c : p1) {
		size_t pos = base.find(c);
		if (pos != std::string::npos)
			result = (result * 10) + (uint32_t)pos;
		else
			return 0;
	}
	
	return result;
}

#define b0  0
#define b1 28
#define b2 53
#define t0 16
void CHEAT::showStatus(void) {
	mvwaddstr(mWin, 0, b0, "Mode          : ");
	mvwaddstr(mWin, 1, b0, "Seg Name      : ");
	mvwaddstr(mWin, 2, b0, "Segment Value : ");
	mvwaddstr(mWin, 3, b0, "Offset        : ");
	mvwaddstr(mWin, 4, b0, "Length(bytes) : ");
	mvwaddstr(mWin, 0, b1, "In Searching  : ");
	mvwaddstr(mWin, 1, b1, "Snap Mode     : ");
	mvwaddstr(mWin, 2, b1, "Matches       : ");
	mvwaddstr(mWin, 0, b2, "Mem Mode      : ");
	mvwaddstr(mWin, 1, b2, "Mem Seg Name  : ");
	mvwaddstr(mWin, 2, b2, "Mem Seg Value : ");
	mvwaddstr(mWin, 3, b2, "Mem Offset    : ");
	mvwaddstr(mWin, 4, b2, "Memory Bound  : ");
	
	setColor(true);
	
	mvwprintw(mWin, 0, b0+t0, "%s", "     ");
	mvwprintw(mWin, 0, b0+t0, "%s", MODE[mMode].c_str());
	mvwprintw(mWin, 1, b0+t0, "%s", "  ");
	mvwprintw(mWin, 1, b0+t0, "%s", mSegName.c_str());
	mvwprintw(mWin, 2, b0+t0, "0x%04X", getSegValue());
	mvwprintw(mWin, 3, b0+t0, "          ");
	mvwprintw(mWin, 3, b0+t0, "0x%08X", mOff);
	mvwprintw(mWin, 4, b0+t0, "        ");
	mvwprintw(mWin, 4, b0+t0, "%u", mLength);
	mvwprintw(mWin, 0, b1+t0, "%s", "   ");
	mvwprintw(mWin, 0, b1+t0, "%s", (mSearching) ? "Yes" : "No");
	mvwprintw(mWin, 1, b1+t0, "%s", "   ");
	mvwprintw(mWin, 1, b1+t0, "%s", (mSnap) ? "Yes" : "No");
	mvwprintw(mWin, 2, b1+t0, "%s", "      ");
	mvwprintw(mWin, 2, b1+t0, "%u", snap.size());
	mvwprintw(mWin, 0, b2+t0, "%s", "     ");
	mvwprintw(mWin, 0, b2+t0, "%s", MODE[mMemMode].c_str());
	mvwprintw(mWin, 1, b2+t0, "%s", "  ");
	mvwprintw(mWin, 1, b2+t0, "%s", mMemSegName.c_str());
	mvwprintw(mWin, 2, b2+t0, "0x%04X", getMmodeSegValue());
	mvwprintw(mWin, 3, b2+t0, "          ");
	mvwprintw(mWin, 3, b2+t0, "0x%08X", mMemOff);
	mvwprintw(mWin, 4, b2+t0, "        ");
	mvwprintw(mWin, 4, b2+t0, "%u", mTolMem);
	
	setColor(false);
	
	wrefresh(mWin);
}

void CHEAT::showCounts(void) {
	setColor(true, true);
	mvwprintw(mWin, 0, b1+t0, "%s", "   ");
	mvwprintw(mWin, 0, b1+t0, "%s", (mSearching) ? "Yes" : "No");
	mvwprintw(mWin, 1, b1+t0, "%s", "   ");
	mvwprintw(mWin, 1, b1+t0, "%s", (mSnap) ? "Yes" : "No");
	mvwprintw(mWin, 2, b1+t0, "%s", "      ");
	mvwprintw(mWin, 2, b1+t0, "%u", snap.size());
	setColor(false);
	wrefresh(mWin);
}

void CHEAT::setMode(std::string p1) {
	if (mSearching | mSnap)
		return;
	
  uint8_t old = mMode;
	
	if (p1 == "BYTE" || p1 == "1")
		mMode = 1;
	else if (p1 == "DWORD" || p1 == "4")
		mMode = 4;
	else
		mMode = 2;
	
	if (old != mMode)
		showStatus();
}

void CHEAT::setMemMode(std::string p1) {
  uint8_t old = mMemMode;
	
	if (p1 == "BYTE" || p1 == "1")
		mMemMode = 1;
	else if (p1 == "DWORD" || p1 == "4")
		mMemMode = 4;
	else
		mMemMode = 2;
	
	if (old != mMemMode)
		showStatus();
}

void CHEAT::setSeg(std::string p1) {
	if (mSearching | mSnap)
		return;
	
	if (p1 == "CS" | p1 == "DS" | p1 == "ES") {
		mSegName = p1;
		mLength = 65536;
	} else {
		mSegName = "";
		mSeg = (cvtValue(p1) % (UINT16_MAX+1));
		if (mSeg == 0)
			mLength = mTolMem;
		else
			mLength = 65536;
	}
	
	setOff(std::to_string(mOff));
	setLength(std::to_string(mLength));
	
	showStatus();
}

void CHEAT::setMemSeg(std::string p1) {
	if (p1 == "CS" | p1 == "DS" | p1 == "ES")
		mMemSegName = p1;
	else {
		mMemSegName = "";
		mMemSeg = (cvtValue(p1) % (UINT16_MAX+1));
	}
	
	setMemOff(std::to_string(mMemOff));
	
	showStatus();
}

void CHEAT::setOff(std::string p1) {
	if (mSearching | mSnap)
		return;
	
	mOff = cvtValue(p1);
	uint16_t seg = getSegValue();
	if (seg != 0)
		mOff = (mOff % (UINT16_MAX+1));
	showStatus();
}

void CHEAT::setMemOff(std::string p1) {
	mMemOff = cvtValue(p1);
	uint16_t seg = getMmodeSegValue();
	if (seg != 0)
		mMemOff = (mMemOff % (UINT16_MAX+1));
	showStatus();
}

void CHEAT::setLength(std::string p1) {
	if (mSearching)
		return;
	
	mLength = cvtValue(p1);
	if (mLength == 0)
		mLength = 65536;
	else {
	  uint16_t seg = getSegValue();
	  if (seg != 0) {
		  mLength = (mLength % (UINT16_MAX+1));
	  }
		if (mLength == 0)
			mLength = 65536;
	}
	
	showStatus();
}

std::string CHEAT::setDview(std::string p1) {
	std::string result;
	uint16_t seg;
	char buf[20];
	
	if (mSegName == "") {
		seg = getSegValue();
		if (seg == 0) {
			return std::string("");
		} else {
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "%X", seg);
			result = std::string(buf) + ":";
		}
	} else {
	  result = mSegName + ":";
	}
	
	uint32_t val = cvtValue(p1);
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%X", mOff+val);
	result = result + std::string(buf);
	
	return result;
}

std::string CHEAT::setDVview(std::string p1) {
	std::string result;
	uint32_t val = cvtValue(p1);
	char buf[20];
	sprintf(buf, "%X", mOff+val);
	result = std::string(buf);
	
	return result;
}

std::string CHEAT::setMemDview(void) {
	std::string result;
	uint16_t seg;
	char buf[20];
	
	if (mMemSegName == ""){
		seg = getMmodeSegValue();
		if (seg == 0){
		  result = "DV ";
	  } else {
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "%X", seg);
	    result = "D  " + std::string(buf) + ":";
		}
	} else {
		result = "D  " + mMemSegName + ":";
	}

  memset(buf, 0, sizeof(buf));
	sprintf(buf, "%X", mMemOff);
	result = result + std::string(buf);
	
	return result;
}

void CHEAT::listMemory(std::string p1, std::string p2) {
	uint32_t val = cvtValue(p1);
	uint32_t result = 0, temp;
	uint8_t mode;
	
	if (p2.empty())
		mode = mMode;
	else {
		if (p2 == "BYTE" || p2 == "1")
			mode = 1;
		else if (p2 == "DWORD" || p2 == "4")
			mode = 4;
		else
			mode = 2;
	}
	
	for (int i=0; i<mode; i++){
		temp = getMemValue(val+i);
		if (mOver)
			return;
		result = result + (uint32_t)(temp * std::pow(256, i));
	}
	
	val = val + mOff;
	if (val <= 65535)
	  DEBUG_ShowMsg("Memory %04X value is %u\n", val, result);
	else
		DEBUG_ShowMsg("Memory %08X value is %u\n", val, result);
}

void CHEAT::listTemp(void) {
	DEBUG_ShowMsg("====================================================================\n");
	for (int i=0; i<10; i++){
		if (i < snap.size())
			DEBUG_ShowMsg("%u : %u <==> 0x%X, Value = %u\n", i+1, snap[i].add, snap[i].add, snap[i].value);
	}
}

void CHEAT::deleteTemp(std::string p1) {
	if (p1 == "")
		return;
	
	if (snap.size() < 1)
		return;
	
	int rec = cvtValue(p1) - 1;
	
	if (rec < 0 || rec > (snap.size()-1))
		return;
	
	snap.erase(snap.begin()+rec);
	listTemp();
	showCounts();
}

void CHEAT::writeTemp(std::string p1, std::string p2) {
	uint8_t rec;
	
	if (p2.empty())
		rec = 0;
	else {
		rec = cvtValue(p2) - 1;
		if (rec > (snap.size()-1)){
			DEBUG_ShowMsg("Wrong tempory number.\n");
			return;
		}
	}
	
	uint32_t val = cvtValue(p1);
	uint32_t off = snap[rec].add;
	uint8_t rem;
	for (int i=0; i<mMode; i++){
		rem = val % 256;
		setMemValue(off+i, rem);
		val = val / 256;
	}
	
	DEBUG_ShowMsg("Write successed.\n");
}

void CHEAT::saveTemp(std::string p1) {
	if (!mSearching && !mSnap && snap.size() == 0){
		DEBUG_ShowMsg("No temporary data.\n");
		return;
	}
	
	if (!createProjectDir()) {
		DEBUG_ShowMsg("Can't create project diretory!\n");
		return;
	}
	
	if (p1.size() == 0){
		DEBUG_ShowMsg("Must provide temporary name.\n");
		return;
	}
	
	std::string name = mDir + p1 + ".tmp";
	std::ofstream file(name);
	
	if (!file.is_open()) {
		DEBUG_ShowMsg("Can't open temporary file!\n");
		return;
	}
	
	std::string tmp;
	if (mSearching)
		tmp = "S";
	if (mSnap)
		tmp = "N";
	file << tmp << "`";
	file << (int)mMode << "`";
	file << mSegName << "`";
	file << mSeg << "`";
	file << mOff << std::endl;
	
	for (SNAP it : snap ) {
		file << it.add << "`" << it.value << std::endl;
	}
	
	file.close();
}

static void formatError(void) {
	DEBUG_ShowMsg("Temporary file with wrong format data, can't go on.\n");
	return;
}

void CHEAT::loadTemp(std::string p1) {
	if (mSearching) {
		DEBUG_ShowMsg("In search mode, can't load temporary file.\n");
		return;
	}
	
	if (mSnap) {
		DEBUG_ShowMsg("In snap mode, can't load temporary file.\n");
		return;
	}
	
	if (!createProjectDir()) {
		DEBUG_ShowMsg("Can't create project diretory!\n");
		return;
	}
	
	if (p1.size() == 0){
		DEBUG_ShowMsg("Must provide temporary name.\n");
		return;
	}
	
	std::string name = mDir + p1 + ".tmp";
	std::ifstream file(name);
	
	std::string line;
	std::getline(file , line);
  if (file.eof() || file.fail()) {
		DEBUG_ShowMsg("No data to load.\n");
		file.close();
		return;
	}
	std::istringstream ss(line);
	std::string token;
	std::vector<std::string> fields;
		
	while (std::getline(ss, token, '`')) {
		fields.push_back(token);
	}
	
	if (fields.size() != 5) {
		DEBUG_ShowMsg("First line!\n");
		formatError();
		file.close();
		return;
	};
	
	std::string tmp = fields[0];
	uint8_t mode = (uint8_t)std::stoi(fields[1]);
	if (mode != 1 && mode != 2 && mode != 4){
		DEBUG_ShowMsg("Mode %d error!", mode);
		formatError();
		file.close();
		return;
	} else {
		mMode = mode;
	}
	setSeg(fields[2]);
	if (mSegName == "")
	  setSeg(fields[3]);
	setOff(fields[4]);
	
	while (std::getline(file, line)) {
		std::istringstream ss(line);
	  std::string token;
	  std::vector<std::string> fields;
		
	  while (std::getline(ss, token, '`')) {
		  fields.push_back(token);
	  }
		
		if (fields.size() != 2) {
			DEBUG_ShowMsg("After first line!\n");
			formatError();
			file.close();
			return;
		}
		
		SNAP it;
		it.add = std::stoul(fields[0]);
		it.value = std::stoul(fields[1]);
		snap.push_back(it);
	}
	
	file.close();
	if (tmp == "S")
	  mSearching = true;
	if (tmp == "N")
	  mSnap = true;
	showCounts();
}

void CHEAT::addRecord(std::string p1) {
	DATA temp;
	
	if (snap.size() != 1) {
		DEBUG_ShowMsg("Can't add record!\n");
		return;
	}
	
	p1.erase(std::remove(p1.begin(), p1.end(), '`'), p1.end());
	temp.msg = p1;
	temp.mode = mMode;
	temp.segname = mSegName;
	temp.seg = mSeg;
	temp.off = mOff;
	temp.add = snap[0].add;
	data.push_back(temp);
	
	DEBUG_ShowMsg("Add memory %X to record.\n", temp.add);
	
	return;
}

void CHEAT::addMemRecord(std::string p1) {
	DATA temp;
	
	p1.erase(std::remove(p1.begin(), p1.end(), '`'), p1.end());
	temp.msg = p1;
	temp.mode = mMemMode;
	temp.segname = mMemSegName;
	temp.seg = mMemSeg;
	temp.off = mMemOff;
	temp.add = 0;
	data.push_back(temp);
	
	DEBUG_ShowMsg("Add memory %X to record.\n", temp.off);
	
	return;
}

void CHEAT::prtRecord(std::string p1) {
	if (data.size() == 0)
		return;
	
	if (p1 == "") {
	  for (int i=0; i<data.size(); i++) {
		  DEBUG_ShowMsg("==================================================\n");
		  DEBUG_ShowMsg("Record Number : %u\tName : %s\n", i+1, data[i].msg);
	  }
		return;
	}
	
	uint32_t rec = cvtValue(p1);
	if (rec < 1 || rec > data.size())
		return;
	rec--;
	DEBUG_ShowMsg("==================================================\n");
	DEBUG_ShowMsg("Record Number : %u\n", rec+1);
	DEBUG_ShowMsg("Name          : %s\n", data[rec].msg);
	DEBUG_ShowMsg("Mode          : %s\n", MODE[data[rec].mode].c_str());
	DEBUG_ShowMsg("Segment name  : %s\n", data[rec].segname);
	uint16_t seg;
	if (data[rec].segname == "CS")
		seg = SegValue(cs);
	else if (data[rec].segname == "DS")
		seg = SegValue(ds);
	else if (data[rec].segname == "ES")
		seg = SegValue(es);
	else
		seg = data[rec].seg;
	DEBUG_ShowMsg("Segment value : 0x%04X\n", seg);
	uint32_t off = data[rec].off + data[rec].add;
	DEBUG_ShowMsg("Offset        : %u <==> 0x%X\n", off, off);
}

void CHEAT::writeRecord(std::string p1, std::string p2) {
	uint32_t val;
	uint32_t rec;
	uint32_t mem;
	uint8_t rem;
	
	if (p1.size() == 0)
		return;
	else
		val = cvtValue(p1);
	
	if (p2.size() == 0)
		return;
	else {
		rec = cvtValue(p2);
		if (rec < 1 || rec > data.size())
			return;
		rec--;
	}
	
	if (data[rec].segname == "CS")
		mem = SegValue(cs);
	else if (data[rec].segname == "DS")
		mem = SegValue(ds);
	else if (data[rec].segname == "ES")
		mem = SegValue(es);
	else
		mem = data[rec].seg;
	
	mem = (mem << 4) + data[rec].off + data[rec].add;
	for (int i=0; i<data[rec].mode; i++){
		rem = val % 256;
		mem_writeb_checked((PhysPt)mem+i, rem);
		val = val / 256;
	}
	
	DEBUG_ShowMsg("Write value %u to record number %u.\n", cvtValue(p1), rec+1);
}

void CHEAT::saveRecord(void) {
  if (!createProjectDir()) {
		DEBUG_ShowMsg("Can't create project diretory!\n");
		return;
	};

	std::ofstream out(mFile);
	
	if (!out.is_open())
		return;
	
	for (int i=0; i<data.size(); i++){
		out << data[i].msg     << "`";
		out << (int)data[i].mode    << "`";
		out << data[i].segname << "`";
		out << data[i].seg     << "`";
		out << data[i].off     << "`";
		out << data[i].add     << std::endl;
	}
	
	out.close();
	
	DEBUG_ShowMsg("Save data to file %s completed.\n", mFile.c_str());
}

void CHEAT::readRecord(void) {
	std::ifstream in(mFile);
	
	if (!in.is_open())
		return;
	
	std::string line;
	int cnt = 0;
	
	while (std::getline(in, line)) {
		std::istringstream ss(line);
		std::string token;
		std::vector<std::string> fields;
		
		while (std::getline(ss, token, '`')) {
			fields.push_back(token);
		}
		
		DATA temp;
	  if (fields.size() == 6) {
			char buf[120];
			if (!CodePageHostToGuestUTF8(buf, fields[0].c_str())){
				temp.msg = fields[0].c_str();
			} else {
				temp.msg = buf;
			}
		  //temp.msg = buf;
		  temp.mode = std::stoi(fields[1]);
		  temp.segname = fields[2];
		  temp.seg = (uint16_t)std::stoul(fields[3]);
		  temp.off = std::stoul(fields[4]);
		  temp.add = std::stoul(fields[5]);
		  data.push_back(temp);
			cnt++;
	  }
	}
	
	DEBUG_ShowMsg("Read %u record(s).\n", cnt);
}

void CHEAT::resetSearch(void) {
	mSearching = false;
	mSnap = false;
	snap.clear();
	showStatus();
	DEBUG_ShowMsg("Temporary data cleared, you can start new search.\n");
}

void CHEAT::snapStart(void) {
	if (mSearching || mSnap)
		return;
	
	if (!createProjectDir()) {
		DEBUG_ShowMsg("Can't create project diretory!\n");
		return;
	}
	
	if (snapSave()){
	  mSnap = true;
	  showCounts();
	  DEBUG_ShowMsg("Save the memory block to file:SNAP.BIN\n");
	}
}

bool CHEAT::snapSave(void) {
	std::ofstream file(mSnapFile, std::ios::binary);
	
	if (!file.is_open()) {
		DEBUG_ShowMsg("Can't open Snap file.\n");
		return false;
	}
	
	uint16_t seg = getSegValue();
	if (seg == 0) {
	  for (uint32_t x=0; x<mTolMem; x++) {
		  uint8_t val;
		  if (mem_readb_checked((PhysPt)(x), &val)) val = 0;
		  file.write(reinterpret_cast<char*>(&val), sizeof(val));
	  }
	} else {
		for (uint32_t x=0; x<65536; x++) {
			uint8_t val;
			uint32_t mem = (seg << 4) + x;
			if (mem_readb_checked((PhysPt)(mem), &val)) val = 0;
			file.write(reinterpret_cast<char*>(&val), sizeof(val));
		}
	}
	
	file.close();
	return true;
}
	
void CHEAT::snapSearch(bool down, std::string p1) {
	if (!mSnap)
		return;
	
	if (snap.size() > 0) {
		snapOld(down, p1);
		return;
	}
	
	std::ifstream file(mSnapFile, std::ios::binary);
	if (!file.is_open()) {
		DEBUG_ShowMsg("Can't open Snap file.\n");
		return;
	}
	
	uint32_t delta = cvtValue(p1);
	uint32_t length;
	UBUF temp;
	temp.dword = 0;
	uint16_t seg = getSegValue();
	if (seg == 0)
		length = mTolMem;
	else
		length = 65536;
	
	file.seekg(0, std::ios::end);
	uint32_t filesize = (uint32_t)file.tellg();
	file.seekg(0, std::ios::beg);
	
	if (filesize < length){
		DEBUG_ShowMsg("Snap file size to small!\n");
		return;
	}
	
	int cnt = 0;
	DEBUG_ShowMsg("====================================================================\n");
	file.read(reinterpret_cast<char*>(&temp), mMode);
	for (uint32_t i=0; i<=(length-mMode); i++){
		uint32_t mval = 0;
		for (int j=0; j<mMode; j++){
			uint8_t tmp = getMemValue(i+j);
			mval = mval + ((uint32_t)std::pow(256, j) * tmp);
		}
		if (snapCompare(down, temp, delta, mval)){
			SNAP it;
			it.add = i;
			it.value = mval;
			snap.push_back(it);
			if (cnt < 10){
				DEBUG_ShowMsg("%u : %u <==> 0x%X, snap:%u <==> memory:%u\n", snap.size(), i, i, temp.dword, mval);
		  }
			cnt++;
		}
		uint8_t bt;
		file.read(reinterpret_cast<char*>(&bt), 1);
		for (int t=0; t<(mMode-1); t++){
			temp.byte[t] = temp.byte[t+1];
		}
		temp.byte[mMode-1] = bt;
	}
	
	showCounts();
	
	file.close();
	
	//snapSave();
}

void CHEAT::snapSearchValue(std::string p1) {
	if (!mSnap)
		return;
	
	if (p1 == "")
		return;
	
	uint32_t val = cvtValue(p1);
	std::vector<bool> mark(snap.size(), false);
	int cnt = 0;
	DEBUG_ShowMsg("====================================================================\n");
	for (int i=0; i<snap.size(); i++) {
		if (snap[i].value == val) {
			mark[i] = true;
			if (cnt < 10)
				DEBUG_ShowMsg("%u : %u <==> 0x%X, value = %u\n", cnt+1, snap[i].add, snap[i].add, val);
			cnt++;
		}
	}
	
	for (int i=int(snap.size()-1); i>=0; i--){
	  if (!mark[i])
		  snap.erase(snap.begin()+i);
	}
	
	showCounts();
}

void CHEAT::snapOld(bool down, std::string p1) {
	uint32_t delta = cvtValue(p1);
	std::vector<bool> mark(snap.size(), false);
	
	UBUF temp;
	int cnt = 0;
	DEBUG_ShowMsg("====================================================================\n");
	for (int i=0; i<snap.size(); i++) {
		uint32_t mval = 0;
		for (int j=0; j<mMode; j++){
			uint8_t tmp = getMemValue(snap[i].add+j);
			mval = mval + ((uint32_t)std::pow(256, j) * tmp);
		}
		temp.dword = 0;
		if (mMode == 1){
			temp.byte[0] = (uint8_t)snap[i].value;
		} else if (mMode == 2) {
			temp.word[0] = (uint16_t)snap[i].value;
		} else {
			temp.dword = snap[i].value;
		}
		if (snapCompare(down, temp, delta, mval)) {
			mark[i] = true;
			snap[i].value = mval;
			if (cnt < 10)
				DEBUG_ShowMsg("%u : %u <==> 0x%X, old:%u <==> memory:%u\n", cnt+1, snap[i].add, snap[i].add, temp.dword, mval);
			cnt++;
		}
	}
	
	for (int i=int(snap.size()-1); i>=0; i--){
	  if (!mark[i])
		  snap.erase(snap.begin()+i);
	}
	
	showCounts();
}

bool CHEAT::snapCompare(bool down, UBUF p1, uint32_t p2, uint32_t p3) {
	bool flag = false;
	uint32_t result = 0;
	
	if (p2 == 0) {
		if (mMode == 1) {
			result = p1.byte[0];
		} else if (mMode == 2) {
			result = p1.word[0];
		} else {
			result = p1.dword;
		}
		if (down) {
			if (p3 < result)
				flag = true;
		} else {
			if (p3 > result)
				flag = true;
		}
	} else {
		if (mMode == 1) {
			if (down) {
			  result = p1.byte[0] - p2;
			} else {
				result = p1.byte[0] + p2;
			}
		} else if (mMode == 2) {
			if (down) {
			  result = p1.word[0] - p2;
			} else {
				result = p1.word[0] + p2;
			}
		} else {
			if (down) {
			  result = p1.dword - p2;
			} else {
				result = p1.dword + p2;
			}	
		}
		if (p3 == result)
			flag = true;
	}
	
	return flag;
}

void CHEAT::makeSearch(std::string p1) {
	if (mSearching | mSnap)
		return;
	
	uint32_t val, tmp;
  val = tmp	= cvtValue(p1);
	uint32_t rem;
	
	DEBUG_ShowMsg("Searching...\n");
	
	for (int i=0; i<4; i++){
		mGoal[i] = 0;
		mRead[i] = 0;
	}
	
	for (int i=0; i<mMode; i++){
		rem = tmp % 256;
		mGoal[i] = rem;
		tmp = tmp / 256;
	}
	
  bool first = true;
	int cnt = 0;
	
	DEBUG_ShowMsg("====================================================================\n");
	for (uint32_t i=0; i<mLength; i++){
		if (first) {
			for (int j=0; j<mMode; j++){
				mRead[j] = getMemValue(j);
			}
			i = mMode - 1;
			first = false;
		} else {
			for (int j=0; j<mMode-1; j++){
				mRead[j] = mRead[j+1];
			}
			mRead[mMode-1] = getMemValue(i);
		}
		if (mOver)
			break;
		if (doCompare()) {
			int target = mOff + i - (mMode-1);
			SNAP it;
			it.add = target;
			it.value = val;
			snap.push_back(it);
			if (cnt < 10) {
			  DEBUG_ShowMsg("%u : %u <==> 0x%X, Value = %u\n", snap.size(), target, target, val);
			}
			showCounts();
			cnt++;
		}
	}
	
	if (snap.size() > 0) {
		mSearching = true;
		showCounts();
	}
	
	DEBUG_ShowMsg("Completed.\n");
}

void CHEAT::makeOldSearch(std::string p1) {
	if (!mSearching)
		return;
	
	std::vector<bool> mark(snap.size(), false);
	uint32_t val, tmp;
	val = tmp = cvtValue(p1);
	uint32_t rem;
	
	DEBUG_ShowMsg("Searching...\n");
	for (int i=0; i<4; i++){
		mGoal[i] = 0;
		mRead[i] = 0;
	}
	
	for (int i=0; i<mMode; i++){
		rem = tmp % 256;
		mGoal[i] = rem;
		tmp = tmp / 256;
	}
	
	int cnt = 0;
	DEBUG_ShowMsg("====================================================================\n");
	for (int i=0; i<snap.size(); i++){
		for (int j=0; j<mMode; j++){
			mRead[j] = getMemValue(snap[i].add+j-mOff);
		}
		if (doCompare()) {
			mark[i] = true;
			snap[i].value = val;
			if (cnt < 10){
				DEBUG_ShowMsg("%u : %u <==> 0x%X, Value = %u\n", cnt+1, snap[i].add, snap[i].add, val);
			}
			cnt++;
		}
	}
	
	for (int i=int(snap.size()-1); i>=0; i--){
		if (!mark[i])
			snap.erase(snap.begin()+i);
	}
	
	showCounts();
	DEBUG_ShowMsg("Stop.\n");
}

bool CHEAT::doCompare(void) {
	bool flag = true;
	
	for (int i=0; i<mMode; i++){
		if (mGoal[i] != mRead[i]){
			flag = false;
			break;
		}
	}
	
	return flag;
}

void CHEAT::prtHelp(void) {
	for (int i=0; i<HELPS; i++) {
		showMsg(HELP[i].c_str());
	}
	
  return;
}

void CHEAT::showMsg(const std::string str) {
	std::string msg = std::string(MSG_Get(str.c_str()));
	if ((msg == str) || (msg.compare(0, 5, "Error") == 0))
		msg = "message not found!\n";
	else
		msg = msg + "\n";
	DEBUG_ShowMsg(msg.c_str());
	return;
}

void CHEAT::showDetailMsg(const std::string str) {
	std::string tmp = str;
	std::string result;
	std::string msg;
	bool flag = true;
	int cnt = 0;
	
	DEBUG_ShowMsg("\n");
	while (flag) {
		if (cnt == 0) {
			result = tmp;
		} else {
			result = tmp + std::to_string(cnt);
		}	
		msg = std::string(MSG_Get(result.c_str()));
		if ((msg == result) || (msg.compare(0, 5, "Error") == 0)){
			flag = false;
			break;
		} else {
			DEBUG_ShowMsg(msg.c_str());
		}
		
    cnt++;	
		if (cnt >= 25)
			break;
	}
	DEBUG_ShowMsg("\n");
	
	return;
}

bool CHEAT::createProjectDir(void) {
	if (!isDirExists(mDir)){
		return createDirectory(mDir);
	}
	
	return true;
}