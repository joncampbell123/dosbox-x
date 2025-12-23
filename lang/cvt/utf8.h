#ifndef __UTF8_H
#define __UTF8_H

#include <cstdint>
#include <string>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <iconv.h>
#include "wide.h"

using namespace std;

namespace utf8{
	
/* 根據unicode值判斷是否為寬字元 */
bool isWideChar(uint32_t);

// 用途：計算UTF8編碼長度	
// 參數1：UTF8編碼第一碼(char)
// 回傳值 > 0：UTF8編碼長度
//        = 0：不合法的UTF8編碼(第一碼)
//
//
//
int getUtfLength(char);

// 用途：計算UTF8編碼長度	
// 參數1：UTF8編碼第一碼(uint8_t)
// 回傳值 > 0：UTF8編碼長度
//        = 0：不合法的UTF8編碼(第一碼)
//
//
//
int getUtfLength(uint8_t);

// 用途：計算UTF8編碼長度	
// 參數1：unicode編碼(uint32_t)
// 回傳值 > 0：UTF8編碼長度
//        = 0：不合法的unicode編碼
//
//
//
int getUtfLength(uint32_t);

// 用途：將UTF8編碼轉換成unicode編碼
// 參數：UTF8編碼位址(char*)
// 回傳值 > 0：轉換後的unicode編碼
//        = 0：不合法的UTF8編碼
//
//
//
uint32_t fromUtf2Unicode(const char*);

// 用途：將UTF8編碼轉換成unicode編碼
// 參數：UTF8編碼位址(uint8_t*)
// 回傳值 > 0：轉換後的unicode編碼
//        = 0：不合法的UTF8編碼
//
//
//
uint32_t fromUtf2Unicode(const uint8_t*);

// 用途：判斷字串內容是不是合法的UTF8編碼
// 參數1：待判斷字串指標(char*)
// 回傳值：true 字串內沒有不合法的utf8編碼
//         false字串內含有不合法的utf8編碼
//        
//
//
bool isUtf8(const char*);

// 用途：判斷字串內容是不是合法的UTF8編碼
// 參數1：待判斷字串指標(string&)
// 回傳值：true 字串內沒有不合法的utf8編碼
//         false字串內含有不合法的utf8編碼
//        
//
//
bool isUtf8(const string&);

// 用途：判斷字串內容是不是合法的UTF8編碼
// 參數1：待判斷字串指標(uint8_t*)
// 回傳值：true 字串內沒有不合法的utf8編碼
//         false字串內含有不合法的utf8編碼
//        
//
//
bool isUtf8(const uint8_t*);

// 用途：計算字串的字元數(中文、英文都算1個字)
// 參數1：待計算字串(string&)
// 回傳值 >= 0：字元數
//         = 0：空字串
//         =-1：字串內有不合法的UTF8編碼
//
//
int countChars(const string&);

// 用途：計算字串的字元數(中文、英文都算1個字)
// 參數1：待計算字串(char*)
// 回傳值 >= 0：字元數
//         = 0：空字串
//          -1：字串內有不合法的UTF8編碼
//
//
int countChars(const char*);

// 用途：計算字串的字元數(中文、英文都算1個字)
// 參數1：待計算字串(uint8_t*)
// 回傳值 >= 0：字元數
//         = 0：空字串
//          -1：字串內有不合法的UTF8編碼
//
//
int countChars(const uint8_t*);

// 用途：計算字串的字元數(中文、英文都算1個字)
// 參數1：待計算字串(char*)
// 參數2：回傳UTF8編碼總長度(int&)
// 回傳值 >= 0：字元數
//         = 0：空字串
//          -1：字串內有不合法的UTF8編碼
//
int countChars(const char*, int&);

// 用途：計算字串的字元數(中文、英文都算1個字)
// 參數1：待計算字串(string&)
// 參數2：回傳UTF8編碼總長度(int&)
// 回傳值 >= 0：字元數
//         = 0：空字串
//          -1：字串內有不合法的UTF8編碼
//
int countChars(const string&, int&);

// 用途：計算字串的字元數(中文、英文都算1個字)
// 參數1：待計算字串(uint8_t*)
// 參數2：回傳UTF8編碼總長度(int&)
// 回傳值 >= 0：字元數
//         = 0：空字串
//          -1：字串內有不合法的UTF8編碼
//
int countChars(const uint8_t*, int&);

// 用途：取得字串第一個字的顯示寬度
// 參數：字串(char*)
// 回傳值 = 2：寬字元
//        = 1：一般字元
//        = 0：空字串
//        =-1：不合法的UTF8編碼
//
int getFirstDisplayLength(const char*);

// 用途：取得字串第一個字的顯示寬度
// 參數：字串(string&)
// 回傳值 = 2：寬字元
//        = 1：一般字元
//        = 0：空字串
//        =-1：不合法的UTF8編碼
//
int getFirstDisplayLength(const string&);

// 用途：取得字串第一個字的顯示寬度
// 參數：字串(uint8_t*)
// 回傳值 = 2：寬字元
//        = 1：一般字元
//        = 0：空字串
//        =-1：不合法的UTF8編碼
//
int getFirstDisplayLength(const uint8_t*);

// 用途：取得字串的顯示寬度
// 參數：字串(char*)
// 回傳值 > 0：字串顯示寬度
//        = 0：空字串
//        =-1：字串含有不合法的UTF8編碼
//
//
int getDisplayLength(const char*);

// 用途：取得字串的顯示寬度
// 參數：字串(string&)
// 回傳值 > 0：字串顯示寬度
//        = 0：空字串
//        =-1：字串含有不合法的UTF8編碼
//
//
int getDisplayLength(const string&);

// 用途：取得字串的顯示寬度
// 參數：字串(uint8_t*)
// 回傳值 > 0：字串顯示寬度
//        = 0：空字串
//        =-1：字串含有不合法的UTF8編碼
//
//
int getDisplayLength(const uint8_t*);

// 以下開始的字串操作函式，字串的起始位置都是1不是0，請注意

// 用途：計算攫取UTF8中間字串所需BYTES數(不會執行攫取動作)
// 參數1：原始字串(char*)
// 參數2：起始位置(以字元為單位，中英文都算1個字)
// 參數3：攫取長度
// 回傳值 > 0 ：攫取字串所需BYTES數
//        = 0 ：空字串
//        < 0 ：字串含有不合法的UTF8字元          
int getMidBytes(const char*, int, int);

// 用途：計算攫取UTF8中間字串所需BYTES數(不會執行攫取動作)
// 參數1：原始字串(string&)
// 參數2：起始位置(以字元為單位，中英文都算1個字)
// 參數3：攫取長度
// 回傳值 > 0 ：攫取字串所需BYTES數
//        = 0 ：空字串
//        < 0 ：字串含有不合法的UTF8字元          
int getMidBytes(const string&, int, int);

// 用途：計算攫取UTF8中間字串所需BYTES數(不會執行攫取動作)
// 參數1：原始字串(uint8_t*)
// 參數2：起始位置(以字元為單位，中英文都算1個字)
// 參數3：攫取長度
// 回傳值 > 0 ：攫取字串所需BYTES數
//        = 0 ：空字串
//        < 0 ：字串含有不合法的UTF8字元          
int getMidBytes(const uint8_t*, int, int);

// 用途：攫取UTF8中間字串
// 參數1：原始字串(char*)
// 參數2：攫取到的字串會放在這裡(結尾自動補0)，呼叫者須自行準備足夠的記憶體空間
// 參數3：起始位置(以字元為單位，中英文都算1個字)
// 參數4：攫取長度
// 回傳值：實際攫取的字元數
//
int getMidStr(const char*, uint8_t*, int, int);

// 用途：攫取UTF8中間字串
// 參數1：原始字串(string&)
// 參數2：攫取到的字串會放在這裡(結尾自動補0)，呼叫者須自行準備足夠的記憶體空間
// 參數3：起始位置(以字元為單位，中英文都算1個字)
// 參數4：攫取長度
// 回傳值：實際攫取的字元數
//
int getMidStr(const string&, uint8_t*, int, int);

// 用途：攫取UTF8中間字串
// 參數1：原始字串(uint8_t*)
// 參數2：攫取到的字串會放在這裡(結尾自動補0)，呼叫者須自行準備足夠的記憶體空間
// 參數3：起始位置(以字元為單位，中英文都算1個字)
// 參數4：攫取長度
// 回傳值：實際攫取的字元數
//
int getMidStr(const uint8_t*, uint8_t*, int, int);

// 用途：攫取UTF8中間字串，以string型式回傳攫取到的字串
// 參數1：原始字串(char*)
// 參數2：起始位置(以字元為單位，中英文都算1個字)
// 參數3：攫取長度
// 回傳值：攫取的字串
//
//
string getMidStr(const char*, int, int);

// 用途：攫取UTF8中間字串，以string型式回傳攫取到的字串
// 參數1：原始字串(string&)
// 參數2：起始位置(以字元為單位，中英文都算1個字)
// 參數3：攫取長度
// 回傳值：攫取的字串
//
//
string getMidStr(const string&, int, int);

// 用途：攫取UTF8中間字串，以string型式回傳攫取到的字串
// 函式會自動配置所需緩衝區，如果配置失敗將回傳空字串
// 參數1：原始字串(uint8_t*)
// 參數2：起始位置(以字元為單位，中英文都算1個字)
// 參數3：攫取長度
// 回傳值：攫取的字串
//
string getMidStr(const uint8_t*, int, int);

// 用途：攫取UTF8左字串
// 參數1：原始字串(char*)
// 參數2：攫取到的字串會放在這裡(結尾自動補0)，呼叫者須自行準備足夠的記憶體空間
// 參數3：攫取長度
// 回傳值：實際攫取的字元數
//
//
int getLeftStr(const char*, char*, int);

// 用途：攫取UTF8左字串
// 參數1：原始字串(string&)
// 參數2：攫取到的字串會放在這裡(結尾自動補0)，呼叫者須自行準備足夠的記憶體空間
// 參數3：攫取長度
// 回傳值：實際攫取的字元數
//
//
int getLeftStr(const string&, char*, int);

// 用途：攫取UTF8左字串
// 參數1：原始字串(string&)
// 參數2：攫取到的字串會放在這裡(結尾自動補0)，呼叫者須自行準備足夠的記憶體空間
// 參數3：攫取長度
// 回傳值：實際攫取的字元數
//
//
int getLeftStr(const string&, uint8_t*, int);

// 用途：攫取UTF8左字串
// 參數1：原始字串(uint8_t*)
// 參數2：攫取到的字串會放在這裡(結尾自動補0)，呼叫者須自行準備足夠的記憶體空間
// 參數3：攫取長度
// 回傳值：實際攫取的字元數
//
//
int getLeftStr(const uint8_t*, uint8_t*, int);

// 用途：攫取UTF8左字串
// 參數1：原始字串(char*)
// 參數2：攫取長度
// 回傳值：攫取到的字串(string)
//
//
//
string getLeftStr(const char*, int);

// 用途：攫取UTF8左字串
// 參數1：原始字串(string&)
// 參數2：攫取長度
// 回傳值：攫取到的字串(string)
//
//
//
string getLeftStr(const string&, int);

// 用途：攫取UTF8左字串
// 參數1：原始字串(uint8_t*)
// 參數2：攫取長度
// 回傳值：攫取到的字串(string)
//
//
//
string getLeftStr(const uint8_t*, int);

// 用途：攫取UTF8右字串
// 參數1：原始字串(char*)
// 參數2：攫取到的字串會放在這裡(結尾自動補0)，呼叫者須自行準備足夠的記憶體空間
// 參數3：攫取長度
// 回傳值：實際攫取的字元數
//
//
int getRightStr(const string&, char*, int);

// 用途：攫取UTF8右字串
// 參數1：原始字串(string&)
// 參數2：攫取到的字串會放在這裡(結尾自動補0)，呼叫者須自行準備足夠的記憶體空間
// 參數3：攫取長度
// 回傳值：實際攫取的字元數
//
//
int getRightStr(const string&, uint8_t*, int);

// 用途：攫取UTF8右字串
// 參數1：原始字串(uint8_t*)
// 參數2：攫取到的字串會放在這裡(結尾自動補0)，呼叫者須自行準備足夠的記憶體空間
// 參數3：攫取長度
// 回傳值：實際攫取的字元數
//
//
int getRightStr(const uint8_t*, uint8_t*, int);

// 用途：攫取UTF8右字串
// 參數1：原始字串(char*)
// 參數2：攫取長度
// 回傳值：攫取到的字串(string)
//
//
//
string getRightStr(const char*, int);

// 用途：攫取UTF8右字串
// 參數1：原始字串(string&)
// 參數2：攫取長度
// 回傳值：攫取到的字串(string)
//
//
//
string getRightStr(const string&, int);

// 用途：攫取UTF8右字串
// 參數1：原始字串(uint8_t*)
// 參數2：攫取長度
// 回傳值：攫取到的字串(string)
//
//
//
string getRightStr(const uint8_t*, int);

// 用途：搜尋UTF字串內是否有特定字串
// 參數1：原始字串(char*)
// 參數2：要尋找的字串(char*)
// 參數3：搜尋起始位置(可省略，表示從頭開始搜尋)
// 回傳值 > 0 ：要尋找字串在原始字串的位置(任何合法的UTF8字元都算1個字)
//        = 0 ：找不到
//        < 0 ：記憶體配置失敗
int inStr(const char*, const char*, int start=1);

// 用途：搜尋UTF字串內是否有特定字串
// 參數1：原始字串(string&)
// 參數2：要尋找的字串(string&)
// 參數3：搜尋起始位置(可省略，表示從頭開始搜尋)
// 回傳值 > 0 ：要尋找字串在原始字串的位置(任何合法的UTF8字元都算1個字)
//        = 0 ：找不到
//        < 0 ：記憶體配置失敗
int inStr(const string&, const string&, int start=1);

// 用途：搜尋UTF字串內是否有特定字串
// 參數1：原始字串(uint8_t*)
// 參數2：要尋找的字串(uint8_t*)
// 參數3：搜尋起始位置(可省略，表示從頭開始搜尋)
// 回傳值 > 0 ：要尋找字串在原始字串的位置(任何合法的UTF8字元都算1個字)
//        = 0 ：找不到
//        < 0 ：記憶體配置失敗
int inStr(const uint8_t*, const uint8_t*, int start=1);

// 用途：搜尋UTF字串內是否有特定字串
// 參數1：原始字串(char*)
// 參數2：目標字串(char*)
// 回傳值：目標字串在原始字串中出現的次數
//         
//        
//        
int countStr(const char*, const char*);

// 用途：搜尋UTF字串內是否有特定字串
// 參數1：原始字串(char*)
// 參數2：目標字串(char*)
// 回傳值：目標字串在原始字串中出現的次數
//         
//        
//        
int countStr(const string&, const string&);

// 用途：搜尋UTF字串內是否有特定字串
// 參數1：原始字串(uint8_t*)
// 參數2：目標字串(uint8_t*)
// 回傳值：目標字串在原始字串中出現的次數
//         
//        
//        
int countStr(const uint8_t*, const uint8_t*);

// 用途：將原始字串中的特定字串取代成目標字串
// 參數1：原始字串(char*)
// 參數2：指定字串(char*)
// 參數3：代換字串(char*)
// 參數4：true -> 全部取代，false -> 只取代第一個(可省略，表示全部取代)
// 回傳值：取代後的字串(string)
//        
string replaceStr(const char*, const char*, const char*, bool mode=true);

// 用途：將原始字串中的特定字串取代成目標字串
// 參數1：原始字串(string&)
// 參數2：指定字串(string&)
// 參數3：代換字串(string&)
// 參數4：true -> 全部取代，false -> 只取代第一個(可省略，表示全部取代)
// 回傳值：取代後的字串(string)
//        
string replaceStr(const string&, const string&, const string&, bool mode=true);

// 用途：將原始字串中的特定字串取代成目標字串
// 參數1：原始字串(uint8_t*)
// 參數2：指定字串(uint8_t*)
// 參數3：代換字串(uint8_t*)
// 參數4：true -> 全部取代，false -> 只取代第一個(可省略，表示全部取代)
// 回傳值：取代後的字串(string)
//        
string replaceStr(const uint8_t*, const uint8_t*, const uint8_t*, bool mode=true);

// 用途：將原始字串分解成數個字串，使用需自備vector<string>用來儲存分解後的字串
// 參數1：原始字串(char*)
// 參數2：分隔字串(char*)
// 參數3：儲存分解後之字串(vector<string>&)
// 回傳值 >= 0：分解後字串總數
//        < 0 ：原始字串含有不合法的UTF8字元
//        
int splitStr(const char*, const char*, vector<string>&);

// 用途：將原始字串分解成數個字串，使用需自備vector<string>用來儲存分解後的字串
// 參數1：原始字串(string&)
// 參數2：分隔字串(string&)
// 參數3：儲存分解後之字串(vector<string>&)
// 回傳值 >= 0：分解後字串總數
//        < 0 ：原始字串含有不合法的UTF8字元
//        
int splitStr(const string&, const string&, vector<string>&);

// 用途：將原始字串分解成數個字串，使用需自備vector<string>用來儲存分解後的字串
// 參數1：原始字串(uint8_t*)
// 參數2：分隔字串(uint8_t*)
// 參數3：儲存分解後之字串(vector<string>&)
// 回傳值 >= 0：分解後字串總數
//        < 0 ：原始字串含有不合法的UTF8字元
//        
int splitStr(const uint8_t*, const uint8_t*, vector<string>&);

std::string convertBig5toUtf8(char *src, int *status);
std::string convertBig5toUtf8(std::string src, int *status);

}

#endif