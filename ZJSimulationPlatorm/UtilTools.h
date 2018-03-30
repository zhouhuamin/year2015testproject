/* 
 * File:   UtilTools.h
 * Author: root
 *
 * Created on 2015年8月1日, 下午5:14
 */

#ifndef UTILTOOLS_H
#define	UTILTOOLS_H

#include <iostream>
#include <string>
#include <errno.h>
#include <iconv.h>

using namespace std;

//Linux下 GB2312和UTF8转换接口
class CUtilTools  
{
public:
    CUtilTools(){};
    ~CUtilTools(){};
    
    //iInLen的长度不包括\0，应该用strlen。返回值是处理后的sOut长度
    static int Utf8ToGb2312(char *sOut, int iMaxOutLen, const char *sIn, int iInLen)
    {
        char *pIn = (char *)sIn;
        char *pOut = sOut;
        size_t ret;
        size_t iLeftLen=iMaxOutLen;
        iconv_t cd;

        cd = iconv_open("gb2312", "utf-8");
        if (cd == (iconv_t) - 1)
        {
            return -1;
        }
        size_t iSrcLen=iInLen;
        ret = iconv(cd, &pIn,&iSrcLen, &pOut,&iLeftLen);
        if (ret == (size_t) - 1)
        {
            //printf("%s\n", strerror(errno));
            iconv_close(cd);
            return -1;
        }

        iconv_close(cd);

        return (iMaxOutLen - iLeftLen);
    }
    
    //iInLen的长度不包括\0，应该用strlen。返回值是处理后的sOut长度
    static int Gb2312ToUtf8(char *sOut, int iMaxOutLen, const char *sIn, int iInLen)
    {
        char *pIn = (char *)sIn;
        char *pOut = sOut;
        size_t ret;
        size_t iLeftLen=iMaxOutLen;
        iconv_t cd;

        cd = iconv_open("utf-8", "gb2312");
        if (cd == (iconv_t) - 1)
        {
            return -1;
        }
        size_t iSrcLen=iInLen;
        ret = iconv(cd, &pIn,&iSrcLen, &pOut,&iLeftLen);
        if (ret == (size_t) - 1)
        {
            iconv_close(cd);
            return -1;
        }

        iconv_close(cd);

        return (iMaxOutLen - iLeftLen);
    }   
};


//int main(int argc, char* argv[])
//{
//    char* pszOri = "中文字符测试";
//    cout << "strlen:" << strlen(pszOri) << endl;
//    
//    char pszDst[50] = {0};
//    
//    int iLen = CUtilTools::Gb2312ToUtf8(pszDst, 50, pszOri, strlen(pszOri)); // Gb2312ToUtf8
//    
//    cout << iLen << "," << strerror(errno) << "," << pszDst << endl;
//    
//    cout << "-----------" << endl;
//    
//    char pszGbDst[50] = {0};  
//    int iNewLen = CUtilTools::Utf8ToGb2312(pszGbDst, 50, pszDst, iLen); // Utf8ToGb2312   
//    cout << iNewLen << "," << strerror(errno) << "," << pszGbDst << endl;
//    
//    
////    输出为：
////    size:12
////    18,Success,涓??瀛??娴??
////    -----------
////    12,Success,中文字符测试
////    
////    //可以看出，UTF8格式下，一个中文字符占三个字节；而GB2312下占两个字节。
//    
//    return 0;
//}

#endif	/* UTILTOOLS_H */

