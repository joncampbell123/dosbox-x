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
	
/* Determine if a character is wide based on its Unicode value */
bool isWideChar(uint32_t);

//          Purpose: Calculate the length of a UTF-8 encoded character.
//      Parameter 1: The first byte of the UTF-8 encoded character (char)
// Return value > 0: Length of the UTF-8 encoded character
//              = 0: Illegal UTF-8 encoding (first byte)
//
//
//
int getUtfLength(char);

//          Purpose: Calculate the length of a UTF-8 encoded character.
//      Parameter 1: The first byte of the UTF-8 encoded character (uint8_t)
// Return value > 0: Length of the UTF-8 encoded character
//              = 0: Illegal UTF-8 encoding (first byte)
//
//
//
int getUtfLength(uint8_t);

//          Purpose: Calculate the length of a UTF-8 encoded character.
//      Parameter 1: The first byte of the UTF-8 encoded character (uint32_t)
// Return value > 0: Length of the UTF-8 encoded character
//              = 0: Illegal UTF-8 encoding (first byte)
//
//
//
int getUtfLength(uint32_t);

//          Purpose: Convert UTF-8 encoded character to Unicode code point.
//        Parameter: Pointer to UTF-8 encoded character (char*)
// Return value > 0: The resulting Unicode code point.
//              = 0: Illegal UTF-8 encoding.
//
//
//
uint32_t fromUtf2Unicode(const char*);

//          Purpose: Convert UTF-8 encoded character to Unicode code point.
//        Parameter: Pointer to UTF-8 encoded character (uint8_t*)
// Return value > 0: The resulting Unicode code point.
//              = 0: Illegal UTF-8 encoding.
//
//
//
uint32_t fromUtf2Unicode(const uint8_t*);

//      Purpose: Check if a string contains valid UTF-8 encoding.
//  Parameter 1: Pointer to the string to be checked (char*)
// Return value: true  - The string contains no illegal UTF-8 encoding.
//               false - The string contains illegal UTF-8 encoding.
//        
//
//
bool isUtf8(const char*);

//      Purpose: Check if a string contains valid UTF-8 encoding.
//  Parameter 1: Pointer to the string to be checked (string&)
// Return value: true  - The string contains no illegal UTF-8 encoding.
//               false - The string contains illegal UTF-8 encoding.
//        
//
//
bool isUtf8(const string&);

//      Purpose: Check if a string contains valid UTF-8 encoding.
//  Parameter 1: Pointer to the string to be checked (uint8_t*)
// Return value: true  - The string contains no illegal UTF-8 encoding.
//               false - The string contains illegal UTF-8 encoding.
//        
//
//
bool isUtf8(const uint8_t*);

//           Purpose: Count the number of characters in a string (both Chinese and English count as one character).
//       Parameter 1: The string to be counted (string&)
// Return value >= 0: Number of characters.
//               = 0: Empty string.
//              = -1: String contains illegal UTF-8 encoding.
//
//
int countChars(const string&);

//           Purpose: Count the number of characters in a string (both Chinese and English count as one character).
//       Parameter 1: The string to be counted (char*)
// Return value >= 0: Number of characters.
//               = 0: Empty string.
//              = -1: String contains illegal UTF-8 encoding.
//
//
int countChars(const char*);

//           Purpose: Count the number of characters in a string (both Chinese and English count as one character).
//       Parameter 1: The string to be counted (uint8_t*)
// Return value >= 0: Number of characters.
//               = 0: Empty string.
//              = -1: String contains illegal UTF-8 encoding.
//
//
int countChars(const uint8_t*);

//           Purpose: Count the number of characters in a string (both Chinese and English count as one character).
//       Parameter 1: The string to be counted (char*)
//       Parameter 2: Returns the total length of UTF-8 encoding (int&)
// Return value >= 0: Number of characters.
//               = 0: Empty string.
//              = -1: The string contains illegal UTF-8 encoding.
//
int countChars(const char*, int&);

//           Purpose: Count the number of characters in a string (both Chinese and English count as one character).
//       Parameter 1: The string to be counted (string&)
//       Parameter 2: Returns the total length of UTF-8 encoding (int&)
// Return value >= 0: Number of characters.
//               = 0: Empty string.
//              = -1: The string contains illegal UTF-8 encoding.
//
int countChars(const string&, int&);

//           Purpose: Count the number of characters in a string (both Chinese and English count as one character).
//       Parameter 1: The string to be counted (uint8_t*)
//       Parameter 2: Returns the total length of UTF-8 encoding (int&)
// Return value >= 0: Number of characters.
//               = 0: Empty string.
//              = -1: The string contains illegal UTF-8 encoding.
//
int countChars(const uint8_t*, int&);

//          Purpose: Get the display width of the first character in a string.
//        Parameter: The string (char*)
// Return value = 2: Wide character.
//              = 1: Normal character.
//              = 0: Empty string.
//             = -1: Illegal UTF-8 encoding.
//
int getFirstDisplayLength(const char*);

//          Purpose: Get the display width of the first character in a string.
//        Parameter: The string (string&)
// Return value = 2: Wide character.
//              = 1: Normal character.
//              = 0: Empty string.
//             = -1: Illegal UTF-8 encoding.
//
int getFirstDisplayLength(const string&);

//          Purpose: Get the display width of the first character in a string.
//        Parameter: The string (uint8_t*)
// Return value = 2: Wide character.
//              = 1: Normal character.
//              = 0: Empty string.
//             = -1: Illegal UTF-8 encoding.
//
int getFirstDisplayLength(const uint8_t*);

//          Purpose: Get the display width of a string.
//        Parameter: The string (char*)
// Return value > 0: Display width of the string.
//              = 0: Empty string.
//             = -1: String contains illegal UTF-8 encoding.
//
//
int getDisplayLength(const char*);

//          Purpose: Get the display width of a string.
//        Parameter: The string (string&)
// Return value > 0: Display width of the string.
//              = 0: Empty string.
//             = -1: String contains illegal UTF-8 encoding.
//
//
int getDisplayLength(const string&);

//          Purpose: Get the display width of a string.
//        Parameter: The string (uint8_t*)
// Return value > 0: Display width of the string.
//              = 0: Empty string.
//             = -1: String contains illegal UTF-8 encoding.
//
//
int getDisplayLength(const uint8_t*);

// For the following string manipulation functions, note that string indexing starts at 1, not 0.

//          Purpose: Calculate the number of bytes required to extract a substring from a UTF-8 string (does not perform the extraction).
//      Parameter 1: Original string (char*)
//      Parameter 2: Start position (in characters, both Chinese and English count as 1 character)
//      Parameter 3: Length to extract
// Return value > 0: Number of bytes required to extract the substring.
//              = 0: Empty string.
//              < 0: String contains illegal UTF-8 characters.          
int getMidBytes(const char*, int, int);

//          Purpose: Calculate the number of bytes required to extract a substring from a UTF-8 string (does not perform the extraction).
//      Parameter 1: Original string (string&)
//      Parameter 2: Start position (in characters, both Chinese and English count as 1 character)
//      Parameter 3: Length to extract
// Return value > 0: Number of bytes required to extract the substring.
//              = 0: Empty string.
//              < 0: String contains illegal UTF-8 characters.   
int getMidBytes(const string&, int, int);

//          Purpose: Calculate the number of bytes required to extract a substring from a UTF-8 string (does not perform the extraction).
//      Parameter 1: Original string (uint8_t*)
//      Parameter 2: Start position (in characters, both Chinese and English count as 1 character)
//      Parameter 3: Length to extract
// Return value > 0: Number of bytes required to extract the substring.
//              = 0: Empty string.
//              < 0: String contains illegal UTF-8 characters.          
int getMidBytes(const uint8_t*, int, int);

//      Purpose: Extract a substring from a UTF-8 string.
//  Parameter 1: Original string (char*)
//  Parameter 2: The extracted substring will be placed here (automatically null-terminated). The caller must allocate sufficient memory.
//  Parameter 3: Start position (in characters, both Chinese and English count as 1 character)
//  Parameter 4: Length to extract
// Return value: Actual number of characters extracted.
//
int getMidStr(const char*, uint8_t*, int, int);

//      Purpose: Extract a substring from a UTF-8 string.
//  Parameter 1: Original string (string&)
//  Parameter 2: The extracted substring will be placed here (automatically null-terminated). The caller must allocate sufficient memory.
//  Parameter 3: Start position (in characters, both Chinese and English count as 1 character)
//  Parameter 4: Length to extract
// Return value: Actual number of characters extracted.
//
int getMidStr(const string&, uint8_t*, int, int);

//      Purpose: Extract a substring from a UTF-8 string.
//  Parameter 1: Original string (uint8_t*)
//  Parameter 2: The extracted substring will be placed here (automatically null-terminated). The caller must allocate sufficient memory.
//  Parameter 3: Start position (in characters, both Chinese and English count as 1 character)
//  Parameter 4: Length to extract
// Return value: Actual number of characters extracted.
//
int getMidStr(const uint8_t*, uint8_t*, int, int);

//      Purpose: Extract a substring from a UTF-8 string and return it as a string type.
//  Parameter 1: Original string (char*)
//  Parameter 2: Start position (in characters, both Chinese and English count as 1 character)
//  Parameter 3: Length to extract
// Return value: The extracted substring.
//
//
string getMidStr(const char*, int, int);

//      Purpose: Extract a substring from a UTF-8 string and return it as a string type.
//  Parameter 1: Original string (string&)
//  Parameter 2: Start position (in characters, both Chinese and English count as 1 character)
//  Parameter 3: Length to extract
// Return value: The extracted substring.
//
//
string getMidStr(const string&, int, int);

//      Purpose: Extract a substring from a UTF-8 string and return it as a string type.
//  Parameter 1: Original string (uint8_t*)
//  Parameter 2: Start position (in characters, both Chinese and English count as 1 character)
//  Parameter 3: Length to extract
// Return value: The extracted substring.
//
string getMidStr(const uint8_t*, int, int);

//      Purpose: Extract the left substring from a UTF-8 string.
//  Parameter 1: Original string (char*)
//  Parameter 2: The extracted substring will be placed here (automatically null-terminated). The caller must allocate sufficient memory.
//  Parameter 3: Length to extract
// Return value: Actual number of characters extracted.
//
//
int getLeftStr(const char*, char*, int);

//      Purpose: Extract the left substring from a UTF-8 string.
//  Parameter 1: Original string (string&)
//  Parameter 2: The extracted substring will be placed here (automatically null-terminated). The caller must allocate sufficient memory.
//  Parameter 3: Length to extract
// Return value: Actual number of characters extracted.
//
//
int getLeftStr(const string&, char*, int);

//      Purpose: Extract the left substring from a UTF-8 string.
//  Parameter 1: Original string (string&)
//  Parameter 2: The extracted substring will be placed here (automatically null-terminated). The caller must allocate sufficient memory.
//  Parameter 3: Length to extract
// Return value: Actual number of characters extracted.
//
//
int getLeftStr(const string&, uint8_t*, int);

//      Purpose: Extract the left substring from a UTF-8 string.
//  Parameter 1: Original string (uint8_t&)
//  Parameter 2: The extracted substring will be placed here (automatically null-terminated). The caller must allocate sufficient memory.
//  Parameter 3: Length to extract
// Return value: Actual number of characters extracted.
//
//
int getLeftStr(const uint8_t*, uint8_t*, int);

//      Purpose: Extract the left substring from a UTF-8 string.
//  Parameter 1: Original string (char*)
//  Parameter 2: Length to extract
// Return value: The extracted substring (string).
//
//
//
string getLeftStr(const char*, int);

//      Purpose: Extract the left substring from a UTF-8 string.
//  Parameter 1: Original string (string&)
//  Parameter 2: Length to extract
// Return value: The extracted substring (string).
//
//
//
string getLeftStr(const string&, int);

//      Purpose: Extract the left substring from a UTF-8 string.
//  Parameter 1: Original string (uint8_t*)
//  Parameter 2: Length to extract
// Return value: The extracted substring (string).
//
//
//
string getLeftStr(const uint8_t*, int);

//      Purpose: Extract the right substring from a UTF-8 string.
//  Parameter 1: Original string (string&)
//  Parameter 2: The extracted substring will be placed here (automatically null-terminated). The caller must allocate sufficient memory.
//  Parameter 3: Length to extract
// Return value: Actual number of characters extracted.
//
//
int getRightStr(const string&, char*, int);

//      Purpose: Extract the right substring from a UTF-8 string.
//  Parameter 1: Original string (string&)
//  Parameter 2: The extracted substring will be placed here (automatically null-terminated). The caller must allocate sufficient memory.
//  Parameter 3: Length to extract
// Return value: Actual number of characters extracted.
//
//
int getRightStr(const string&, uint8_t*, int);

//      Purpose: Extract the right substring from a UTF-8 string.
//  Parameter 1: Original string (uint8_t*)
//  Parameter 2: The extracted substring will be placed here (automatically null-terminated). The caller must allocate sufficient memory.
//  Parameter 3: Length to extract
// Return value: Actual number of characters extracted.
//
//
int getRightStr(const uint8_t*, uint8_t*, int);

//      Purpose: Extract the right substring from a UTF-8 string.
//  Parameter 1: Original string (char*)
//  Parameter 2: Length to extract
// Return value: The extracted substring (string).
//
//
//
string getRightStr(const char*, int);

//      Purpose: Extract the right substring from a UTF-8 string.
//  Parameter 1: Original string (string&)
//  Parameter 2: Length to extract
// Return value: The extracted substring (string).
//
//
//
string getRightStr(const string&, int);

//      Purpose: Extract the right substring from a UTF-8 string.
//  Parameter 1: Original string (uint8_t*)
//  Parameter 2: Length to extract
// Return value: The extracted substring (string).
//
//
//
string getRightStr(const uint8_t*, int);

//          Purpose: Search for a specific substring within a UTF-8 string.
//      Parameter 1: Original string (char*)
//      Parameter 2: Substring to search for (char*)
//      Parameter 3: Search starting position (optional, defaults to beginning of string if omitted)
// Return value > 0: Position of the substring in the original string (any valid UTF-8 character counts as 1 character).
//              = 0: Not found.
//              < 0: Memory allocation failed.
int inStr(const char*, const char*, int start=1);

//          Purpose: Search for a specific substring within a UTF-8 string.
//      Parameter 1: Original string (string&)
//      Parameter 2: Substring to search for (string&)
//      Parameter 3: Search starting position (optional, defaults to beginning of string if omitted)
// Return value > 0: Position of the substring in the original string (any valid UTF-8 character counts as 1 character).
//              = 0: Not found.
//              < 0: Memory allocation failed.
int inStr(const string&, const string&, int start=1);

//          Purpose: Search for a specific substring within a UTF-8 string.
//      Parameter 1: Original string (uint8_t*)
//      Parameter 2: Substring to search for (uint8_t*)
//      Parameter 3: Search starting position (optional, defaults to beginning of string if omitted)
// Return value > 0: Position of the substring in the original string (any valid UTF-8 character counts as 1 character).
//              = 0: Not found.
//              < 0: Memory allocation failed.
int inStr(const uint8_t*, const uint8_t*, int start=1);

//      Purpose: Count the number of occurrences of a specific substring within a UTF-8 string.
//  Parameter 1: Original string (char*)
//  Parameter 2: Target substring (char*)
// Return value: Number of times the target substring appears in the original string.
//         
//        
//        
int countStr(const char*, const char*);

//      Purpose: Count the number of occurrences of a specific substring within a UTF-8 string.
//  Parameter 1: Original string (string&)
//  Parameter 2: Target substring (string&)
// Return value: Number of times the target substring appears in the original string.
//         
//        
//        
int countStr(const string&, const string&);

//      Purpose: Count the number of occurrences of a specific substring within a UTF-8 string.
//  Parameter 1: Original string (uint8_t*)
//  Parameter 2: Target substring (uint8_t*)
// Return value: Number of times the target substring appears in the original string.
//         
//        
//        
int countStr(const uint8_t*, const uint8_t*);

//      Purpose: Replace occurrences of a specified substring with a target substring in the original string.
//  Parameter 1: Original string (char*)
//  Parameter 2: Substring to be replaced (char*)
//  Parameter 3: Replacement substring (char*)
//  Parameter 4: true -> replace all occurrences, false -> replace only the first occurrence (optional, defaults to replace all)
// Return value: The resulting string after replacement (string).
//        
string replaceStr(const char*, const char*, const char*, bool mode=true);

//      Purpose: Replace occurrences of a specified substring with a target substring in the original string.
//  Parameter 1: Original string (string&)
//  Parameter 2: Substring to be replaced (string&)
//  Parameter 3: Replacement substring (string&)
//  Parameter 4: true -> replace all occurrences, false -> replace only the first occurrence (optional, defaults to replace all)
// Return value: The resulting string after replacement (string).
//        
string replaceStr(const string&, const string&, const string&, bool mode=true);

//      Purpose: Replace occurrences of a specified substring with a target substring in the original string.
//  Parameter 1: Original string (uint8_t*)
//  Parameter 2: Substring to be replaced (uint8_t*)
//  Parameter 3: Replacement substring (uint8_t*)
//  Parameter 4: true -> replace all occurrences, false -> replace only the first occurrence (optional, defaults to replace all)
// Return value: The resulting string after replacement (string).
//        
string replaceStr(const uint8_t*, const uint8_t*, const uint8_t*, bool mode=true);

//           Purpose: Split the original string into multiple substrings. Requires a pre-defined vector<string> to store the split results.
//       Parameter 1: Original string (char*)
//       Parameter 2: Delimiter string (char*)
//       Parameter 3: Vector to store the split substrings (vector<string>&)
// Return value >= 0: Total number of substrings after splitting.
//               < 0: Original string contains illegal UTF-8 characters.
//        
int splitStr(const char*, const char*, vector<string>&);

//           Purpose: Split the original string into multiple substrings. Requires a pre-defined vector<string> to store the split results.
//       Parameter 1: Original string (string&)
//       Parameter 2: Delimiter string (string&)
//       Parameter 3: Vector to store the split substrings (vector<string>&)
// Return value >= 0: Total number of substrings after splitting.
//               < 0: Original string contains illegal UTF-8 characters.
//        
int splitStr(const string&, const string&, vector<string>&);

//           Purpose: Split the original string into multiple substrings. Requires a pre-defined vector<string> to store the split results.
//       Parameter 1: Original string (uint8_t*)
//       Parameter 2: Delimiter string (uint8_t*)
//       Parameter 3: Vector to store the split substrings (vector<string>&)
// Return value >= 0: Total number of substrings after splitting.
//               < 0: Original string contains illegal UTF-8 characters.
//        
int splitStr(const uint8_t*, const uint8_t*, vector<string>&);

std::string convertBig5toUtf8(char *src, int *status);
std::string convertBig5toUtf8(std::string src, int *status);

}

#endif