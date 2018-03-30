#pragma once
#include "includes.h"
#include <time.h>
#include <string>

#define MAX_BUF_SIZE        256
#define LCD_SIZE_MAX        1024
using namespace std;


int barcode_dev_open(int *p_fd_led);
std::string& trim(std::string &s);
void ByteToHexStr(unsigned char* source, char* dest, int sourceLen);
void Hex2Str(char *sSrc,  char *sDest, int nSrcLen );
void HexStrToByte(const char* source, unsigned char* dest, int sourceLen);  
