#ifndef GAMEBOARD_H
#define GAMEBOARD_H

#include <array>
#include <QObject>
#include <QVector>
#include <QPoint>

constexpr int ROW = 8;
constexpr int COL = 8;

struct Spot {
    int pic = 0;      // 0..5 颜色，-1 空
    int marked=0;     // 预留
};
using Grid = std::array<std::array<Spot, COL>, ROW>;

class GameBoard : public QObject
{
    Q_OBJECT
public:
    explicit GameBoard(QObject *parent = nullptr);

    Grid &grid()  { return m_grid; }
    void initNoThree(); // 初始化
    bool trySwap(int r1, int c1, int r2, int c2); // UI 调用的交换判断
    bool isDead(const Grid &g); // 死局判断


    // gameboard.h (添加到 public 区域)
    bool tryRotate(int r, int c); // 尝试顺时针旋转以 (r,c) 为左上角的 2x2 区域

    Grid m_grid;

signals:
    void gridUpdated();   // 通知 UI

private:
    struct MatchInfo {
        bool has3 = false;
        bool has4Line = false;
        bool has4Col  = false;
        bool has5Line = false;
        bool has5Col  = false;
        bool hasTL    = false;
        QVector<QPoint> cells;
    };

    void generateNoThreeAlone(Grid &g);

    // 内部使用的辅助函数
    bool hasMatchInCross(const Grid &g, int r, int c);
    bool trySwapInternal(const Grid &g, int r1, int c1, int r2, int c2);

    MatchInfo scanCross(const Grid &g, int r, int c);
    static MatchInfo mergeInfo(const MatchInfo &a, const MatchInfo &b);
};

#endif // GAMEBOARD_H
