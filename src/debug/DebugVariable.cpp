#include "DebugVariable.h"

std::vector<DebugVariable*> DebugVariable::allVariables;

DebugVariable::DebugVariable(char* _name, PhysPt _adr) {
    adr = _adr;
    safe_strncpy(name, _name, 16);
    hasvalue = false; value = 0;
};

char* DebugVariable::GetName(void) {
    return name;
}

PhysPt DebugVariable::GetAdr(void) {
    return adr;
}

void DebugVariable::SetValue(bool has, uint16_t val) {
    hasvalue = has; value = val;
}

uint16_t DebugVariable::GetValue(void) {
    return value;
}

bool DebugVariable::HasValue(void) {
    return hasvalue;
}


void DebugVariable::InsertVariable(char* name, PhysPt adr)
{
	allVariables.push_back(new DebugVariable(name,adr));
}

void DebugVariable::DeleteAll(void)
{
	std::vector<DebugVariable*>::iterator i;
	for(i=allVariables.begin(); i != allVariables.end(); ++i) {
		DebugVariable* bp = *i;
		delete bp;
	}
	(allVariables.clear)();
}

DebugVariable* DebugVariable::FindVar(PhysPt pt)
{
	if (allVariables.empty()) return 0;

	std::vector<DebugVariable*>::size_type s = allVariables.size();
	for(std::vector<DebugVariable*>::size_type i = 0; i != s; i++) {
		DebugVariable* bp = allVariables[i];
		if (bp->GetAdr() == pt) return bp;
	}
	return 0;
}

bool DebugVariable::SaveVars(char* name) {
	if (allVariables.size() > 65535) return false;

	FILE* f = fopen(name,"wb+");
	if (!f) return false;

	// write number of vars
	uint16_t num = (uint16_t)allVariables.size();
	fwrite(&num,1,sizeof(num),f);

	std::vector<DebugVariable*>::iterator i;
	DebugVariable* bp;
	for(i=allVariables.begin(); i != allVariables.end(); ++i) {
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

bool DebugVariable::LoadVars(char* name)
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

