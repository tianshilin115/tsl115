/**
 * Created by root on 19-4-12.
 * 描述：
 * Copyright (c) 2013 - 2019 成都安舟. All rights reserved.
 */


#include "Common.h"

/**
 * 网络接口是否在配置列表中
 * @param value
 * @param arry
 * @param length
 * @return
 */
bool isValueInArry(int value, list<uint16_t> arry) {
    for (uint16_t arry_value:arry) {
        if (value == arry_value)
            return true;
    }
    return false;
}

/**
 * 获取多数据
 * @param list_string
 * @param result
 * @param max_len
 * @param split_car
 */
void stringToUint16List(char *list_string, list<uint16_t> *result, int max_len, char split_char) {
    char *values[max_len];
    int port_num = (unsigned) rte_strsplit(list_string, sizeof(list_string), values, max_len, split_char);
    for (int i = 0; i < port_num; i++) {
        result->push_front((uint16_t) atoi(values[i]));
    }
}

/**
 * 获取string多数据
 * @param list_string
 * @param result
 * @param max_len
 * @param split_car
 */
void stringToStringList(char *list_string, list<string> *result, int max_len, char split_char) {
    char *values[max_len];
    int port_num = (unsigned) rte_strsplit(list_string, strlen(list_string), values, max_len, split_char);
    for (int i = 0; i < port_num; i++) {
        result->push_front(values[i]);
    }
}

/**
 *
 * @param value
 * @return
 */
bool intValueIsPowerOf2(uint64_t value) {
    if ((value & (value - 1)) == 0)
        return true;
    else
        return false;
}

/**
 *
 * @param source
 * @param dest
 * @param sourceLen
 */
void ByteToHexStr(const unsigned char *source, char *dest, int sourceLen) {
    short i;
    unsigned char highByte, lowByte;

    for (i = 0; i < sourceLen; i++) {
        highByte = source[i] >> 4;
        lowByte = source[i] & 0x0f;

        highByte += 0x30;

        if (highByte > 0x39)
            dest[i * 2] = highByte + 0x07;
        else
            dest[i * 2] = highByte;

        lowByte += 0x30;
        if (lowByte > 0x39)
            dest[i * 2 + 1] = lowByte + 0x07;
        else
            dest[i * 2 + 1] = lowByte;
    }
    return;
}

