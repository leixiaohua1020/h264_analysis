/* 
 * H.264 分析器
 * H.264 Analysis
 *
 * 雷霄骅 Lei Xiaohua
 * leixiaohua1020@126.com
 * 中国传媒大学/数字电视技术
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 * 
 * H.264码流分析工具
 * H.264 Stream Analysis Tools
 *
 */

#include "stdafx.h"
#include "SpecialVH264.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "NALParse.h"



typedef struct
{
  int startcodeprefix_len;      //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
  unsigned len;                 //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
  unsigned max_size;            //! Nal Unit Buffer size
  int forbidden_bit;            //! should be always FALSE
  int nal_reference_idc;        //! NALU_PRIORITY_xxxx
  int nal_unit_type;            //! NALU_TYPE_xxxx    
  char *buf;                    //! contains the first byte followed by the EBSP
  unsigned short lost_packets;  //! true, if packet loss is detected
  int data_offset;
} NALU_t;

FILE *bits = NULL;                //!< the bit stream file

static int FindStartCode2 (unsigned char *Buf)
{
	if(Buf[0]!=0 || Buf[1]!=0 || Buf[2] !=1) return 0; //判断是否为0x000001,如果是返回1
	else return 1;
}

static int FindStartCode3 (unsigned char *Buf)
{
	if(Buf[0]!=0 || Buf[1]!=0 || Buf[2] !=0 || Buf[3] !=1) return 0;//判断是否为0x00000001,如果是返回1
	else return 1;
}

//static bool flag = true;
static int info2=0, info3=0;

//-------------------
CSpecialVH264Dlg *dlg;


//这个函数输入为一个NAL结构体，主要功能为得到一个完整的NALU并保存在NALU_t的buf中，获取他的长度，填充F,IDC,TYPE位。
//并且返回两个开始字符之间间隔的字节数，即包含有前缀的NALU的长度
int GetAnnexbNALU (NALU_t *nalu)
{
  int pos = 0;
  int StartCodeFound, rewind;
  unsigned char *Buf;
    
  if ((Buf = (unsigned char*)calloc (nalu->max_size , sizeof(char))) == NULL) 
	  printf ("GetAnnexbNALU: Could not allocate Buf memory\n");

  nalu->startcodeprefix_len=3;//初始化码流序列的开始字符为3个字节
  
   if (3 != fread (Buf, 1, 3, bits))//从码流中读3个字节
	   {
		free(Buf);
		return 0;
	   }
   info2 = FindStartCode2 (Buf);//判断是否为0x000001 
   if(info2 != 1) 
   {
	//如果不是，再读一个字节
    if(1 != fread(Buf+3, 1, 1, bits))//读一个字节
		{
		 free(Buf);
		 return 0;
		}
    info3 = FindStartCode3 (Buf);//判断是否为0x00000001
    if (info3 != 1)//如果不是，返回-1
		{ 
		 free(Buf);
		 return -1;
		}
    else 
		{
		//如果是0x00000001,得到开始前缀为4个字节
		 pos = 4;
		 nalu->startcodeprefix_len = 4;
		}
   }
   
   else
	   {
	   //如果是0x000001,得到开始前缀为3个字节
		nalu->startcodeprefix_len = 3;
		pos = 3;
	   }
   //查找下一个开始字符的标志位
   StartCodeFound = 0;
   info2 = 0;
   info3 = 0;
  
  while (!StartCodeFound)
  {
    if (feof (bits))//判断是否到了文件尾
    {
      nalu->len = (pos-1)-nalu->startcodeprefix_len;
      memcpy (nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);     
      nalu->forbidden_bit = nalu->buf[0] & 0x80; //1 bit
	  nalu->nal_reference_idc = nalu->buf[0] & 0x60; // 2 bit
	  nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;// 5 bit
      free(Buf);
      return pos-1;
    }
    Buf[pos++] = fgetc (bits);//读一个字节到BUF中
    info3 = FindStartCode3(&Buf[pos-4]);//判断是否为0x00000001
    if(info3 != 1)
      info2 = FindStartCode2(&Buf[pos-3]);//判断是否为0x000001
    StartCodeFound = (info2 == 1 || info3 == 1);
  }
  

 
  // Here, we have found another start code (and read length of startcode bytes more than we should
  // have.  Hence, go back in the file
  rewind = (info3 == 1)? -4 : -3;

  if (0 != fseek (bits, rewind, SEEK_CUR))//把文件指针指向前一个NALU的末尾
  {
    free(Buf);
	printf("GetAnnexbNALU: Cannot fseek in the bit stream file");
  }

  // Here the Start code, the complete NALU, and the next start code is in the Buf.  
  // The size of Buf is pos, pos+rewind are the number of bytes excluding the next
  // start code, and (pos+rewind)-startcodeprefix_len is the size of the NALU excluding the start code

  nalu->len = (pos+rewind)-nalu->startcodeprefix_len;
  memcpy (nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);//拷贝一个完整NALU，不拷贝起始前缀0x000001或0x00000001
  nalu->forbidden_bit = nalu->buf[0] & 0x80; //1 bit
  nalu->nal_reference_idc = nalu->buf[0] & 0x60; // 2 bit
  nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;// 5 bit
  free(Buf);
 
  return (pos+rewind);//返回两个开始字符之间间隔的字节数，即包含有前缀的NALU的长度
}


int h264_nal_parse(LPVOID lparam,char *fileurl)
{
	bits=fopen(fileurl, "r+b");
	if ( bits== NULL){
		AfxMessageBox(_T("Error open file"));
		return -1;
	}
	
	NALU_t *n;
	int buffersize=800000;

	if ((n = (NALU_t*)calloc (1, sizeof (NALU_t))) == NULL){
		AfxMessageBox(_T("Error AllocNALU: n"));
		return -1;
	}
	n->max_size=buffersize;
	if ((n->buf = (char*)calloc (buffersize, sizeof (char))) == NULL){
		free (n);
		AfxMessageBox (_T("Error AllocNALU: n->buf"));
		return -1;
	}
	
	//------------------
	int data_offset=0;
	//赋值-----------------
	dlg=(CSpecialVH264Dlg *)lparam;
	//----------
	int nal_num=0;
	//----------
	while(!feof(bits)) {
		int data_lenth;
		data_lenth=GetAnnexbNALU(n);//每执行一次，文件的指针指向本次找到的NALU的末尾，下一个位置即为下个NALU的起始码0x000001
		n->data_offset=data_offset;
		data_offset=data_offset+data_lenth;
		//输出NALU长度和TYPE
		int nal_reference_idc=n->nal_reference_idc>>5;
		dlg->AppendNLInfo(nal_reference_idc,n->nal_unit_type,n->len,n->len+n->startcodeprefix_len,n->data_offset);

		//判断是否选择了“只分析5000条”，如果选择了就不再分析了
		if(dlg->m_vh264nallistmaxnum.GetCheck()==1&&nal_num>5000){
			break;
		}
		nal_num++;
	}

	if (n){
		if (n->buf){
			free(n->buf);
			n->buf=NULL;
		}
		free (n);
	}
	return 0;
}


