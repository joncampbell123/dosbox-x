#include <iostream>
#include <fstream>
#include <string>
#include "utf8.h"

using namespace std;
using namespace utf8;

string keyName = ":CHEAT_HELP_";
int keyCnt = 1;
string SEPARATE = " ,.;:|\t";
ofstream outFile;

string toLower(string p1){
	for (char &c : p1){
		if (c >= 'A' && c <= 'Z') {
			c |= 0x20;
		}
	}
	return p1;
}

string toUpper(string p1){
	for (char &c : p1){
		if (c >= 'a' && c <= 'z') {
			c &= ~0x20;
		}
	}
	return p1;
}

bool openRead(const string& filename, ifstream& handle) {
	handle.open(filename);
	if (!handle.is_open())
		return false;
	
	return true;
}

bool openWrite(const string& filename, ofstream& handle) {
	handle.open(filename);
	if (!handle.is_open())
		return false;
	
	return true;
}

bool isSeparate(string p1){
	if (isWideChar(fromUtf2Unicode(p1.c_str())))
		return true;
	
	for (int i=1; i<=countChars(SEPARATE); i++){
		if (p1 == getMidStr(SEPARATE, i, 1))
			return true;
	}
	return false;
}

string result = "";
string word = "";

void processWord(string& c){
	int len1 = getDisplayLength(result);
	int len2 = getDisplayLength(word);
	int len3 = getDisplayLength(c);
	int sum = len1 + len2 + len3;
	//int sum = len1 + len2;
	
	if (sum <= 78) {
		result = result + word + c;
		//result = result + word;
		word = "";
		c = "";
	} else {
		sum = len1 + len2;
		if (sum == 78){
			string keyLine = keyName + to_string(keyCnt);
			outFile << keyLine << endl;
			outFile << result << word << endl;
			outFile << "." << endl;
			result = c;
			word = "";
			c = "";
			keyCnt++;
		} else {
		  string keyLine = keyName + to_string(keyCnt);
		  outFile << keyLine << endl;
		  outFile << result << endl;
		  outFile << "." << endl;
		  result = word + c;
		  //result = word;
		  word = "";
		  c = "";
		  keyCnt++;
		}
	}
}

void processChar(string& c){
	if (isSeparate(c)) {
		processWord(c);
	} else {
		word = word + c;
	}
	//word = word + c;
}

void processLine(const string& str){
	string tmp = "";
	
	for (int i=1; i<=countChars(str); i++){
		tmp = getMidStr(str, i, 1);
		processChar(tmp);
	}
	
	/*if (!result.empty()) {
		string keyLine = keyName + to_string(keyCnt);
		outFile << keyLine << endl;
		outFile << result << endl;
		outFile << "." << endl;
	}*/
}

int main(int argc, char* argv[]){
	
	if (argc < 2){
		cout << "Need to provide parameters." << endl;
		return 1;
	}
	
	string inFileName = string(argv[1]) + ".txt";
	string outFileName = string(argv[1]) + ".lng";
	string buf = string(toUpper(argv[1]));
	size_t pos = buf.find('_');
	if (pos != string::npos) {
		keyName = keyName + buf.substr(0, pos);
	} else {
		cout << "Can't combine keyName!" << endl;
		return 1;
	}
	
	ifstream inFile;
	
	if (!openRead(inFileName, inFile)){
		cout << "Can't open source text file!" << endl;
		return 2;
	}
	
	if (!openWrite(outFileName, outFile)) {
		cout << "Can't open destiny file!" << endl;
		return 2;
	}
	
	string line;
	
	bool first = true;
	while (getline(inFile, line)) {
		if (first){
			first = false;
		} else {
			if (!result.empty()){
			  string keyLine = keyName + to_string(keyCnt);
				result = result + word;
			  outFile << keyLine << endl;
		    outFile << result << endl;
		    outFile << "." << endl;
				result = "";
				word = "";
				keyCnt++;
			}
		}
		processLine(line);
	}
	
	if (!result.empty()){
	  string keyLine = keyName + to_string(keyCnt);
	  outFile << keyLine << endl;
	  outFile << result << word << endl;
	  outFile << "." << endl;
		result = "";
	}
	
	inFile.close();
	outFile.close();
	
	return 0;
}