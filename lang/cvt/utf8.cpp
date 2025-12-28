#include "utf8.h"

namespace utf8{

bool isWideChar(uint32_t unicode){
	if (BOXDRAWSTYLE==1){
		return (unicode >= SEC0_LOW && unicode <= SEC0_HIGH) ? true :
	         (unicode >= SEC1_LOW && unicode <= SEC1_HIGH) ? true :
				   (unicode >= SEC2_LOW && unicode <= SEC2_HIGH) ? true :
				   (unicode >= SEC3_LOW && unicode <= SEC3_HIGH) ? true :
				   (unicode >= SEC4_LOW && unicode <= SEC4_HIGH) ? true :
				   (unicode >= SEC5_LOW && unicode <= SEC5_HIGH) ? true :
				   (unicode >= SEC6_LOW && unicode <= SEC6_HIGH) ? true :
				   (unicode >= SEC7_LOW && unicode <= SEC7_HIGH) ? true :
				   (unicode >= SEC8_LOW && unicode <= SEC8_HIGH) ? true : false;
	}
	
	return (unicode >= SEC0_LOW && unicode <= SEC0_HIGH) ? false :
	       (unicode >= SEC1_LOW && unicode <= SEC1_HIGH) ? true :
				 (unicode >= SEC2_LOW && unicode <= SEC2_HIGH) ? true :
				 (unicode >= SEC3_LOW && unicode <= SEC3_HIGH) ? true :
				 (unicode >= SEC4_LOW && unicode <= SEC4_HIGH) ? true :
				 (unicode >= SEC5_LOW && unicode <= SEC5_HIGH) ? true :
				 (unicode >= SEC6_LOW && unicode <= SEC6_HIGH) ? true :
				 (unicode >= SEC7_LOW && unicode <= SEC7_HIGH) ? true :
				 (unicode >= SEC8_LOW && unicode <= SEC8_HIGH) ? true : false;
}

int getUtfLength(char code){
	return ((uint8_t)code);
}	
	
int getUtfLength(uint8_t code){
  return (code >> 7)   == 0    ? 1 :
	       (code & 0xFC) == 0xFC ? 6 :
	       (code & 0xF8) == 0xF8 ? 5 :
	       (code & 0xF0) == 0xF0 ? 4 :
	       (code & 0xE0) == 0xE0 ? 3 :
	       (code & 0xC0) == 0xC0 ? 2 : 0;
}

int getUtfLength(uint32_t code){
  return (code <= 0x7F)      ? 1 :
	       (code <= 0x7FF)     ? 2 :
				 (code <= 0xFFFF)    ? 3 :
				 (code <= 0x1FFFFF)  ? 4 :
				 (code <= 0x3FFFFFF) ? 5 :
				 (code <= 0x7FFFFFF) ? 6 : 0;
}

uint32_t fromUtf2Unicode(const char* utf){
	return fromUtf2Unicode((uint8_t*)utf);
}

uint32_t fromUtf2Unicode(const uint8_t* utf){
	uint32_t unicode = 0;
	uint32_t mask = 0xFF;
	int len = getUtfLength(utf[0]);
	
	if (len == 0)
		return 0;
	unicode = ((utf[0] << len) & mask) >> len;
	for (int i=1; i<len; i++){
		if (((utf[i]>>6)^0x02) != 0)
			return 0;
		unicode <<= 6;
		unicode += (utf[i] & 0x3F);
	}
	
	return unicode;
}

bool isUtf8(const char* str){
	return isUtf8((uint8_t*)str);
}

bool isUtf8(const string& str){
	return isUtf8((uint8_t*)str.c_str());
}
	
bool isUtf8(const uint8_t* str){
	uint8_t temp;
	int len;
	
	for (uint32_t s=0; str[s]!=0; s=s+len){
		len = getUtfLength(str[s]);
	  if (len==0)
		  return false;
		
	  for (int i=1; i<len; i++){
		  temp = (str[s+i]>>6)^0x02;
		  if (temp != 0){
			  return false;
			}
	  }
	}
		
	return true;
}
	
int countChars(const char* str){
	return countChars((uint8_t*)str);
}	
	
int countChars(const string& str){
	return countChars((uint8_t*)str.c_str());
}
	
int countChars(uint8_t const* str){
  int temp;
  int i = 0;
	int len = 0;
	
	if (!isUtf8(str))
		return -1;
	
	while (str[i] != 0){
		temp = getUtfLength(str[i]);
	  if (temp == 0){
			return -1;
		}
	  len = len + 1;
	  i = i + temp;
  }
	
  return len;
}

int countChars(const char* str, int& codelen){
	return countChars((uint8_t*)str, codelen);
}

int countChars(const string& str, int& codelen){
	return countChars((uint8_t*)str.c_str(), codelen);
}

int countChars(const uint8_t* str, int& codelen){
  int temp;
  int i = 0;
	int len = 0;
	
	codelen = 0;
	
	if (!isUtf8(str)){
		codelen = -1;
		return -1;
	}
	
	while (str[i] != 0){
		temp = getUtfLength(str[i]);
	  if (temp == 0)
			return -1;
	  len = len + 1;
	  i = i + temp;
		codelen = codelen + temp;
  }
	
  return len;
}

int getFirstDisplayLength(const char* str){
	return getFirstDisplayLength((uint8_t*)str);
}

int getFirstDisplayLength(const string& str){
	return getFirstDisplayLength((uint8_t*)str.c_str());
}

int getFirstDisplayLength(const uint8_t* str){
	uint32_t unicode;
	
	if (str[0] == 0)
		return 0;
	
	unicode = fromUtf2Unicode(str);
	
	if (unicode==0)
		return -1;
	else
		return isWideChar(unicode) ? 2 : 1;
}

int getDisplayLength(const char* str){
	return getDisplayLength((uint8_t*)str);
}

int getDisplayLength(const string& str){
	return getDisplayLength((uint8_t*)str.c_str());
}

int getDisplayLength(const uint8_t* str){
	int result = 0;
	int len, dlen;
	int i = 0;
	
	if (!isUtf8(str))
		return -1;
	
	while (str[i] != 0){
		len = getUtfLength(str[i]);
		dlen = getFirstDisplayLength(&str[i]);
		result = result + dlen;
		i = i + len;
	}
	
	return result;
}

int getMidBytes(const char* src, int start, int length){
	return getMidBytes((uint8_t*)src, start, length);
}

int getMidBytes(const string& src, int start, int length){
	return getMidBytes((uint8_t*)src.c_str(), start, length);
}

int getMidBytes(const uint8_t* src, int start, int length){
	int rlen = 0;
	int rbytes = 0;
	int len;
	int ps = 0;
	int bps = 0;
	
	if (start <= 0 || start > countChars(src)){
		return rbytes;
	}
	
	if (length <= 0){
		return rbytes;
	}
	
	if (!isUtf8(src)){
		return -1;
	}
	
	rlen = 1;
	while (rlen < start){
		len = getUtfLength(src[ps]);
		ps = ps + len;
		rlen = rlen + 1;
	}
	
	rlen = 0;
	while (src[ps] != 0){
		len = getUtfLength(src[ps]);
		ps = ps + len;
		rbytes = rbytes + len;
		rlen = rlen + 1;
		if (rlen >= length){
			break;
		}
	}
	
	return rbytes;
}

int getMidStr(const char* src, uint8_t* dst, int start, int length){
	return getMidStr((uint8_t*)src, dst, start, length);
}

int getMidStr(const string& src, uint8_t* dst, int start, int length){
	//return getMidStr((uint8_t*)&src[0], dst, start, length);
	return getMidStr((uint8_t*)src.c_str(), dst, start, length);
}

int getMidStr(const uint8_t* src, uint8_t* dst, int start, int length){
	int rlen = 0;
	int len;
	int ps = 0;
	int bps = 0;
	
	if (start <= 0 || start > countChars(src)){
		dst[bps] = 0;
		return rlen;
	}
	
	if (length <= 0){
		dst[bps] = 0;
		return rlen;
	}
	
	rlen = 1;
	while (rlen < start && src[ps] != 0){
		len = getUtfLength(src[ps]);
		ps = ps + len;
		rlen = rlen + 1;
	}
	
	rlen = 0;
	while (src[ps] != 0){
		len = getUtfLength(src[ps]);
		for (int i=0; i<len; i++){
			dst[bps] = src[ps];
			ps++;
			bps++;
		}
		rlen = rlen + 1;
		if (rlen >= length){
			break;
		}
	}
	dst[bps] = 0;
	
	return rlen;
}

string getMidStr(const char* src, int start, int length){
	return getMidStr((uint8_t*)src, start, length);
}

string getMidStr(const string& src, int start, int length){
	//return getMidStr((uint8_t*)&src[0], start, length);
	return getMidStr((uint8_t*)src.c_str(), start, length);
}

string getMidStr(const uint8_t* src, int start, int length){
	int rbytes = getMidBytes(src, start, length);
	int rlen;
	
	if (rbytes <= 0)
		return string("");
	
	uint8_t* buf = (uint8_t*)malloc(rbytes+1);
	if (buf == NULL)
		return string("");
	
	rlen = getMidStr(src, buf, start, length);
	
	if (rlen <= 0){
		free(buf);
		return string("");
	}
	else{
		string result = string((char*)buf);
		free(buf);
		return result;
	}
}

int getLeftStr(const char* src, char* dst, int length){
	return getLeftStr((uint8_t*)src, (uint8_t*)dst, length);
}

int getLeftStr(const string& src, char* dst, int length){
	return getLeftStr((uint8_t*)src.c_str(), (uint8_t*)dst, length);
}

int getLeftStr(const string& src, uint8_t* dst, int length){
	//return getLeftStr((uint8_t*)&src[0], dst, length);
	return getLeftStr((uint8_t*)src.c_str(), dst, length);
}

int getLeftStr(const uint8_t* src, uint8_t* dst, int length){
	return getMidStr(src, dst, 1, length);
}

string getLeftStr(const char* src, int length){
	return getLeftStr((uint8_t*)src, length);
}

string getLeftStr(const string& src, int length){
	//return getLeftStr((uint8_t*)&src[0], length);
	return getLeftStr((uint8_t*)src.c_str(), length);
}

string getLeftStr(const uint8_t* src, int length){
	return getMidStr(src, 1, length);
}

int getRightStr(const string& src, char* dst, int length){
	return getRightStr((uint8_t*)src.c_str(), (uint8_t*)dst, length);
}

int getRightStr(const string& src, uint8_t* dst, int length){
	//return getRightStr((uint8_t*)&src[0], dst, length);
	return getRightStr((uint8_t*)src.c_str(), dst, length);
}

int getRightStr(const uint8_t* src, uint8_t* dst, int length){
	int rellen = countChars(src);
	
	if (length > rellen)
		length = rellen;
	
	return getMidStr(src, dst, rellen-length+1, length);
}

string getRightStr(const char* src, int length){
	return getRightStr((uint8_t*)src, length);
}

string getRightStr(const string& src, int length){
	return getRightStr((uint8_t*)src.c_str(), length);
}

string getRightStr(const uint8_t* src, int length){
	int rellen = countChars(src);
	
	if (length > rellen)
		length = rellen;
	
	return getMidStr(src, rellen-length+1, length);
}

int inStr(const char* src, const char* dst, int start){
	return inStr((uint8_t*)src, (uint8_t*)dst, start);
}

int inStr(const string& src, const string& dst, int start){
	return inStr((uint8_t*)src.c_str(), (uint8_t*)dst.c_str(), start);
}

int inStr(const uint8_t* src, const uint8_t* target, int start){
	const char* ps;
	int rlen = 1;
	int pt = 0;
	int temp = 0;
	
	if (target[0] == 0)
		return 0;
	
	while (rlen < start && src[pt] != 0){
		temp = getUtfLength(src[pt]);
		rlen = rlen + 1;
		pt = pt + temp;
	}
	
	ps = strstr((char*)&src[pt], (char*)target);
	
	if (ps==NULL)
		return 0;
	
	int len = (int)(ps - (char*)src);
	
	char* buf = (char*)malloc(len+1);
	if (buf == NULL)
		return -1;
	
	for (int i=0; i<len; i++)
		buf[i] = (char)src[i];
	buf[len] = 0;
	
	int pos = countChars(buf) + 1;
	
	free(buf);

	return pos;
}

int countStr(const char* src, const char* target){
	int ps = 1, start = 1;
	int cnt = 0;
	
	int len = countChars(target);

  ps = inStr(src, target, start);
	while (ps > 0){
		cnt++;
		start = ps + len;
		ps = inStr(src, target, start);
	}
	
	return cnt;
}

int countStr(const string& src, const string& target){
	return countStr((char*)src.c_str(), (char*)target.c_str());
}

int countStr(const uint8_t* src, const uint8_t* target){
	return countStr((char*)src, (char*)target);
}

string replaceStr(const char* src, const char* target, const char* replace, bool mode){
  return replaceStr((uint8_t*)src, (uint8_t*)target, (uint8_t*)replace, mode);	
}

string replaceStr(const string& src, const string& target, const string& replace, bool mode){
  return replaceStr((uint8_t*)src.c_str(), (uint8_t*)target.c_str(), (uint8_t*)replace.c_str(), mode);	
}

string replaceStr(const uint8_t* src, const uint8_t* target, const uint8_t* replace, bool mode){
	bool first = true;
	int len, ps, i, targetlen;
	
	string result((char*)src);
	len = countChars(result);
	targetlen = countChars(target);
	
	if (len == -1 || targetlen == -1)
		return string("");
	
	i = 1;
	ps = 1;
	while (i <= len && ps != 0){
		ps = inStr((uint8_t*)result.c_str(), target, i);
		if (ps != 0){
			result = getLeftStr(result, ps-1) + string((char*)replace) + getRightStr(result, len-(ps+targetlen)+1);
      len = countChars(result);			
			if (first){
				first = false;
				if (!mode)
					break;
			}
		}
		i = ps;
	}
	
	return result;
}

int splitStr(const char* src, const char* target, vector<string>& buf){
  if (!isUtf8(src))
		return -1;
	
	if (!isUtf8(target))
		return -1;
	
	int len = countChars(target);
	int ps, cnt = 0;
	int start = 1, end=0 - len;
	string temp;
	while (ps=inStr(src, target, start)){
		end = ps - 1;
		if (start > end){
			temp = "";
			start = ps + len;
		} else {
			temp = getMidStr(src, start, end-start+1);
			start = ps + len;
		}
		buf.push_back(temp);
		cnt++;
	}
	start = end + len + 1;
	end = countChars(src);
	if (start > end)
		temp = "";
	else
		temp = getMidStr(src, start, end-start+1);
	buf.push_back(temp);
	cnt++;

  return cnt;  
}

int splitStr(const string& src, const string& target, vector<string>& buf){
  if (!isUtf8(src))
		return -1;
	
	if (!isUtf8(target))
		return -1;
	
  return splitStr(src.c_str(), target.c_str(), buf);  
}

int splitStr(const uint8_t* src, const uint8_t* target, vector<string>& buf){
  if (!isUtf8(src))
		return -1;
	
	if (!isUtf8(target))
		return -1;
	
  return splitStr((char*)src, (char*)target, buf);  
}

std::string convertBig5toUtf8(char *src, int *status){
	char *InBuf, *OutBuf;
	char *TmpInBuf, *TmpOutBuf;
	iconv_t cd;
	size_t SrcLen, DstLen, Ret;
	
	*status = 0; /* A return value of 0 indicates the task was completed correctly. */
	if (src == NULL){
		*status = 1; /* A return value of 1 indicates the original string is NULL. */
		return std::string(""); /* Return an empty string. */
	}
	
	SrcLen = strlen(src);
	DstLen = SrcLen * 6 + 1;
	SrcLen = SrcLen + 1;
	if ((InBuf=(char*)malloc(SrcLen))==0){
		*status = 2; /* A return value of 2 indicates memory allocation failure. */
		return std::string(""); /* Return an empty string. */
	}
	
	if ((OutBuf=(char*)malloc(DstLen))==0){
		*status = 2; /* A return value of 2 indicates memory allocation failure. */
		free(InBuf);
		return std::string(""); /* Return an empty string. */
	}
	strcpy(InBuf, src);
	TmpInBuf = InBuf;
	TmpOutBuf = OutBuf;
	cd = iconv_open("utf8", "big5");
	if (cd == (iconv_t)-1){
		*status = 3; /* A return value of 3 indicates conversion initialization failed. */
		free(InBuf);
		free(OutBuf);
		return std::string(""); /* Return an empty string. */
	}
	
	Ret = iconv(cd, &TmpInBuf, &SrcLen, &TmpOutBuf, &DstLen);
	if (Ret == -1){
		*status = 4; /* A return value of 4 indicates conversion failure. */
		free(InBuf);
		free(OutBuf);
		iconv_close(cd);
		return std::string(""); /* Return an empty string. */
	}
	
	std::string result(OutBuf);
	free(InBuf);
	free(OutBuf);
	iconv_close(cd);
	return result;
}

std::string convertBig5toUtf8(std::string src, int *status){
	const char *tmp = src.c_str();
	return convertBig5toUtf8((char *)tmp, status);
}

}