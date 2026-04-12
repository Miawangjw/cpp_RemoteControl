#pragma once
#include <Windows.h>
//分别是
// 移动、
// 左键按下、
// 左键弹起、
// 右键按下、
// 右键弹起、
// 中键按下、
// 中键弹起、
// 左键单击、
// 右键单击、
// 中键单击、
// 左键双击、
// 右键双击、
// 中键双击
enum MOUSE_TYPE {
    MOUSE_MOVE = 1,
    MOUSE_LDOWN = 2,
    MOUSE_LUP = 3,
    MOUSE_RDOWN = 4,
    MOUSE_RUP = 5,
    MOUSE_MDOWN = 6,
    MOUSE_MUP = 7,
    MOUSE_LCLICK = 8,
    MOUSE_RCLICK = 9,
    MOUSE_MCLICK = 10,
    MOUSE_LDCLICK = 11,
    MOUSE_RDCLICK = 12,
    MOUSE_MDCLICK = 13
};

struct Mouse {
    int action; //鼠标行为
    POINT ptXY; //鼠标坐标
};
