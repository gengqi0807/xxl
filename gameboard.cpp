#include "gameboard.h"
#include <QRandomGenerator>
#include <QDebug>

GameBoard::GameBoard(QObject *parent) : QObject(parent) {}

/* 对外接口：生成无三连且非死局棋盘 */
void GameBoard::initNoThree()
{
    Grid tmp;
    int safetyCount = 0; // 【关键修复】防止死循环的保险丝

    do {
        // 1. 先生成无三连
        generateNoThreeAlone(tmp);
        safetyCount++;

        // 如果尝试超过 1000 次（极小概率），强制跳出，避免界面卡死
        if (safetyCount > 1000) {
            qDebug() << "Warning: Loop safety break triggered in initNoThree.";
            break;
        }
    } while (isDead(tmp));

    m_grid = tmp;
    emit gridUpdated();
}

/* 只生成无三连，不管死局 */
void GameBoard::generateNoThreeAlone(Grid &g)
{
    auto &rng = *QRandomGenerator::global();
    // 必须重置棋盘数据，防止脏数据干扰
    for(int r=0; r<ROW; ++r)
        for(int c=0; c<COL; ++c)
            g[r][c] = Spot();

    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            int color;
            do {
                color = rng.bounded(6);
            } while (
                (c >= 2 && g[r][c-1].pic == color && g[r][c-2].pic == color) ||
                (r >= 2 && g[r-1][c].pic == color && g[r-2][c].pic == color));
            g[r][c].pic = color;
        }
    }
}

/* 死局判定：O(ROW*COL*4) 足够快 */
bool GameBoard::isDead(const Grid &g)
{
    for (int r = 0; r < ROW; ++r) {
        for (int c = 0; c < COL; ++c) {
            // 只检查右、下两个方向
            if (c + 1 < COL && trySwapInternal(g, r, c, r, c + 1))
                return false; // 有解，非死局
            if (r + 1 < ROW && trySwapInternal(g, r, c, r + 1, c))
                return false; // 有解，非死局
        }
    }
    return true; // 所有位置都无法消除，死局
}

/* 内部使用的虚拟交换判定（简化版） */
bool GameBoard::trySwapInternal(const Grid &g, int r1, int c1, int r2, int c2)
{
    Grid t = g;
    std::swap(t[r1][c1].pic, t[r2][c2].pic);

    // 局部扫描范围
    int top    = qMax(0, qMin(r1, r2) - 2);
    int bottom = qMin(ROW - 1, qMax(r1, r2) + 2);
    int left   = qMax(0, qMin(c1, c2) - 2);
    int right  = qMin(COL - 1, qMax(c1, c2) + 2);

    for (int r = top; r <= bottom; ++r) {
        for (int c = left; c <= right; ++c) {
            if (hasMatchInCross(t, r, c))
                return true;
        }
    }
    return false;
}

/* 快速扫描十字区有无 3 连 */
bool GameBoard::hasMatchInCross(const Grid &g, int r, int c)
{
    int color = g[r][c].pic;
    if (color < 0) return false;

    // 横向扫描
    int cnt = 1;
    for (int i = c - 1; i >= 0 && g[r][i].pic == color; --i) ++cnt;
    for (int i = c + 1; i < COL && g[r][i].pic == color; ++i) ++cnt;
    if (cnt >= 3) return true;

    // 纵向扫描
    cnt = 1;
    for (int i = r - 1; i >= 0 && g[i][c].pic == color; --i) ++cnt;
    for (int i = r + 1; i < ROW && g[i][c].pic == color; ++i) ++cnt;
    return cnt >= 3;
}

/* ========================================================= */
/* 以下是 UI 交互使用的复杂判定（含炸弹识别） */
/* ========================================================= */

bool GameBoard::trySwap(int r1, int c1, int r2, int c2)
{
    Grid tmp = m_grid;
    std::swap(tmp[r1][c1].pic, tmp[r2][c2].pic);

    MatchInfo info;
    int top    = qMax(0, qMin(r1, r2) - 2);
    int bottom = qMin(ROW - 1, qMax(r1, r2) + 2);
    int left   = qMax(0, qMin(c1, c2) - 2);
    int right  = qMin(COL - 1, qMax(c1, c2) + 2);

    for (int r = top; r <= bottom; ++r)
        for (int c = left; c <= right; ++c)
            info = mergeInfo(info, scanCross(tmp, r, c));

    // 只要有任何形式的匹配，就视为有效交换
    if (info.has3 || info.has4Line || info.has4Col ||
        info.has5Line || info.has5Col || info.hasTL) {
        return true;
    }
    return false;
}

GameBoard::MatchInfo GameBoard::scanCross(const Grid &g, int r, int c)
{
    MatchInfo ret;
    int color = g[r][c].pic;
    if (color < 0) return ret;

    /* 横向 */
    int cntL = 1, left = c, right = c;
    while (left - 1 >= 0 && g[r][left - 1].pic == color) { left--; cntL++; }
    while (right + 1 < COL && g[r][right + 1].pic == color) { right++; cntL++; }
    if (cntL >= 3) ret.has3 = true;
    if (cntL == 4) ret.has4Line = true;
    if (cntL >= 5) ret.has5Line = true;
    for (int i = left; i <= right; ++i) ret.cells.append(QPoint(r, i));

    /* 纵向 */
    int cntC = 1, top = r, bottom = r;
    while (top - 1 >= 0 && g[top - 1][c].pic == color) { top--; cntC++; }
    while (bottom + 1 < ROW && g[bottom + 1][c].pic == color) { bottom++; cntC++; }
    if (cntC >= 3) ret.has3 = true;
    if (cntC == 4) ret.has4Col = true;
    if (cntC >= 5) ret.has5Col = true;
    for (int i = top; i <= bottom; ++i) ret.cells.append(QPoint(i, c));

    /* T/L 型判断 */
    bool hMatch = (cntL >= 3);
    bool vMatch = (cntC >= 3);
    if (hMatch && vMatch) ret.hasTL = true;

    return ret;
}

GameBoard::MatchInfo GameBoard::mergeInfo(const MatchInfo &a, const MatchInfo &b)
{
    MatchInfo ret = a;
    ret.has3 |= b.has3;
    ret.has4Line |= b.has4Line; ret.has4Col |= b.has4Col;
    ret.has5Line |= b.has5Line; ret.has5Col |= b.has5Col;
    ret.hasTL |= b.hasTL;
    for (auto p : b.cells) if (!ret.cells.contains(p)) ret.cells.append(p);
    return ret;
}



/* 尝试顺时针旋转 2x2 区域，看是否能产生消除 */
/* r, c 是 2x2 区域左上角的坐标 */
bool GameBoard::tryRotate(int r, int c)
{
    // 边界检查
    if (r < 0 || r >= ROW - 1 || c < 0 || c >= COL - 1) return false;

    Grid tmp = m_grid;

    // 顺时针旋转逻辑：
    // [r][c]   -> [r][c+1]
    // [r][c+1] -> [r+1][c+1]
    // [r+1][c+1]-> [r+1][c]
    // [r+1][c] -> [r][c]

    Spot topLeft  = tmp[r][c];
    Spot topRight = tmp[r][c+1];
    Spot botRight = tmp[r+1][c+1];
    Spot botLeft  = tmp[r+1][c];

    // 执行交换
    tmp[r][c+1]     = topLeft;
    tmp[r+1][c+1]   = topRight;
    tmp[r+1][c]     = botRight;
    tmp[r][c]       = botLeft;

    // 检查 2x2 区域内涉及的行和列是否有消除
    // 只需要检查这四格所在的行(r, r+1)和列(c, c+1)即可
    MatchInfo info;
    info = mergeInfo(info, scanCross(tmp, r, c));
    info = mergeInfo(info, scanCross(tmp, r, c+1));
    info = mergeInfo(info, scanCross(tmp, r+1, c));
    info = mergeInfo(info, scanCross(tmp, r+1, c+1));

    if (info.has3 || info.has4Line || info.has4Col ||
        info.has5Line || info.has5Col || info.hasTL) {
        return true;
    }

    return false;
}
