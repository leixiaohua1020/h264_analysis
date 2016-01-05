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
#include "SpecialVH264Dlg.h"

char* probe_nal_unit(char* filename,int data_offset,int data_lenth);
int h264_analyze(int argc, char *argv[]);