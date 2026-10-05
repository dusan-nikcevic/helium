#pragma once
#include <QQuickItem>
#include <QColor>
class Waveform : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(int seed READ seed WRITE setSeed NOTIFY seedChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
public:
    explicit Waveform(QQuickItem *parent = nullptr);
    int seed() const { return m_seed; }
    QColor color() const { return m_color; }
    void setSeed(int seed);
    void setColor(const QColor &color);
signals:
    void seedChanged();
    void colorChanged();
protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &next, const QRectF &previous) override;
private:
    int m_seed = 1;
    QColor m_color = QColor("#8bbeb8");
};
