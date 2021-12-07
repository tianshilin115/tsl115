/**
 * Created by root on 19-4-12.
 * 描述：
 * Copyright (c) 2013 - 2019 成都安舟. All rights reserved.
 */

#ifndef DPDKCAPTURE_COMMON_H
#define DPDKCAPTURE_COMMON_H
#include <iostream>
#include <list>
#include <rte_string_fns.h>
using namespace std;

//数据是否在列表中
bool isValueInArry(int value, list <uint16_t> arry);

//分割配置中的多值到list
void stringToUint16List(char *list_string, list<uint16_t> *result, int max_len, char split_char);

//分割配置中的多值到list
void stringToStringList(char *list_string, list<string> *result, int max_len, char split_char);

//
bool intValueIsPowerOf2(uint64_t value);

void ByteToHexStr(const unsigned char* source, char* dest, int sourceLen);


#endif //DPDKCAPTURE_COMMON_H
