#include "DebugVariable.h"

std::vector<CDebugVar*> CDebugVar::varList;

CDebugVar::CDebugVar(char* _name, PhysPt _adr) {
    adr = _adr;
    safe_strncpy(name, _name, 16);
    hasvalue = false; value = 0;
};

char* CDebugVar::GetName(void) {
    return name;
}

PhysPt CDebugVar::GetAdr(void) {
    return adr;
}

void CDebugVar::SetValue(bool has, uint16_t val) {
    hasvalue = has; value = val;
}

uint16_t CDebugVar::GetValue(void) {
    return value;
}

bool CDebugVar::HasValue(void) {
    return hasvalue;
}


void CDebugVar::InsertVariable(char* name, PhysPt adr)
{
	varList.push_back(new CDebugVar(name,adr));
}

void CDebugVar::DeleteAll(void)
{
	std::vector<CDebugVar*>::iterator i;
	for(i=varList.begin(); i != varList.end(); ++i) {
		CDebugVar* bp = *i;
		delete bp;
	}
	(varList.clear)();
}

CDebugVar* CDebugVar::FindVar(PhysPt pt)
{
	if (varList.empty()) return 0;

	std::vector<CDebugVar*>::size_type s = varList.size();
	for(std::vector<CDebugVar*>::size_type i = 0; i != s; i++) {
		CDebugVar* bp = varList[i];
		if (bp->GetAdr() == pt) return bp;
	}
	return 0;
}

bool CDebugVar::SaveVars(char* name) {
	if (varList.size() > 65535) return false;

	FILE* f = fopen(name,"wb+");
	if (!f) return false;

	// write number of vars
	uint16_t num = (uint16_t)varList.size();
	fwrite(&num,1,sizeof(num),f);

	std::vector<CDebugVar*>::iterator i;
	CDebugVar* bp;
	for(i=varList.begin(); i != varList.end(); ++i) {
		bp = *i;
		// name
		fwrite(bp->GetName(),1,16,f);
		// adr
		PhysPt adr = bp->GetAdr();
		fwrite(&adr,1,sizeof(adr),f);
	}
	fclose(f);
	return true;
}

bool CDebugVar::LoadVars(char* name)
{
	FILE* f = fopen(name,"rb");
	if (!f) return false;

	// read number of vars
	uint16_t num;
	if (fread(&num,sizeof(num),1,f) != 1) {
		fclose(f);
		return false;
	}

	for (uint16_t i=0; i<num; i++) {
		char name2[16];
		// name
		if (fread(name2,16,1,f) != 1) break;
		// adr
		PhysPt adr;
		if (fread(&adr,sizeof(adr),1,f) != 1) break;
		// insert
		InsertVariable(name2,adr);
	}
	fclose(f);
	return true;
}

